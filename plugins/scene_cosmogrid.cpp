// ======================================================================== //
// BLOSPRAY - OSPRay as a Blender render engine                             //
// Paul Melis, SURFsara <paul.melis@surfsara.nl>                            //
// Cosmogrid dataset example plugin                                         //
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

#include <cstdio>
#include <stdint.h>
#include <ospray/ospray_testing/ospray_testing.h>
#include "uhdf5.h"

#include "plugin.h"

std::string         data_file;
OSPGeometricModel   model;

bool
load_points(const char *fname, int max_points, float sphere_radius, float sphere_opacity)
{
    printf("Loading %d points from %s\n", max_points, fname);

    uint32_t  num_points;
    float     *colors;  
    
    float       *positions;
    uint32_t    *nbcounts;
    
    // Read points
    
    h5::File        file;
    h5::Dataset     *dset;
    h5::Attribute   *attr;
    h5::Type        *type;

    file.open(fname);
    
    // Positions
    // XXX we actually load ALL the points, regardless of max_points

    dset = file.open_dataset("/positions");

    h5::dimensions dims;
    dset->get_dimensions(dims);
    printf("N=%d: %d, %d\n", dims.size(), dims[0], dims[1]);
    
    num_points = dims[0];

    type = dset->get_type();
    printf("Dataset data class = %d, order = %d, size = %d, precision = %d, signed = %d\n",
        type->get_class(), type->get_order(), type->get_size(), type->get_precision(), type->is_signed());

    if (!type->matches<float>())
    {
        printf("Type doesn't match float!\n");
        // XXX throw error
        exit(-1);
    }

    delete type;

    positions = new float[dims[0]*dims[1]];
    dset->read<float>(positions);
    
    delete dset;
    
    // Counts

    dset = file.open_dataset("/nbcounts");

    type = dset->get_type();
    printf("Dataset data class = %d, order = %d, size = %d, precision = %d, signed = %d\n",
        type->get_class(), type->get_order(), type->get_size(), type->get_precision(), type->is_signed());

    if (!type->matches<uint32_t>())
    {
        printf("Type doesn't match uint32_t!\n");
        // XXX error
        exit(-1);
    }

    delete type;

    nbcounts = new uint32_t[num_points];
    
    dset->read<uint32_t>(nbcounts);
    delete dset;
    
    file.close();
    
#if 0    
    // Set vertex colors
    
    colors = new float[4*num_vertices];
    
    for (int i = 0; i < num_vertices; i++)
    {
        colors[4*i+0] = 1.0f;   //187
        colors[4*i+1] = 0.0f;   //96
        colors[4*i+2] = 0.0f;   //96
        colors[4*i+3] = 1.0f;
    }
#endif

    // Create spheres
    
    OSPGeometry spheres = ospNewGeometry("spheres");
    
      OSPData data = ospNewData(num_points, OSP_VEC3F, positions);
      ospSetData(spheres, "sphere", data);
      ospSetInt(spheres, "bytes_per_sphere", 3*sizeof(float));
      ospSetFloat(spheres, "radius", sphere_radius);

      //data = ospNewData(num_vertices, OSP_VEC4F, colors);
      //ospCommit(data);
      //ospSetData(mesh, "vertex.color", data);
      
    ospCommit(spheres);
  
    // XXX renderer dependent
    OSPMaterial material = ospNewMaterial("pathtracer", "OBJMaterial");
        ospSetVec3f(material, "Kd", 1.0f, 1.0f, 1.0f);
        //ospSetFloat(material, "d", sphere_opacity);
    ospCommit(material);
    
    model = ospNewGeometricModel(spheres);
        ospSetObject(model, "material", material);
    ospCommit(model);
    ospRelease(material);
    ospRelease(spheres);
    
    delete [] positions;
    delete [] nbcounts;
        
    return true;
}

extern "C" 
void
generate(GenerateFunctionResult &result, PluginState *state)
{    
    const json& parameters = state->parameters;
    
    if (parameters.find("cosmogrid_data_file") != parameters.end())
        data_file = parameters["cosmogrid_data_file"];
    else 
    {
        const char *s = getenv("COSMOGRID_DATA_FILE");
        if (!s)
        {
            fprintf(stderr, "ERROR: COSMOGRID_DATA_FILE not set, nor parameter cosmogrid_data_file!\n");
            result.set_success(false);
            result.set_message("COSMOGRID_DATA_FILE not set, nor parameter cosmogrid_data_file!");
            return;
        }
        data_file = s;
    }
    
    printf("data_file = %s\n", data_file.c_str());
    
    int max_points = -1;
    float sphere_radius = 0.01f;
    float sphere_opacity = 1.0f;
    
    if (parameters.find("max_points") != parameters.end())
        max_points = parameters["max_points"].get<int>();
    
    if (parameters.find("sphere_radius") != parameters.end())
        sphere_radius = parameters["sphere_radius"].get<float>();
    if (parameters.find("sphere_opacity") != parameters.end())
        sphere_opacity = parameters["sphere_opacity"].get<float>();

    GroupInstances &instances = state->group_instances;
    
    /*
    if (!load_points(data_file.c_str(), max_points, sphere_radius, sphere_opacity))
    {
        result.set_success(false);
        result.set_message("Failed to load points from HDF5 file");
        return;
    }
    */

    float positions[6] = { 0, 0, 0, 1, 1, 1 };

    OSPGeometry spheres = ospNewGeometry("spheres");    
      OSPData data = ospNewData(2, OSP_VEC3F, positions);
      ospSetData(spheres, "sphere", data);
      ospSetInt(spheres, "bytes_per_sphere", 3*sizeof(float));
      ospSetFloat(spheres, "radius", 0.5f);
    ospCommit(spheres);
  
    OSPMaterial material = ospNewMaterial("pathtracer", "OBJMaterial");
        ospSetVec3f(material, "Kd", 0,0,0);//0.8f, 0.8f, 0.8f);
    ospCommit(material);
    
    model = ospNewGeometricModel(spheres);
        ospSetObject(model, "material", material);
    ospCommit(model);
    ospRelease(material);
    ospRelease(spheres);

    
    OSPGroup group = ospNewGroup();
        OSPData models = ospNewData(1, OSP_OBJECT, &model, 0);
        ospSetData(group, "geometry", models);  // XXX setdata after all?
        ospRelease(models);
    ospCommit(group);
    
    instances.push_back(std::make_pair(group, glm::mat4(1.0f)));
    // XXX hmm, can't release group here?    

    state->bound = BoundingMesh::bbox_edges(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
}


static PluginParameters 
parameters = {
    
    {"cosmogrid_data_file",   PARAM_STRING,   1, FLAG_SCENE, 
        "Path to data file"},
        
    {"max_points",        PARAM_INT,      1, FLAG_SCENE, 
        "Maximum number of points to load"},
        
    {"sphere_radius",        PARAM_FLOAT,      1, FLAG_SCENE, 
        "Radius of each sphere"},
        
    {"sphere_opacity",        PARAM_FLOAT,      1, FLAG_SCENE, 
        "Opacity of each sphere"},

    PARAMETERS_DONE         // Sentinel (signals end of list)
};

static PluginFunctions
functions = {

    NULL,           // Plugin load
    NULL,           // Plugin unload
    
    generate,       // Generate    
    NULL,           // Clear data
};


extern "C" bool
initialize(PluginDefinition *def)
{
    def->type = PT_SCENE;
    def->parameters = parameters;
    def->functions = functions;
    
    // Do any other plugin-specific initialization here
    
    return true;
}
