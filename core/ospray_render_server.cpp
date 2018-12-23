/*
Ospray render server for blospray

Paul Melis, SURFsara <paul.melis@surfsara.nl>
Copyright (C) 2017-2018

This source file started out as a copy of ospTutorial.c (although not
much is left of it), as distributed in the OSPRay source code. 
Hence the original copyright message below.
*/

// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
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

#include <boost/program_options.hpp>
#include <ospray/ospray.h>

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

// image size
osp::vec2i  image_size;

OSPModel        world;
OSPCamera       camera;
OSPRenderer     renderer;
OSPFrameBuffer  framebuffer;
bool            framebuffer_created = false;
OSPMaterial     material;   // XXX hack for now

typedef std::map<std::string, OSPModel>     LoadedMeshesMap;
typedef std::map<std::string, OSPVolume>    LoadedVolumesMap;

LoadedMeshesMap     loaded_meshes;
LoadedVolumesMap    loaded_volumes;

ImageSettings   image_settings;
RenderSettings  render_settings;
CameraSettings  camera_settings;
LightSettings   light_settings;

// Volume loaders

typedef std::map<std::string, volume_load_function_t>   VolumeLoadFunctionMap;
VolumeLoadFunctionMap                                   volume_load_functions;

// Geometry buffers

std::vector<float>      vertex_buffer;
std::vector<float>      normal_buffer;
std::vector<float>      vertex_color_buffer;

std::vector<uint32_t>   triangle_buffer;

// Utility

void
affine3f_from_matrix(osp::affine3f& xform, float *m)
{
    xform.l.vx = osp::vec3f{ m[0], m[4], m[8] };
    xform.l.vy = osp::vec3f{ m[1], m[5], m[9] };
    xform.l.vz = osp::vec3f{ m[2], m[6], m[10] };
    xform.p = osp::vec3f{ m[3], m[7], m[11] };
}

template<typename T>
void
object2world_from_protobuf(float *matrix, T& protobuf)
{
    for (int i = 0; i < 16; i++)
        matrix[i] = protobuf.object2world(i);   
}

// Receive methods

bool
receive_and_add_mesh_data(TCPSocket *sock, const SceneElement& element)
{
    printf("[MESH] %s\n", element.name().c_str());
    
    if (loaded_meshes.find(element.name()) != loaded_meshes.end())
        printf("WARNING: mesh '%s' already loaded!\n", element.name().c_str());
    
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
  
        data = ospNewData(nv, OSP_FLOAT3, &vertex_buffer[0]);   
        ospCommit(data);
        ospSetData(mesh, "vertex", data);
        ospRelease(data);

        if (flags & MeshData::NORMALS)
        {
            data = ospNewData(nv, OSP_FLOAT3, &normal_buffer[0]);
            ospCommit(data);
            ospSetData(mesh, "vertex.normal", data);
            ospRelease(data);
        }

        if (flags & MeshData::VERTEX_COLORS)
        {
            data = ospNewData(nv, OSP_FLOAT4, &vertex_color_buffer[0]);
            ospCommit(data);
            ospSetData(mesh, "vertex.color", data);
            ospRelease(data);
        }
        
        data = ospNewData(nt, OSP_INT3, &triangle_buffer[0]);            
        ospCommit(data);
        ospSetData(mesh, "index", data);
        ospRelease(data);

        // XXX for now
        ospSetMaterial(mesh, material);
    
    ospCommit(mesh);
    
    OSPModel model = ospNewModel();
    ospAddGeometry(model, mesh);
    ospRelease(mesh);
        
    loaded_meshes[element.name()] = model;
    
    return true;
}

bool
receive_and_add_mesh_object(TCPSocket *sock, const SceneElement& element)
{
    printf("[OBJECT] %s (mesh)\n", element.name().c_str());
    printf("........ --> %s\n", element.data_link().c_str());
    
    LoadedMeshesMap::iterator it = loaded_meshes.find(element.data_link());
    
    if (it == loaded_meshes.end())
    {
        printf("WARNING: linked mesh data '%s' not loaded!\n", element.data_link().c_str());
        return false;
    }
    
    OSPModel model = it->second;
    
    float obj2world[16];
    osp::affine3f xform;
    
    object2world_from_protobuf(obj2world, element);
    affine3f_from_matrix(xform, obj2world);
    
    OSPGeometry instance = ospNewInstance(model, xform);    
    ospAddGeometry(world, instance);    
    ospRelease(instance);
    
    return true;
}

bool
receive_and_add_volume_data(TCPSocket *sock, const SceneElement& element)
{
    // Object-to-world matrix (copy of the parent object)
    
    float obj2world[16];
    
    object2world_from_protobuf(obj2world, element);

    // Custom properties
    
    const char *encoded_properties = element.properties().c_str();
    //printf("Received volume properties:\n%s\n", encoded_properties);
    
    json properties = json::parse(encoded_properties);
    
    // XXX we print the mesh_name here, but that mesh is replaced by the python
    // export after it receives the volume extents. so a bit confusing as that original
    // mesh is reported, but that isn't in the scene anymore
    printf("[VOLUME (mesh)] %s\n", element.name().c_str());
    printf("%s\n", properties.dump().c_str());
    
    // Prepare result
    
    VolumeLoadResult result;
    
    result.set_success(true);
    
    // Find load function 
    
    volume_load_function_t load_function = NULL;
    
    const std::string& volume_type = properties["_plugin"];
    
    VolumeLoadFunctionMap::iterator it = volume_load_functions.find(volume_type);
    if (it == volume_load_functions.end())
    {
        printf("No load function yet for volume type '%s'\n", volume_type.c_str());
        
        std::string plugin_name = "volume_" + volume_type + ".so";
        
        printf("Loading plugin %s\n", plugin_name.c_str());
        
        void *plugin = dlopen(plugin_name.c_str(), RTLD_LAZY);
        
        if (!plugin) 
        {
            result.set_success(false);
            result.set_message("Failed to open plugin");
            send_protobuf(sock, result);

            fprintf(stderr, "dlopen() error: %s\n", dlerror());
            return false;
        }
        
        // Clear previous error
        dlerror(); 
        
        load_function = (volume_load_function_t) dlsym(plugin, "load");
        
        if (load_function == NULL)
        {
            result.set_success(false);
            result.set_message("Failed to get load function from plugin");
            send_protobuf(sock, result);
    
            fprintf(stderr, "dlsym() error: %s\n", dlerror());
            return false;
        }
        
        volume_load_functions[volume_type] = load_function;
    }
    else
        load_function = it->second;
    
    // Let load function do its job
    
    struct timeval t0, t1;
    
    printf("Calling load function\n");
    
    gettimeofday(&t0, NULL);
    
    OSPVolume   volume;
    float       bbox[6];
    
    volume = load_function(bbox, result, properties, obj2world);
    
    gettimeofday(&t1, NULL);
    printf("Load function executed in %.3fs\n", time_diff(t0, t1));
    
    if (volume == NULL)
    {
        send_protobuf(sock, result);

        printf("ERROR: volume load function failed!\n");
        return false;
    }    
    
    // Load function succeeded
    
    result.set_hash(get_sha1(encoded_properties));
    
    for (int i = 0; i < 6; i++)
        result.add_bbox(bbox[i]);
    
    // Set up further volume parameters
    
    if (properties.find("_sampling_rate") != properties.end())
        ospSetf(volume,  "samplingRate", properties["_sampling_rate"].get<float>());
    else
        ospSetf(volume,  "samplingRate", 0.1f);
    
    if (properties.find("_gradient_shading") != properties.end())
        ospSet1i(volume,  "gradientShadingEnabled", properties["_gradient_shading"].get<bool>());
    else
        ospSet1i(volume,  "gradientShadingEnabled", false);
    
    if (properties.find("_pre_integration") != properties.end())
        ospSet1i(volume,  "preIntegration", properties["_pre_integration"].get<bool>());
    else
        ospSet1i(volume,  "preIntegration", false);    
    
    if (properties.find("_single_shade") != properties.end())
        ospSet1i(volume,  "singleShade", properties["_single_shade"].get<bool>());
    else
        ospSet1i(volume,  "singleShade", true);  
    
    ospSet1b(volume, "adaptiveSampling", false);
    
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
            float minval = properties["data_range"][0];
            float maxval = properties["data_range"][1];
            ospSet2f(tf, "valueRange", minval, maxval);
        }
        
        OSPData color_data = ospNewData(cool2warm_entries, OSP_FLOAT3, tf_colors);
        ospSetData(tf, "colors", color_data);
        ospRelease(color_data);
        
        OSPData opacity_data = ospNewData(cool2warm_entries, OSP_FLOAT, tf_opacities);
        ospSetData(tf, "opacities", opacity_data);
        ospRelease(opacity_data);
    
    ospCommit(tf);
    
    ospSetObject(volume,"transferFunction", tf);
    ospRelease(tf);
    
    ospCommit(volume);

    // Cache loaded volume object
    
    loaded_volumes[element.name()] = volume;

    send_protobuf(sock, result);
    
    return true;
}
    

bool
receive_and_add_volume_object(TCPSocket *sock, const SceneElement& element)
{
    // Object-to-world matrix
    
    float obj2world[16];
    
    object2world_from_protobuf(obj2world, element);

    // Custom properties
    
    const char *encoded_properties = element.properties().c_str();
    //printf("Received volume properties:\n%s\n", encoded_properties);
    
    json properties = json::parse(encoded_properties);
    
    printf("[VOLUME (object)] %s\n", element.name().c_str());
    printf("........ --> %s\n", element.data_link().c_str());
    
    LoadedVolumesMap::iterator it = loaded_volumes.find(element.data_link());
    
    if (it == loaded_volumes.end())
    {
        printf("WARNING: linked volume data '%s' not loaded!\n", element.data_link().c_str());
        return false;
    }
    
    OSPVolume volume = it->second;
    
#if 0
    // See https://github.com/ospray/ospray/pull/165, support for volume transformations was reverted
    ospSetVec3f(volume, "xfm.l.vx", osp::vec3f{ obj2world[0], obj2world[4], obj2world[8] });
    ospSetVec3f(volume, "xfm.l.vy", osp::vec3f{ obj2world[1], obj2world[5], obj2world[9] });
    ospSetVec3f(volume, "xfm.l.vz", osp::vec3f{ obj2world[2], obj2world[6], obj2world[10] });
    ospSetVec3f(volume, "xfm.p", osp::vec3f{ obj2world[3], obj2world[7], obj2world[11] });
#endif

    if (properties.find("isovalues") != properties.end())
    {
        // Isosurfacing
        
        printf("Representing volume with isosurface(s)\n");
        
        json isovalues_prop = properties["isovalues"];
        int n = isovalues_prop.size();

        float *isovalues = new float[n];
        for (int i = 0; i < n; i++)
            isovalues[i] = isovalues_prop[i];
        
        OSPData isovaluesData = ospNewData(n, OSP_FLOAT, isovalues);
        ospCommit(isovaluesData);
        
        delete [] isovalues;
        
        OSPGeometry isosurface = ospNewGeometry("isosurfaces");
        
            ospSetObject(isosurface, "volume", volume);
            ospRelease(volume);

            ospSetData(isosurface, "isovalues", isovaluesData);
            ospRelease(isovaluesData);
            
        ospCommit(isosurface);
            
        ospAddGeometry(world, isosurface);
        ospRelease(isosurface);
    }
    else if (properties.find("slice_plane") != properties.end())
    {
        // Slice plane (only a single one supported, atm)
        
        printf("Representing volume with slice plane\n");
        
        json slice_plane_prop = properties["slice_plane"];
        
        if (slice_plane_prop.size() == 4)
        {
            float plane[4];
            for (int i = 0; i < 4; i++)
                plane[i] = slice_plane_prop[i];
            
            OSPData planeData = ospNewData(4, OSP_FLOAT, plane);
            ospCommit(planeData);
            
            OSPGeometry slices = ospNewGeometry("slices");
            
                ospSetObject(slices, "volume", volume);
                ospRelease(volume);

                ospSetData(slices, "planes", planeData);
                ospRelease(planeData);
                
            ospCommit(slices);
                
            ospAddGeometry(world, slices);
            ospRelease(slices);
        }
        else
        {
            fprintf(stderr, "ERROR: slice_plane attribute should contain list of 4 floats values!\n");
        }
    }
    else
    {
        // Represent as volume 
        
        ospAddVolume(world, volume);
        ospRelease(volume);
    }

    return true;
}

// XXX currently has big memory leak as we never release the new objects ;-)
bool
receive_scene(TCPSocket *sock)
{
    // Image settings
    
    // XXX use percentage value? or is that handled in the blender side?
    
    receive_protobuf(sock, image_settings);
    
    if (image_size.x != image_settings.width() || image_size.y != image_settings.height())
    {
        image_size.x = image_settings.width() ;
        image_size.y = image_settings.height();
        
        if (framebuffer_created)
            ospRelease(framebuffer);
        
        framebuffer = ospNewFrameBuffer(image_size, OSP_FB_RGBA32F, OSP_FB_COLOR | /*OSP_FB_DEPTH |*/ OSP_FB_ACCUM);            
        framebuffer_created = true;
    }
    
    // Render settings
    
    receive_protobuf(sock, render_settings);
    
    renderer = ospNewRenderer(render_settings.renderer().c_str());
    
    ospSet4f(renderer, "bgColor", 
        render_settings.background_color(0),
        render_settings.background_color(1),
        render_settings.background_color(2),
        1.0f);
    
    ospSet1i(renderer, "aoSamples", render_settings.ao_samples());
    ospSet1i(renderer, "shadowsEnabled", render_settings.shadows_enabled());

    // Update camera
    
    receive_protobuf(sock, camera_settings);
    
    printf("[CAMERA] %s (%s)\n", camera_settings.object_name().c_str(), camera_settings.camera_name().c_str());
    
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
            ospSetf(camera, "fovy",  camera_settings.fov_y());
            break;
        
        case CameraSettings::ORTHOGRAPHIC:
            camera = ospNewCamera("orthographic");
            ospSetf(camera, "height", camera_settings.height());
            break;
            
        case CameraSettings::PANORAMIC:
            camera = ospNewCamera("panoramic");
            break;
        
        default:
            fprintf(stderr, "WARNING: unknown camera type %d\n", camera_settings.type());
            break;
    }

    ospSetf(camera, "aspect", camera_settings.aspect());
    ospSetf(camera, "nearClip", camera_settings.clip_start());     
    
    ospSet3fv(camera, "pos", cam_pos);
    ospSet3fv(camera, "dir", cam_viewdir);
    ospSet3fv(camera, "up",  cam_updir);
    
    if (camera_settings.dof_focus_distance() > 0.0f)
    {
        // XXX seem to stuck in loop during rendering when distance is 0
        ospSetf(camera, "focusDistance", camera_settings.dof_focus_distance());
        ospSetf(camera, "apertureRadius", camera_settings.dof_aperture());
    }
    ospCommit(camera); 
    
    ospSetObject(renderer, "camera", camera);
    
    // Lights
    
    receive_protobuf(sock, light_settings);
    
    const int num_lights = light_settings.lights_size();
    OSPLight *osp_lights = new OSPLight[num_lights+1];
    OSPLight osp_light;
    
    for (int i = 0; i < num_lights; i++)
    {
        const Light& light = light_settings.lights(i);
        
        printf("[LIGHT] %s (%s)\n", light.object_name().c_str(), light.light_name().c_str());
        
        if (light.type() == Light::POINT)
        {
            osp_light = osp_lights[i] = ospNewLight3("point");
        }
        else if (light.type() == Light::SPOT)
        {
            osp_light = osp_lights[i] = ospNewLight3("spot");
            ospSetf(osp_light, "openingAngle", light.opening_angle());
            ospSetf(osp_light, "penumbraAngle", light.penumbra_angle());
        }
        else if (light.type() == Light::SUN)
        {
            osp_light = osp_lights[i] = ospNewLight3("directional");
            ospSetf(osp_light, "angularDiameter", light.angular_diameter());
        }
        else if (light.type() == Light::AREA)            
        {
            // XXX blender's area light is more general than ospray quad light
            osp_light = osp_lights[i] = ospNewLight3("quad");
            ospSet3f(osp_light, "edge1", light.edge1(0), light.edge1(1), light.edge1(2));
            ospSet3f(osp_light, "edge2", light.edge2(0), light.edge2(1), light.edge2(2));
        }
        //else
        // XXX HDRI
        
        ospSet3f(osp_light, "color", light.color(0), light.color(1), light.color(2));
        ospSet1f(osp_light, "intensity", light.intensity());    
        ospSet1b(osp_light, "isVisible", light.visible());                      
        
        if (light.type() != Light::SUN)
            ospSet3f(osp_light, "position", light.position(0), light.position(1), light.position(2));
        
        if (light.type() == Light::SUN || light.type() == Light::SPOT)
            ospSet3f(osp_light, "direction", light.direction(0), light.direction(1), light.direction(2));
        
        if (light.type() == Light::POINT || light.type() == Light::SPOT)
            ospSetf(osp_light, "radius", light.radius());
        
        ospCommit(osp_light);      
    }
    
    // Ambient
    osp_lights[num_lights] = ospNewLight3("ambient");
    ospSet1f(osp_lights[num_lights], "intensity", light_settings.ambient_intensity());
    ospSet3f(osp_lights[num_lights], "color", 
        light_settings.ambient_color(0), light_settings.ambient_color(1), light_settings.ambient_color(2));
    
    ospCommit(osp_lights[num_lights]);
    
    OSPData light_data = ospNewData(num_lights+1, OSP_LIGHT, osp_lights);  
    ospCommit(light_data);
    
    ospSetObject(renderer, "lights", light_data); 
    
    // For now a single material
    
    material = ospNewMaterial2(render_settings.renderer().c_str(), "OBJMaterial");
    
    ospSet3f(material, "Kd", 0.8f, 0.8f, 0.8f);
    ospCommit(material);
    
    // Setup world and scene objects
    
    world = ospNewModel();
    
    SceneElement element;
    
    // XXX check return value
    receive_protobuf(sock, element);
    
    while (element.type() != SceneElement::NONE)
    {
        if (element.type() == SceneElement::MESH_DATA)
        {
            receive_and_add_mesh_data(sock, element);
        }
        else if (element.type() == SceneElement::MESH_OBJECT)
        {
            receive_and_add_mesh_object(sock, element);
        }
        else if (element.type() == SceneElement::VOLUME_DATA)
        {
            receive_and_add_volume_data(sock, element);
        }
        else if (element.type() == SceneElement::VOLUME_OBJECT)
        {
            receive_and_add_volume_object(sock, element);
        }
        // else XXX
        
        // XXX check return value
        receive_protobuf(sock, element);
    }
    
    ospCommit(world);
    
    ospSetObject(renderer, "model",  world);
    
    // Done!
    
    ospCommit(renderer);
    
    return true;
}

// Send result

size_t
write_framebuffer_exr(const char *fname)
{
    // Access framebuffer 
    const float *fb = (float*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
    
    writeEXRFramebuffer(fname, image_size, fb);
    
    // Unmap framebuffer
    ospUnmapFrameBuffer(fb, framebuffer);    
    
    struct stat st;
    stat(fname, &st);
    
    return st.st_size;
}

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
    
    size_t size = write_framebuffer_exr(FBFILE);
    
    printf("Sending framebuffer as OpenEXR file, %d bytes\n", size);
    
    bufsize = size;
    sock->send((uint8_t*)&bufsize, 4);
    sock->sendfile(FBFILE);
#else
    // Send directly
    bufsize = image_size.x*image_size.y*4*4;
    
    printf("Sending %d bytes of framebuffer data\n", bufsize);
    
    sock->send(&bufsize, 4);
    sock->sendall((uint8_t*)fb, image_size.x*image_size.y*4*4);
#endif
    
    // Unmap framebuffer
    ospUnmapFrameBuffer(fb, framebuffer);    
    
    gettimeofday(&t1, NULL);
    printf("Sent framebuffer in %.3f seconds\n", time_diff(t0, t1));
}

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
    ospFrameBufferClear(framebuffer, OSP_FB_COLOR | OSP_FB_ACCUM);

    for (int i = 1; i <= render_settings.samples(); i++)
    {
        printf("Rendering sample %d ... ", i);
        fflush(stdout);
        
        gettimeofday(&t1, NULL);

        ospRenderFrame(framebuffer, renderer, OSP_FB_COLOR | OSP_FB_ACCUM);

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
            
            switch (client_message.type())
            {
                case ClientMessage::UPDATE_SCENE:
                    // XXX handle clear_scene 
                    // XXX check res
                    receive_scene(sock);
                    break;
            
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
                    sock->sendfile(render_result.file_name().c_str());
                
                    // Remove local file
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
    
    // Framebuffer "init"
    image_size.x = image_size.y = 0;
    framebuffer_created = false;
    
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
