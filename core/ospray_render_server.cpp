// ======================================================================== //
// BLOSPRAY - OSPRay as a Blender render engine                             //
// Paul Melis, SURFsara <paul.melis@surfsara.nl>                            //
// Render server                                                            //
// ======================================================================== //
// Copyright 2018-2019 SURFsara                                             //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include <sys/time.h>
#include <sys/stat.h>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>
#include <unistd.h>
#include <dlfcn.h>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#include <ospray/ospray.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>      // to_string()

#include "image.h"
#include "tcpsocket.h"
#include "json.hpp"
#include "blocking_queue.h"
#include "cool2warm.h"
#include "util.h"
#include "plugin.h"
#include "messages.pb.h"

using json = nlohmann::json;

const int   PORT = 5909;

OSPWorld        world;
std::vector<OSPInstance>    scene_instances;
OSPCamera       camera;
OSPRenderer     renderer;
OSPFrameBuffer  framebuffer;
int             framebuffer_width=0, framebuffer_height=0;
bool            framebuffer_created = false;
OSPMaterial     material;                       // XXX hack for now

// In Blender a material is linked to the MESH (object data) by default, so
// to match this we use an OSPRay GeometricModel, which includes the material
// A VolumetricModel includes the transfer function.
typedef std::map<std::string, OSPGeometricModel>    LoadedMeshesMap;
typedef std::map<std::string, OSPVolumetricModel>   LoadedVolumesMap;
typedef std::map<std::string, ModelInstances>       LoadedGeometriesMap;

// XXX rename to ..._cache
LoadedMeshesMap     loaded_meshes;
LoadedVolumesMap    loaded_volumes;
LoadedGeometriesMap loaded_geometries;

ImageSettings   image_settings;
RenderSettings  render_settings;
CameraSettings  camera_settings;
LightSettings   light_settings;

// Plugin registry

typedef std::map<std::string, PluginDefinition> PluginDefinitionsMap;

PluginDefinitionsMap plugin_definitions;

// Geometry buffers

std::vector<float>      vertex_buffer;
std::vector<float>      normal_buffer;
std::vector<float>      vertex_color_buffer;

std::vector<uint32_t>   triangle_buffer;

// Utility

template<typename T>
void
object2world_from_protobuf(glm::mat4 &matrix, T& protobuf)
{
    float *M = glm::value_ptr(matrix);
    
    // Protobuf elements assumed in row-major order 
    // (while GLM uses column-major order)
    M[0] = protobuf.object2world(0);
    M[1] = protobuf.object2world(4);
    M[2] = protobuf.object2world(8);
    M[3] = protobuf.object2world(12);

    M[4] = protobuf.object2world(1);
    M[5] = protobuf.object2world(5);
    M[6] = protobuf.object2world(9);
    M[7] = protobuf.object2world(13);
    
    M[8] = protobuf.object2world(2);
    M[9] = protobuf.object2world(6);
    M[10] = protobuf.object2world(10);
    M[11] = protobuf.object2world(14);
    
    M[12] = protobuf.object2world(3);
    M[13] = protobuf.object2world(7);
    M[14] = protobuf.object2world(11);
    M[15] = protobuf.object2world(15);    
}

// Plugin handling

// If needed, loads plugin shared library and initializes plugin
bool
ensure_plugin_is_loaded(LoadFunctionResult &result, const std::string& name)
{
    if (name == "")
    {
        printf("No plugin name provided!\n");
        return false;
    }
    
    PluginDefinitionsMap::iterator it = plugin_definitions.find(name);
    
    if (it != plugin_definitions.end())
        return true;
    
    // Plugin not loaded yet (or failed to load previous time)
    
    printf("Plugin '%s' not loaded yet\n", name.c_str());
    
    std::string plugin_file = name + ".so";
    
    // Open plugin shared library
    
    printf("Loading plugin %s (%s)\n", name.c_str(), plugin_file.c_str());
    
    void *plugin = dlopen(plugin_file.c_str(), RTLD_LAZY);
    
    if (!plugin) 
    {
        result.set_success(false);
        result.set_message("Failed to open plugin");            

        fprintf(stderr, "dlopen() error: %s\n", dlerror());
        return false;
    }
            
    dlerror();  // Clear previous error
    
    // Initialize plugin 
    
    plugin_initialization_function *initialize = (plugin_initialization_function*) dlsym(plugin, "initialize");
            
    if (initialize == NULL)
    {
        result.set_success(false);
        result.set_message("Failed to get initialization function from plugin!");            
 
        fprintf(stderr, "dlsym() error: %s\n", dlerror());
        
        dlclose(plugin);
        
        return false;
    }
    
    PluginDefinition definition;
    
    if (!initialize(&definition))
    {
        result.set_success(false);
        result.set_message("Plugin failed to initialize!");            
 
        dlclose(plugin);
        
        return false;
    }
        
    plugin_definitions[name] = definition;
    
    printf("Plugin parameters:\n");
    
    PluginParameter *p = definition.parameters;
    while (p->name)
    {
        printf("... [%s] type %d, length %d, flags 0x%02x - %s\n", p->name, p->type, p->length, p->flags, p->description);
        p++;
    }
    
    return true;
}

bool
check_parameters(LoadFunctionResult& result, const PluginParameter *plugin_parameters, const json &actual_parameters)
{
    // We don't return false on the first error, but keep checking for any subsequent errors
    bool ok = true;
    
    for (const PluginParameter *pdef = plugin_parameters; pdef->name; pdef++)
    {
        const char *name = pdef->name;
        const int length = pdef->length;
        const ParameterType type = pdef->type;
        
        // XXX param might be optional in future
        if (actual_parameters.find(name) == actual_parameters.end())
        {
            printf("ERROR: Missing parameter '%s'!\n", name);
            ok = false;
            continue;
        }
        
        const json &value = actual_parameters[name];
        
        if (length > 1)
        {
            // Array value
            if (!value.is_array())
            {
                printf("ERROR: Expected array of length %d for parameter '%s'!\n", length, name);
                ok = false;
                continue;
            }
            
            // XXX check array items
        }
        else 
        {
            // Scalar value
            if (!value.is_primitive())
            {
                printf("ERROR: Expected primitive value for parameter '%s', but found array of length %d!\n", name, value.size());
                ok = false;
                continue;
            }
                
            switch (type)
            {
            case PARAM_INT:
                if (!value.is_number_integer())
                {
                    printf("ERROR: Expected integer value for parameter '%s'!\n", name);
                    ok = false;
                    continue;
                }
                break;
                
            case PARAM_FLOAT:
                if (!value.is_number_float())
                {
                    printf("ERROR: Expected float value for parameter '%s'!\n", name);
                    ok = false;
                    continue;
                }
                break;
                
            //case PARAM_BOOL:
            case PARAM_STRING:
                if (!value.is_string())
                {
                    printf("ERROR: Expected string value for parameter '%s'!\n", name);
                    ok = false;
                    continue;
                }
                
            case PARAM_USER:
                break;
            }
        }
    }
    
    return ok;
}

// Receive methods

bool
receive_and_add_blender_mesh_data(TCPSocket *sock, const SceneElement& element)
{
    printf("%s [MESH]\n", element.name().c_str());
    
    if (loaded_meshes.find(element.name()) != loaded_meshes.end())
        printf("WARNING: mesh '%s' already loaded, overwriting!\n", element.name().c_str());
    
    MeshData    mesh_data;
    OSPData     data;
    
    if (!receive_protobuf(sock, mesh_data))
        return false;
    
    uint32_t nv, nt, flags;
    
    nv = mesh_data.num_vertices();
    nt = mesh_data.num_triangles();
    flags = mesh_data.flags();
    
    printf("...... %d vertices, %d triangles, flags 0x%08x\n", nv, nt, flags);
    
    vertex_buffer.reserve(nv*3);    
    if (sock->recvall(&vertex_buffer[0], nv*3*sizeof(float)) == -1)
        return false;
    
    if (flags & MeshData::NORMALS)
    {
        printf("...... Mesh has normals\n");
        normal_buffer.reserve(nv*3);
        if (sock->recvall(&normal_buffer[0], nv*3*sizeof(float)) == -1)
            return false;        
    }
        
    if (flags & MeshData::VERTEX_COLORS)
    {
        printf("...... Mesh has vertex colors\n");
        vertex_color_buffer.reserve(nv*4);
        if (sock->recvall(&vertex_color_buffer[0], nv*4*sizeof(float)) == -1)
            return false;        
    }
    
    triangle_buffer.reserve(nt*3);
    if (sock->recvall(&triangle_buffer[0], nt*3*sizeof(uint32_t)) == -1)
        return false;
    
    OSPGeometry mesh = ospNewGeometry("triangles");
  
        data = ospNewData(nv, OSP_VEC3F, &vertex_buffer[0]);   
        ospCommit(data);
        ospSetData(mesh, "vertex", data);
        ospRelease(data);

        if (flags & MeshData::NORMALS)
        {
            data = ospNewData(nv, OSP_VEC3F, &normal_buffer[0]);
            ospCommit(data);
            ospSetData(mesh, "vertex.normal", data);
            ospRelease(data);
        }

        if (flags & MeshData::VERTEX_COLORS)
        {
            data = ospNewData(nv, OSP_VEC4F, &vertex_color_buffer[0]);
            ospCommit(data);
            ospSetData(mesh, "vertex.color", data);
            ospRelease(data);
        }
        
        data = ospNewData(nt, OSP_VEC3I, &triangle_buffer[0]);            
        ospCommit(data);
        ospSetData(mesh, "index", data);
        ospRelease(data);

        // XXX set Blender material?
    
    ospCommit(mesh);
    
    OSPGeometricModel model = ospNewGeometricModel(mesh);
        //ospSetObject(model, "material", material);
    ospCommit(model);
    ospRelease(mesh);
        
    loaded_meshes[element.name()] = model;
    
    return true;
}

bool
receive_and_add_blender_mesh_object(TCPSocket *sock, const SceneElement& element)
{
    printf("%s [OBJECT]\n", element.name().c_str());
    printf("........ --> %s (blender mesh)\n", element.data_link().c_str());
    
    LoadedMeshesMap::iterator it = loaded_meshes.find(element.data_link());
    
    if (it == loaded_meshes.end())
    {
        printf("WARNING: linked mesh data '%s' not found!\n", element.data_link().c_str());
        return false;
    }
    
    OSPGeometricModel model = it->second;
    
    glm::mat4   obj2world;
    float       affine_xform[12];
    
    object2world_from_protobuf(obj2world, element);
    affine3fv_from_mat4(affine_xform, obj2world);
    
    OSPGroup group = ospNewGroup();
    
        OSPData data = ospNewData(1, OSP_OBJECT, &model, 0);
        ospSetData(group, "geometry", data);
        ospRelease(model);
    
    ospCommit(group);
    ospRelease(data);
    
    OSPInstance instance = ospNewInstance(group);
        ospSetAffine3fv(instance, "xfm", affine_xform);
    ospCommit(instance);
    ospRelease(group);
    
    scene_instances.push_back(instance);
    
    return true;
}

bool
receive_and_add_ospray_volume_data(TCPSocket *sock, const SceneElement& element)
{
    // Object-to-world matrix (copy of the parent object)
    
    glm::mat4 obj2world;
    object2world_from_protobuf(obj2world, element);

    // From Blender custom properties
    
    const char *encoded_properties = element.properties().c_str();
    //printf("Received volume properties:\n%s\n", encoded_properties);
    
    const json &properties = json::parse(encoded_properties);
    
    // XXX we print the mesh_name here, but that mesh is replaced by the python
    // export after it receives the volume extents. so a bit confusing as that original
    // mesh is reported, but that isn't in the scene anymore
    printf("%s [VOLUME]\n", element.name().c_str());
    printf("Properties:\n");
    printf("%s\n", properties.dump(4).c_str());
    
    // Prepare result
    
    LoadFunctionResult result;
    
    result.set_success(true);
    
    // Find load function 
                
    const std::string& plugin = properties["plugin"];
    
    if (!ensure_plugin_is_loaded(result, plugin))
    {
        // Something went wrong...
        send_protobuf(sock, result);
        return false;
    }
    
    PluginDefinition& definition = plugin_definitions[plugin];
    volume_load_function_t load_function = definition.functions.volume_load_function;
    
    // Check parameters passed to load function
    
    const json& plugin_parameters = properties["plugin_parameters"];
    
    if (!check_parameters(result, definition.parameters, plugin_parameters))
    {
        // Something went wrong...
        send_protobuf(sock, result);
        return false;
    }

    // Call load function
    
    struct timeval t0, t1;
    
    printf("Calling load function\n");
    gettimeofday(&t0, NULL);
    
    OSPVolumetricModel  volume_model;
    float               bbox[6];
    float               data_range[2];
    
    volume_model = load_function(bbox, data_range, result, plugin_parameters, obj2world);
    
    gettimeofday(&t1, NULL);
    printf("Load function executed in %.3fs\n", time_diff(t0, t1));
    
    if (volume_model == NULL)
    {
        send_protobuf(sock, result);

        printf("ERROR: volume load function failed!\n");
        return false;
    }    
    
    // Load function succeeded
    
    // XXX disable for now
    //result.set_hash(get_sha1(encoded_parameters));
    result.set_hash("12345");
    
    for (int i = 0; i < 6; i++)
        result.add_bbox(bbox[i]);
    
    // Set up further volume properties
    
    if (properties.find("sampling_rate") != properties.end())
        ospSetFloat(volume_model,  "samplingRate", properties["sampling_rate"].get<float>());
    else
        ospSetFloat(volume_model,  "samplingRate", 0.1f);
    
    if (properties.find("gradient_shading") != properties.end())
        ospSetBool(volume_model,  "gradientShadingEnabled", properties["gradient_shading"].get<bool>());
    else
        ospSetBool(volume_model,  "gradientShadingEnabled", false);
    
    if (properties.find("pre_integration") != properties.end())
        ospSetBool(volume_model,  "preIntegration", properties["pre_integration"].get<bool>());
    else
        ospSetBool(volume_model,  "preIntegration", false);    
    
    if (properties.find("single_shade") != properties.end())
        ospSetBool(volume_model,  "singleShade", properties["single_shade"].get<bool>());
    else
        ospSetBool(volume_model,  "singleShade", true);  
    
    ospSetBool(volume_model, "adaptiveSampling", false);
    
    // Transfer function
    
    float tf_colors[3*cool2warm_entries];
    float tf_opacities[cool2warm_entries];

    for (int i = 0; i < cool2warm_entries; i++)
    {
        tf_opacities[i]  = cool2warm[4*i+0];
        tf_colors[3*i+0] = cool2warm[4*i+1];
        tf_colors[3*i+1] = cool2warm[4*i+2];
        tf_colors[3*i+2] = cool2warm[4*i+3];
    }

    OSPTransferFunction tf = ospNewTransferFunction("piecewise_linear");
    
        if (properties.find("data_range") != properties.end())
        {
            // Override data range provided by the plugin
            float minval = properties["data_range"][0];
            float maxval = properties["data_range"][1];
            ospSetVec2f(tf, "valueRange", minval, maxval);
        }
        else
            ospSetVec2f(tf, "valueRange", data_range[0], data_range[1]);
        
        OSPData color_data = ospNewData(cool2warm_entries, OSP_VEC3F, tf_colors);
        ospSetData(tf, "colors", color_data);
        ospRelease(color_data);
        
        OSPData opacity_data = ospNewData(cool2warm_entries, OSP_FLOAT, tf_opacities);
        ospSetData(tf, "opacities", opacity_data);
        ospRelease(opacity_data);
    
    ospCommit(tf);
    
    ospSetObject(volume_model, "transferFunction", tf);
    ospRelease(tf);
    
    ospCommit(volume_model);

    // Cache loaded volume object
    
    loaded_volumes[element.name()] = volume_model;

    send_protobuf(sock, result);
    
    return true;
}
    

bool
receive_and_add_ospray_volume_object(TCPSocket *sock, const SceneElement& element)
{
    // Object-to-world matrix
    
    glm::mat4   obj2world;
    float       affine_xform[12];
    
    object2world_from_protobuf(obj2world, element);
    affine3fv_from_mat4(affine_xform, obj2world);

    // From Blender custom properties
    
    const char *encoded_properties = element.properties().c_str();
    //printf("Received volume properties:\n%s\n", encoded_properties);
    
    const json &properties = json::parse(encoded_properties);
    
    printf("%s [OBJECT]\n", element.name().c_str());
    printf("........ --> %s (ospray volume)\n", element.data_link().c_str());
    printf("Properties:\n");
    printf("%s\n", properties.dump(4).c_str());
    
    LoadedVolumesMap::iterator it = loaded_volumes.find(element.data_link());
    
    if (it == loaded_volumes.end())
    {
        printf("WARNING: linked volume data '%s' not found!\n", element.data_link().c_str());
        return false;
    }
    
    OSPVolumetricModel volume_model = it->second;
    
    OSPGroup group = ospNewGroup();
    
        OSPData data = ospNewData(1, OSP_OBJECT, &volume_model, 0);
        ospSetData(group, "volume", data);
        ospRelease(volume_model);
        
    ospCommit(group);    
    
    OSPInstance instance = ospNewInstance(group);
        ospSetAffine3fv(instance, "xfm", affine_xform);
    ospCommit(instance);
    ospRelease(group);
    
#if 0
    // See https://github.com/ospray/ospray/pull/165, support for volume transformations was reverted
    ospSetVec3f(volume, "xfm.l.vx", osp::vec3f{ obj2world[0], obj2world[4], obj2world[8] });
    ospSetVec3f(volume, "xfm.l.vy", osp::vec3f{ obj2world[1], obj2world[5], obj2world[9] });
    ospSetVec3f(volume, "xfm.l.vz", osp::vec3f{ obj2world[2], obj2world[6], obj2world[10] });
    ospSetVec3f(volume, "xfm.p", osp::vec3f{ obj2world[3], obj2world[7], obj2world[11] });
#endif

    // XXX need to use the representation property set 
    
    if (properties.find("isovalues") != properties.end())
    {
        // Isosurfacing
        
        printf("Property 'isovalues' set, representing volume with isosurface(s)\n");
        
        json isovalues_prop = properties["isovalues"];
        int n = isovalues_prop.size();

        float *isovalues = new float[n];
        for (int i = 0; i < n; i++)
            isovalues[i] = isovalues_prop[i];
        
        OSPData isovaluesData = ospNewData(n, OSP_FLOAT, isovalues);
        ospCommit(isovaluesData);
        delete [] isovalues;
        
        OSPGeometry isosurface = ospNewGeometry("isosurfaces");
        
            ospSetObject(isosurface, "volume", volume_model);       // XXX need volume here, not the volume model!
            ospRelease(volume_model);

            ospSetData(isosurface, "isovalues", isovaluesData);
            ospRelease(isovaluesData);
            
        ospCommit(isosurface);
        
        OSPGeometricModel model = ospNewGeometricModel(isosurface);
        ospCommit(model);
        ospRelease(isosurface);
        
        OSPData data = ospNewData(1, OSP_OBJECT, &model, 0);
        ospCommit(data);
        ospRelease(model);
        
        ospSetData(instance, "geometries", data);
        ospCommit(instance);
        ospRelease(data);
    }
    else if (properties.find("slice_plane") != properties.end())
    {
        // Slice plane (only a single one supported, atm)
        
        printf("Property 'slice_plane' set, representing volume with slice plane\n");
        
        json slice_plane_prop = properties["slice_plane"];
        
        if (slice_plane_prop.size() == 4)
        {
            float plane[4];
            for (int i = 0; i < 4; i++)
                plane[i] = slice_plane_prop[i];
            printf("plane: %.3f, %3f, %.3f, %.3f\n", plane[0], plane[1], plane[2], plane[3]);
            
            OSPData planeData = ospNewData(1, OSP_VEC4F, plane);
            ospCommit(planeData);
            
            OSPGeometry slices = ospNewGeometry("slices");
            
                ospSetObject(slices, "volume", volume_model);   // XXX need volume here, not the volume model!  
                ospRelease(volume_model);

                ospSetData(slices, "planes", planeData);
                ospRelease(planeData);
                
            ospCommit(slices);
                
            OSPGeometricModel model = ospNewGeometricModel(slices);
            ospCommit(model);
            ospRelease(slices);
            
            OSPData data = ospNewData(1, OSP_OBJECT, &model, 0);
            ospCommit(data);
            ospRelease(model);
            
            ospSetData(instance, "geometries", data);
            ospCommit(instance);
            ospRelease(data);
        }
        else
        {
            fprintf(stderr, "ERROR: slice_plane attribute should contain list of 4 floats values!\n");
        }
    }
    else
    {
        // Represent as volume 
        OSPData data = ospNewData(1, OSP_OBJECT, &volume_model, 0);
        ospCommit(data);
        ospRelease(volume_model);
        
        // XXX need group in between
        ospSetData(instance, "volumes", data);
        ospCommit(instance);
        ospRelease(data);
    }
    
    return true;
}


bool
receive_and_add_ospray_geometry_data(TCPSocket *sock, const SceneElement& element)
{
    // Object-to-world matrix (copy of the parent object)
    
    glm::mat4 obj2world;
    object2world_from_protobuf(obj2world, element);

    // Custom parameters
    
    const char *encoded_properties = element.properties().c_str();
    //printf("Received geeomtry properties:\n%s\n", encoded_properties);
    
    const json &properties = json::parse(encoded_properties);
    
    printf("%s [GEOMETRY] (ospray geometry)\n", element.name().c_str());
    printf("Properties:\n");
    printf("%s\n", properties.dump(4).c_str());
    
    // Prepare result
    
    LoadFunctionResult result;
    
    result.set_success(true);
    
    // Find load function 
    
    const std::string& plugin = properties["plugin"];
    
    if (!ensure_plugin_is_loaded(result, plugin))
    {
        // Something went wrong...
        send_protobuf(sock, result);
        return false;
    }
    
    PluginDefinition& definition = plugin_definitions[plugin];
    geometry_load_function_t load_function = definition.functions.geometry_load_function;
    
    // Check parameters
    
    const json& plugin_parameters = properties["plugin_parameters"];
    
    if (!check_parameters(result, definition.parameters, plugin_parameters))
    {
        // Something went wrong...
        send_protobuf(sock, result);
        return false;
    }

    // Let load function do its job
    
    struct timeval t0, t1;
    
    printf("Calling load function\n");
    gettimeofday(&t0, NULL);
    
    float                   bbox[6];
    ModelInstances          model_instances;
    
    load_function(model_instances, bbox, result, plugin_parameters, obj2world);
    
    gettimeofday(&t1, NULL);
    printf("Load function executed in %.3fs\n", time_diff(t0, t1));
    
    if (model_instances.size() == 0)
        printf("WARNING: geometry load function returned no instances\n");
    
    // Load function succeeded
    
    // XXX disable for now
    //result.set_hash(get_sha1(encoded_parameters));
    result.set_hash("12345");
    
    for (int i = 0; i < 6; i++)
        result.add_bbox(bbox[i]);

    // Cache loaded geometry object
    
    loaded_geometries[element.name()] = model_instances;
    
    send_protobuf(sock, result);
    
    return true;
}

bool
receive_and_add_ospray_geometry_object(TCPSocket *sock, const SceneElement& element)
{
    // Object-to-world matrix
    
    glm::mat4 obj2world;
    object2world_from_protobuf(obj2world, element);    

    // From Blender custom properties
    
    const char *encoded_properties = element.properties().c_str();
    //printf("Received geometry object properties:\n%s\n", encoded_properties);
    
    const json &properties = json::parse(encoded_properties);
    
    printf("%s [OBJECT]\n", element.name().c_str());
    printf("........ --> %s (ospray geometry)\n", element.data_link().c_str());
    printf("Properties:\n");
    printf("%s\n", properties.dump(4).c_str());
    
    LoadedGeometriesMap::iterator it = loaded_geometries.find(element.data_link());
    
    if (it == loaded_geometries.end())
    {
        printf("WARNING: linked geometry data '%s' not found!\n", element.data_link().c_str());
        return false;
    }
    
    // Create instance(s)    
    
    ModelInstances  model_instances = it->second;
    glm::mat4       xform; 
    
    for (ModelInstances::const_iterator it = model_instances.begin(); it != model_instances.end(); ++it)
    {
        const OSPGeometricModel& model = it->first;
        const glm::mat4& instance_xform = it->second;
        
        xform = obj2world * instance_xform;
        //printf("xform = %s\n", glm::to_string(xform).c_str());
        
        float affine[13];
        affine3fv_from_mat4(affine, xform);
        
        OSPGroup group = ospNewGroup();
        
            OSPData models = ospNewData(1, OSP_OBJECT, &model, 0);
            ospSetData(group, "geometry", models);
            ospRelease(models);
        
        ospCommit(group);

        OSPInstance instance = ospNewInstance(group);
        
            ospSetAffine3fv(instance, "xfm", affine);
        
        ospCommit(instance);
        ospRelease(group);

        scene_instances.push_back(instance);
    }
    
    return true;
}

// XXX currently has big memory leak as we never release the new objects ;-)
bool
receive_scene(TCPSocket *sock)
{
    // Image settings
    
    receive_protobuf(sock, image_settings);
    
    if (framebuffer_width != image_settings.width() || framebuffer_height != image_settings.height())
    {
        framebuffer_width = image_settings.width() ;
        framebuffer_height = image_settings.height();
        
        if (framebuffer_created)
            ospRelease(framebuffer);
        
        printf("Initializing framebuffer of %dx%d pixels\n", framebuffer_width, framebuffer_height);
        
        framebuffer = ospNewFrameBuffer(framebuffer_width, framebuffer_height, OSP_FB_RGBA32F, OSP_FB_COLOR | /*OSP_FB_DEPTH |*/ OSP_FB_ACCUM);         
        ospResetAccumulation(framebuffer);        
        
        framebuffer_created = true;
    }
    
    // Render settings
    
    receive_protobuf(sock, render_settings);
    
    // XXX hmm, we create a new renderer on each new scene
    renderer = ospNewRenderer(render_settings.renderer().c_str());
    
    ospSetVec4f(renderer, "bgColor", 
        render_settings.background_color(0),
        render_settings.background_color(1),
        render_settings.background_color(2),
        render_settings.background_color(3));
    
    ospSetInt(renderer, "aoSamples", render_settings.ao_samples());
    ospSetBool(renderer, "shadowsEnabled", render_settings.shadows_enabled());
    //ospSetInt(renderer, "spp", 1);

    // Update camera
    
    receive_protobuf(sock, camera_settings);
    
    printf("%s [OBJECT] (camera)\n", camera_settings.object_name().c_str());
    printf("........ --> %s (camera)\n", camera_settings.camera_name().c_str());
    
    float cam_pos[3], cam_viewdir[3], cam_updir[3];
    
    cam_pos[0] = camera_settings.position(0);
    cam_pos[1] = camera_settings.position(1);
    cam_pos[2] = camera_settings.position(2);
    
    cam_viewdir[0] = camera_settings.view_dir(0); 
    cam_viewdir[1] = camera_settings.view_dir(1); 
    cam_viewdir[2] = camera_settings.view_dir(2); 

    cam_updir[0] = camera_settings.up_dir(0);
    cam_updir[1] = camera_settings.up_dir(1);
    cam_updir[2] = camera_settings.up_dir(2);
    
    switch (camera_settings.type())
    {
        case CameraSettings::PERSPECTIVE:
            camera = ospNewCamera("perspective");
            ospSetFloat(camera, "fovy",  camera_settings.fov_y());
            break;
        
        case CameraSettings::ORTHOGRAPHIC:
            camera = ospNewCamera("orthographic");
            ospSetFloat(camera, "height", camera_settings.height());
            break;
            
        case CameraSettings::PANORAMIC:
            camera = ospNewCamera("panoramic");
            break;
        
        default:
            fprintf(stderr, "WARNING: unknown camera type %d\n", camera_settings.type());
            break;
    }

    ospSetFloat(camera, "aspect", camera_settings.aspect());
    ospSetFloat(camera, "nearClip", camera_settings.clip_start());     
    
    ospSetVec3fv(camera, "pos", cam_pos);
    ospSetVec3fv(camera, "dir", cam_viewdir);
    ospSetVec3fv(camera, "up",  cam_updir);
    
    if (camera_settings.dof_focus_distance() > 0.0f)
    {
        // XXX seem to stuck in loop during rendering when distance is 0
        ospSetFloat(camera, "focusDistance", camera_settings.dof_focus_distance());
        ospSetFloat(camera, "apertureRadius", camera_settings.dof_aperture());
    }
    
    if (image_settings.border_size() == 4)
    {
        ospSetVec2f(camera, "imageStart", image_settings.border(0), image_settings.border(1));
        ospSetVec2f(camera, "imageEnd", image_settings.border(2), image_settings.border(3));
    }
    
    ospCommit(camera); 
    
    // Lights
    
    receive_protobuf(sock, light_settings);
    
    const int num_lights = light_settings.lights_size();
    OSPLight *osp_lights = new OSPLight[num_lights+1];      // Scene lights + ambient light
    OSPLight osp_light;
    
    for (int i = 0; i < num_lights; i++)
    {
        const Light& light = light_settings.lights(i);
        
        printf("%s [OBJECT] (object)\n", light.object_name().c_str());
        printf("........ --> %s (blender light)\n", light.light_name().c_str());
        
        if (light.type() == Light::POINT)
        {
            osp_light = osp_lights[i] = ospNewLight("point");
        }
        else if (light.type() == Light::SPOT)
        {
            osp_light = osp_lights[i] = ospNewLight("spot");
            ospSetFloat(osp_light, "openingAngle", light.opening_angle());
            ospSetFloat(osp_light, "penumbraAngle", light.penumbra_angle());
        }
        else if (light.type() == Light::SUN)
        {
            osp_light = osp_lights[i] = ospNewLight("directional");
            ospSetFloat(osp_light, "angularDiameter", light.angular_diameter());
        }
        else if (light.type() == Light::AREA)            
        {
            // XXX blender's area light is more general than ospray's quad light
            osp_light = osp_lights[i] = ospNewLight("quad");
            ospSetVec3f(osp_light, "edge1", light.edge1(0), light.edge1(1), light.edge1(2));
            ospSetVec3f(osp_light, "edge2", light.edge2(0), light.edge2(1), light.edge2(2));
        }
        //else
        // XXX HDRI
        
        printf("........ intensity %.3f, visible %d\n", light.intensity(), light.visible());      
        
        ospSetVec3f(osp_light, "color", light.color(0), light.color(1), light.color(2));
        ospSetFloat(osp_light, "intensity", light.intensity());    
        ospSetBool(osp_light, "isVisible", light.visible());                      
        
        if (light.type() != Light::SUN)
            ospSetVec3f(osp_light, "position", light.position(0), light.position(1), light.position(2));
        
        if (light.type() == Light::SUN || light.type() == Light::SPOT)
            ospSetVec3f(osp_light, "direction", light.direction(0), light.direction(1), light.direction(2));
        
        if (light.type() == Light::POINT || light.type() == Light::SPOT)
            ospSetFloat(osp_light, "radius", light.radius());
        
        ospCommit(osp_light);      
    }
    
    // Ambient
    printf("Ambient light\n");
    printf("........ intensity %.3f\n", light_settings.ambient_intensity());      
    
    osp_light = osp_lights[num_lights] = ospNewLight("ambient");
    ospSetFloat(osp_light, "intensity", light_settings.ambient_intensity());
    ospSetVec3f(osp_light, "color", 
        light_settings.ambient_color(0), light_settings.ambient_color(1), light_settings.ambient_color(2));
    
    ospCommit(osp_light);
    
    OSPData light_data = ospNewData(num_lights+1, OSP_LIGHT, osp_lights, 0);  
    ospCommit(light_data);
    //delete [] osp_lights;
    
    ospSetObject(renderer, "lights", light_data);
    
    ospCommit(renderer);
    
    // For now a single material
    
    /*
    material = ospNewMaterial(render_settings.renderer().c_str(), "OBJMaterial");
        ospSetVec3f(material, "Kd", 0.8f, 0.8f, 0.8f);
    ospCommit(material);
    */
    
    // Receive scene elements
    
    SceneElement element;
    
    // XXX check return value
    receive_protobuf(sock, element);
    
    // XXX use function table
    while (element.type() != SceneElement::NONE)
    {
        if (element.type() == SceneElement::BLENDER_MESH_DATA)
        {
            receive_and_add_blender_mesh_data(sock, element);
        }
        else if (element.type() == SceneElement::BLENDER_MESH_OBJECT)
        {
            receive_and_add_blender_mesh_object(sock, element);
        }
        else if (element.type() == SceneElement::OSPRAY_VOLUME_DATA)
        {
            receive_and_add_ospray_volume_data(sock, element);
        }
        else if (element.type() == SceneElement::OSPRAY_VOLUME_OBJECT)
        {
            receive_and_add_ospray_volume_object(sock, element);
        }
        else if (element.type() == SceneElement::OSPRAY_GEOMETRY_DATA)
        {
            receive_and_add_ospray_geometry_data(sock, element);
        }
        else if (element.type() == SceneElement::OSPRAY_GEOMETRY_OBJECT)
        {
            receive_and_add_ospray_geometry_object(sock, element);
        }
        // else XXX
        
        // Get next element
        // XXX check return value
        receive_protobuf(sock, element);
    }
    
    // Setup world and scene objects
    
    OSPData instances = ospNewData(scene_instances.size(), OSP_OBJECT, &scene_instances[0], 0);
    ospCommit(instances);
    scene_instances.clear();
    
    world = ospNewWorld();
        // See https://github.com/ospray/ospray/issues/277
        ospSetBool(world, "compactMode", true);
        ospSetData(world, "instances", instances);
    ospCommit(world);
    ospRelease(instances);
    
    // Done!
    
    return true;
}

// Send result

size_t
write_framebuffer_exr(const char *fname)
{
    // Access framebuffer 
    const float *fb = (float*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
    
    writeEXRFramebuffer(fname, framebuffer_width, framebuffer_height, fb);
    
    // Unmap framebuffer
    ospUnmapFrameBuffer(fb, framebuffer);    
    
    struct stat st;
    stat(fname, &st);
    
    return st.st_size;
}

/*
// Not used atm, as we use sendfile() 
void 
send_framebuffer(TCPSocket *sock)
{
    uint32_t bufsize;
    
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    
    // Access framebuffer 
    const float *fb = (float*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
    
#if 1
    // Write to OpenEXR file and send *its* contents
    const char *FBFILE = "/dev/shm/orsfb.exr";
    
    // XXX this also maps/unmaps the framebuffer!
    size_t size = write_framebuffer_exr(FBFILE);
    
    printf("Sending framebuffer as OpenEXR file, %d bytes\n", size);
    
    bufsize = size;
    sock->send((uint8_t*)&bufsize, 4);
    sock->sendfile(FBFILE);
#else
    // Send directly
    bufsize = framebuffer_width*framebuffer_height*4*4;
    
    printf("Sending %d bytes of framebuffer data\n", bufsize);
    
    sock->send(&bufsize, 4);
    sock->sendall((uint8_t*)fb, framebuffer_width*framebuffer_height*4*4);
#endif
    
    // XXX can already unmap after written to file
    // Unmap framebuffer
    ospUnmapFrameBuffer(fb, framebuffer);    
    
    gettimeofday(&t1, NULL);
    printf("Sent framebuffer in %.3f seconds\n", time_diff(t0, t1));
}
*/

// Rendering

void
render_thread_func(BlockingQueue<ClientMessage>& render_input_queue,
    BlockingQueue<RenderResult>& render_result_queue)
{
    struct timeval t0, t1, t2;
    size_t  framebuffer_file_size;
    char fname[1024];
    
    gettimeofday(&t0, NULL);
    
    // Clear framebuffer    
    // XXX no 2.0 equivalent?
    //ospFrameBufferClear(framebuffer, OSP_FB_COLOR | OSP_FB_ACCUM);
    ospResetAccumulation(framebuffer);

    for (int i = 1; i <= render_settings.samples(); i++)
    {
        printf("Rendering sample %d ... ", i);
        fflush(stdout);
        
        gettimeofday(&t1, NULL);

        // XXX OSP_FB_COLOR | OSP_FB_ACCUM
        ospRenderFrame(framebuffer, renderer, camera, world);

        gettimeofday(&t2, NULL);
        printf("frame in %.3f seconds\n", time_diff(t1, t2));
        
        // Save framebuffer to file
        sprintf(fname, "/dev/shm/blosprayfb%04d.exr", i);
        
        framebuffer_file_size = write_framebuffer_exr(fname);
        // XXX check res
        
        // Signal a new frame is available
        RenderResult rs;
        rs.set_type(RenderResult::FRAME);
        rs.set_sample(i);
        rs.set_file_name(fname);
        rs.set_file_size(framebuffer_file_size);
        rs.set_memory_usage(memory_usage());
        
        render_result_queue.push(rs);
        
        // XXX handle cancel input
        
        if (render_input_queue.size() > 0)
        {
            ClientMessage cm = render_input_queue.pop();
            
            if (cm.type() == ClientMessage::CANCEL_RENDERING)
            {
                printf("{render thread} Canceling rendering\n");
                            
                RenderResult rs;
                rs.set_type(RenderResult::CANCELED);
                render_result_queue.push(rs);
                
                return;
            }
        }
    }
    
    RenderResult rs;
    rs.set_type(RenderResult::DONE);
    render_result_queue.push(rs);
    
    gettimeofday(&t2, NULL);
    printf("Rendering done in %.3f seconds\n", time_diff(t0, t2));
}

// Querying

bool
handle_volume_extent_query(TCPSocket *sock)
{
    VolumeExtentRequest volume_extent_request;
    VolumeExtentFunctionResult result;
    
    receive_protobuf(sock, volume_extent_request);
    printf("%s\n", volume_extent_request.DebugString().c_str());
    
    result.set_success(true);
    
    send_protobuf(sock, result);
    
    
    return true;
}

// Connection handling

bool
handle_connection(TCPSocket *sock)
{   
    BlockingQueue<ClientMessage> render_input_queue;
    BlockingQueue<RenderResult> render_result_queue;
    
    ClientMessage       client_message;
    RenderResult        render_result;
    RenderResult::Type  rr_type;
    
    std::thread         render_thread;
    bool                rendering = false;
    
    while (true)
    {
        // Check for new client message
        
        if (sock->is_readable())
        {
            if (!receive_protobuf(sock, client_message))
            {
                // XXX if we were rendering, handle the chaos
                
                fprintf(stderr, "Failed to receive client message (%d), goodbye!\n", sock->get_errno());
                sock->close();
                return false;
            }
            
            printf("Got client message of type %s\n", ClientMessage_Type_Name(client_message.type()).c_str());
            printf("%s\n", client_message.DebugString().c_str());
            
            switch (client_message.type())
            {
                // GET_CACHE_ENTRIES    (volumes and meshes)
                // GET_VOLUME_EXTENT    (volume plugin)
                // GET_GEOMETRY_EXTENT  (geometry plugin)
                
                case ClientMessage::UPDATE_SCENE:
                    // XXX handle clear_scene 
                    // XXX check res
                    // XXX ignore if rendering
                    receive_scene(sock);
                    break;
                
                case ClientMessage::QUERY_VOLUME_EXTENT:
                    handle_volume_extent_query(sock);
                
                    return true;
            
                case ClientMessage::START_RENDERING:
                    
                    if (rendering)
                    {
                        // Ignore
                        break;
                    }
                    
                    //render_input_queue.clear();
                    //render_result_queue.clear();        // XXX handle any remaining results
                
                    // Start render thread
                    render_thread = std::thread(&render_thread_func, std::ref(render_input_queue), std::ref(render_result_queue));
                    
                    rendering = true;
                    break;
                    
                case ClientMessage::CANCEL_RENDERING:
                    
                    printf("Got request to CANCEL rendering\n");
                    
                    if (!rendering)
                        break;
                    
                    render_input_queue.push(client_message);
                    
                    break;
                    
                case ClientMessage::QUIT:
                    // XXX if we were still rendering, handle the chaos
                
                    printf("Got QUIT message\n");
                
                    sock->close();
                
                    return true;
            }
        }
        
        // Check for new render results
        
        if (rendering && render_result_queue.size() > 0)
        {
            render_result = render_result_queue.pop();
            
            // Forward render results on socket
            send_protobuf(sock, render_result);
            
            switch (render_result.type())
            {
                case RenderResult::FRAME:
                    // New framebuffer (for a single sample) available, send
                    // it to the client
                
                    printf("Frame available, sample %d (%s, %d bytes)\n", render_result.sample(), render_result.file_name().c_str(), render_result.file_size());
                
                    sock->sendfile(render_result.file_name().c_str());
                
                    // Remove local framebuffer file
                    unlink(render_result.file_name().c_str());
                
                    break;
                
                case RenderResult::CANCELED:
                    printf("Rendering canceled!\n");
                
                    // Thread should have finished by now
                    render_thread.join();
                
                    rendering = false;
                    break;
                
                case RenderResult::DONE:
                    printf("Rendering done!\n");
                
                    // Thread should have finished by now
                    render_thread.join();
                
                    rendering = false;
                    break;
            }
        }
        
        usleep(1000);
    }
    
    sock->close();
    
    return true;
}

// Main

int 
main(int argc, const char **argv) 
{
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    
    // Initialize OSPRay.
    // OSPRay parses (and removes) its commandline parameters, e.g. "--osp:debug"
    ospInit(&argc, argv);
    
    // Server loop
    
    TCPSocket *listen_sock;
    
    listen_sock = new TCPSocket;
    listen_sock->bind(PORT);
    listen_sock->listen(1);
    
    printf("Listening on port %d\n", PORT);    
    
    TCPSocket *sock;
    
    while (true)
    {            
        printf("Waiting for new connection...\n");    
        
        sock = listen_sock->accept();
        
        printf("Got new connection\n");
        
        if (!handle_connection(sock))
            printf("Error handling connection!\n");
        else
            printf("Connection successfully handled\n");
    }

    return 0;
}
