// ======================================================================== //
// BLOSPRAY - OSPRay as a Blender render engine                             //
// Paul Melis, SURFsara <paul.melis@surfsara.nl>                            //
// Red-blood cell data example plugin                                       //
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
#include <ospray/ospray.h>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//#include "util.h"       // XXX for ...?
#include "plugin.h"

using json = nlohmann::json;

std::string         rbc_data_path;
OSPGeometricModel   mesh_model_rbc, mesh_model_plt;
OSPGroup            rbc_group, plt_group;

bool
load_cell_models(const char *renderer_type)
{
    char fname[1024];
    
    uint32_t  num_vertices, num_triangles;
    float     *vertices, *colors;  
    uint32_t  *triangles;
    
    // Read RBC geometry
    
    sprintf(fname, "%s/rbc_normal_translated.bin", rbc_data_path.c_str());
    FILE *f = fopen(fname, "rb");
    if (!f)
        return false;

    fread(&num_vertices, 4, 1, f);
    fread(&num_triangles, 4, 1, f);
    
    printf("RBC: %d vertices, %d triangles\n", num_vertices, num_triangles);

    vertices = new float[3*num_vertices];    
    triangles = new uint32_t[3*num_triangles];

    fread(vertices, num_vertices*3*sizeof(float), 1, f);
    fread(triangles, num_triangles*3*sizeof(uint32_t), 1, f);

    fclose(f);    
    
    // Set vertex colors
    
    colors = new float[4*num_vertices];
    
    for (int i = 0; i < num_vertices; i++)
    {
        colors[4*i+0] = 1.0f;   //187
        colors[4*i+1] = 0.0f;   //96
        colors[4*i+2] = 0.0f;   //96
        colors[4*i+3] = 1.0f;
    }
        
    // Create mesh
    
    OSPGeometry mesh = ospNewGeometry("triangles");
    
      // XXX use ospRelease() here?
  
      OSPData data = ospNewCopiedData(num_vertices, OSP_VEC3F, vertices);    
      ospCommit(data);
      ospSetObject(mesh, "vertex.position", data);

      data = ospNewCopiedData(num_vertices, OSP_VEC4F, colors);
      ospCommit(data);
      ospSetObject(mesh, "vertex.color", data);

      data = ospNewCopiedData(num_triangles, OSP_VEC3UI, triangles);            
      ospCommit(data);
      ospSetObject(mesh, "index", data);
            
    ospCommit(mesh);

    delete [] vertices;
    delete [] colors;
    delete [] triangles;    
  
    // Create model
  
    OSPMaterial material = ospNewMaterial(renderer_type, "OBJMaterial");    
        ospSetVec3f(material, "Kd", 0.8f, 0, 0);        
    ospCommit(material);

    mesh_model_rbc = ospNewGeometricModel(mesh);
        ospSetObjectAsData(mesh_model_rbc, "material", OSP_MATERIAL, material);
    ospCommit(mesh_model_rbc);
    ospRelease(material);
    ospRelease(mesh);
    
    // Read PLT geometry

    sprintf(fname, "%s/plt_normal_translated.bin", rbc_data_path.c_str());
    f = fopen(fname, "rb");
    if (!f)
        return false;

    fread(&num_vertices, 4, 1, f);
    fread(&num_triangles, 4, 1, f);
    
    printf("PLT: %d vertices, %d triangles\n", num_vertices, num_triangles);

    vertices = new float[3*num_vertices];    
    triangles = new uint32_t[3*num_triangles];

    fread(vertices, num_vertices*3*sizeof(float), 1, f);
    fread(triangles, num_triangles*3*sizeof(uint32_t), 1, f);

    fclose(f);    
    
    // Set vertex colors
    
    colors = new float[4*num_vertices];
    
    for (int i = 0; i < num_vertices; i++)
    {
        colors[4*i+0] = 230 / 255.0f;
        colors[4*i+1] = 230 / 255.0f;
        colors[4*i+2] = 110 / 255.0f;
        colors[4*i+3] = 1.0f;
    }
        
    // Create mesh
    
    mesh = ospNewGeometry("triangles");
    
      // XXX use ospRelease() here?
  
      data = ospNewCopiedData(num_vertices, OSP_VEC3F, vertices);    
      ospCommit(data);
      ospSetObject(mesh, "vertex.position", data);

      data = ospNewCopiedData(num_vertices, OSP_VEC4F, colors);
      ospCommit(data);
      ospSetObject(mesh, "vertex.color", data);

      data = ospNewCopiedData(num_triangles, OSP_VEC3UI, triangles);            
      ospCommit(data);
      ospSetObject(mesh, "index", data);
    
    ospCommit(mesh);
    
    delete [] vertices;
    delete [] colors;
    delete [] triangles;    
  
    // Create model

    material = ospNewMaterial(renderer_type, "OBJMaterial");
        ospSetVec3f(material, "Kd", 0.8f, 0.8f, 0.8f);
    ospCommit(material);

    mesh_model_plt = ospNewGeometricModel(mesh);
        ospSetObjectAsData(mesh_model_plt, "material", OSP_MATERIAL, material);
    ospCommit(mesh_model_plt);
    ospRelease(mesh);
    ospRelease(material);
    
    // Create groups for instancing
    
    rbc_group = ospNewGroup();
        OSPData models = ospNewCopiedData(1, OSP_GEOMETRIC_MODEL, &mesh_model_rbc);
        ospSetObject(rbc_group, "geometry", models);
        ospRelease(models);
    ospCommit(rbc_group);

    plt_group = ospNewGroup();
        models = ospNewCopiedData(1, OSP_GEOMETRIC_MODEL, &mesh_model_plt);
        ospSetObject(plt_group, "geometry", models);
        ospRelease(models);
    ospCommit(plt_group);

    return true;
}

/*
void
add_ground_plane()
{
    uint32_t  num_vertices, num_triangles;
    float     *vertices, *colors;  
    uint32_t  *triangles;    
    
    num_vertices = 4;
    num_triangles = 2;
    
    vertices = new float[num_vertices*3];
    triangles = new uint32_t[num_triangles*3];
    colors = new float[num_vertices*4];
    
    const float M = 2000.0f;
    const float z = -100.0f;
    
    vertices[0] = 0.0f - M;
    vertices[1] = 0.0f - M;
    vertices[2] = z;

    vertices[3] = 2000.0f + M;
    vertices[4] = 0.0f - M;
    vertices[5] = z;

    vertices[6] = 2000.0f + M;
    vertices[7] = 1000.0f + M;
    vertices[8] = z;

    vertices[9] = 0.0f - M;
    vertices[10] = 1000.0f + M;
    vertices[11] = z;
    
    triangles[0] = 0;
    triangles[1] = 1;
    triangles[2] = 2;

    triangles[3] = 0;
    triangles[4] = 2;
    triangles[5] = 3;

    for (int i = 0; i < num_vertices; i++)
    {
        colors[4*i+0] = 0.5f;
        colors[4*i+1] = 0.5f;
        colors[4*i+2] = 0.5f;
        colors[4*i+3] = 1.0f;
    }    
    
    OSPGeometry mesh2 = ospNewGeometry("triangles");
  
      OSPData data = ospNewCopiedData(num_vertices, OSP_FLOAT3, vertices);   
      ospCommit(data);
      ospSetData(mesh2, "vertex.position", data);

      data = ospNewCopiedData(num_vertices, OSP_FLOAT4, colors);
      ospCommit(data);
      ospSetData(mesh2, "vertex.color", data);

      data = ospNewCopiedData(num_triangles, OSP_VEC3UI, triangles);            
      ospCommit(data);
      ospSetData(mesh2, "index", data);

    ospCommit(mesh2);
    
    ospAddGeometry(world, mesh2);    
    
    delete [] vertices;
    delete [] triangles;    
}
*/

extern "C" 
void
generate(PluginResult &result, PluginState *state)
{    
    const json& parameters = state->parameters;
    
    if (parameters.find("rbc_data_path") != parameters.end())
        rbc_data_path = parameters["rbc_data_path"];
    else 
    {
        const char *s = getenv("RBC_DATA_PATH");
        if (!s)
        {
            fprintf(stderr, "ERROR: RBC_DATA_PATH not set, nor parameter rbc_data_path!\n");
            result.set_success(false);
            result.set_message("RBC_DATA_PATH not set, nor parameter rbc_data_path!");
            return;
        }
        rbc_data_path = s;
    }
    
    printf("rbc_data_path = %s\n", rbc_data_path.c_str());
    
    int max_rbcs = -1;
    int max_plts = -1;
    
    if (parameters.find("num_rbcs") != parameters.end())
        max_rbcs = parameters["num_rbcs"].get<int>();
    if (parameters.find("num_plts") != parameters.end())
        max_plts = parameters["num_plts"].get<int>();
    
    if (!load_cell_models(state->renderer.c_str()))
    {
        result.set_success(false);
        result.set_message("Failed to load cell models");
        return;
    }
    
    uint32_t    num_rbc, num_plt, num_wbc;
    float       tx, ty, tz, rx, ry, rz;
    glm::mat4   R;

    char fname[1024];
    sprintf(fname, "%s/cells.bin", rbc_data_path.c_str());
    FILE *p = fopen(fname, "rb");
    
    if (!p)
    {
        fprintf(stderr, "ERROR: could not open %s!\n", fname);
        result.set_success(false);
        result.set_message("could not open cells.bin");
        return;
    }
    
    fread(&num_rbc, sizeof(uint32_t), 1, p);
    fread(&num_plt, sizeof(uint32_t), 1, p);
    fread(&num_wbc, sizeof(uint32_t), 1, p);    
    printf("On-disk scene: %d rbc, %d plt, %d wbc\n", num_rbc, num_plt, num_wbc);

    // Instantiate RBCs & PLTs
    
    GroupInstances &instances = state->group_instances;
    
    if (max_rbcs == -1)
        max_rbcs = num_rbc;
    
    printf("Adding %d RBCs\n", max_rbcs);    
      
    for (int i = 0; i < max_rbcs; i++)
    {      
        fread(&tx, sizeof(float), 1, p);
        fread(&ty, sizeof(float), 1, p);
        fread(&tz, sizeof(float), 1, p);
        fread(&rx, sizeof(float), 1, p);
        fread(&ry, sizeof(float), 1, p);
        fread(&rz, sizeof(float), 1, p);
        
        R = glm::mat4(1.0f);
        R = glm::translate(R, glm::vec3(tx,ty,tz));        
        R = glm::rotate(R, glm::radians(rx), glm::vec3(1,0,0));
        R = glm::rotate(R, glm::radians(ry), glm::vec3(0,1,0));
        R = glm::rotate(R, glm::radians(rz), glm::vec3(0,0,1));   
        
        // Add instance
        instances.push_back(std::make_pair(rbc_group, R));
    }    
    
    // Skip remaining RBCs in scene file
    
    for (int i = 0; i < num_rbc-max_rbcs; i++)
    {      
        fread(&tx, sizeof(float), 1, p);
        fread(&ty, sizeof(float), 1, p);
        fread(&tz, sizeof(float), 1, p);
        fread(&rx, sizeof(float), 1, p);
        fread(&ry, sizeof(float), 1, p);
        fread(&rz, sizeof(float), 1, p);
    }
    
    if (max_plts == -1)
        max_plts = num_plt;
    
    printf("Adding %d PLTs\n", max_plts);
      
    for (int i = 0; i < max_plts; i++)
    {      
        fread(&tx, sizeof(float), 1, p);
        fread(&ty, sizeof(float), 1, p);
        fread(&tz, sizeof(float), 1, p);
        fread(&rx, sizeof(float), 1, p);
        fread(&ry, sizeof(float), 1, p);
        fread(&rz, sizeof(float), 1, p);

        R = glm::mat4(1.0f);
        R = glm::translate(R, glm::vec3(tx,ty,tz));
        R = glm::rotate(R, glm::radians(rx), glm::vec3(1,0,0));
        R = glm::rotate(R, glm::radians(ry), glm::vec3(0,1,0));
        R = glm::rotate(R, glm::radians(rz), glm::vec3(0,0,1));
        
        /*glm::mat4 T = glm::translate(
        glm::mat4(1.0f), glm::vec3(tx, ty, tz)
        );*/
        
        // Add instance
        instances.push_back(std::make_pair(plt_group, R));
    }        
    
    fclose(p);        
    
    printf("Data loaded...\n");
    
    state->bound = BoundingMesh::bbox(
        0.0f, 0.0f, 0.0f, 
        2000.0f, 1000.0f, 1000.0f
    );
}

static PluginParameters 
parameters = {
    
    {"rbc_data_path",   PARAM_STRING,   1, FLAG_NONE, 
        "Path to data files"},
        
    {"num_rbcs",        PARAM_INT,      1, FLAG_NONE, 
        "Limit number of RBCs"},
        
    {"num_plts",        PARAM_INT,      1, FLAG_NONE, 
        "Limit number of PLTs"},
        
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
    def->uses_renderer_type = true;
    def->parameters = parameters;
    def->functions = functions;
    
    // Do any other plugin-specific initialization here
    
    return true;
}


