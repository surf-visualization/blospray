// ======================================================================== //
// BLOSPRAY - OSPRay as a Blender render engine                             //
// Paul Melis, SURFsara <paul.melis@surfsara.nl>                            //
// Simple geometry plugin example                                           //
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

// A plane of 1x1 units in X and Y, centered at the origin (for testing)
#include <cstdio>
#include <stdint.h>
#include <ospray/ospray.h>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "plugin.h"

OSPGeometry
create_plane(float cx, float cy, float cz, float sx, float sy)
{
    uint32_t  num_vertices, num_triangles;
    float     *vertices, *colors;  
    uint32_t  *triangles;    
    
    num_vertices = 4;
    num_triangles = 2;
    
    vertices = new float[num_vertices*3];
    triangles = new uint32_t[num_triangles*3];
    colors = new float[num_vertices*4];
    
    vertices[0] = cx - 0.5f*sx;
    vertices[1] = cy - 0.5f*sy;
    vertices[2] = cz;

    vertices[3] = cx + 0.5f*sx;
    vertices[4] = cy - 0.5f*sy;
    vertices[5] = cz;

    vertices[6] = cx + 0.5f*sx;
    vertices[7] = cy + 0.5f*sy;
    vertices[8] = cz;

    vertices[9] = cx - 0.5f*sx;
    vertices[10] = cy + 0.5f*sy;
    vertices[11] = cz;
    
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
    
    OSPGeometry mesh = ospNewGeometry("triangles");
  
        OSPData data = ospNewData(num_vertices, OSP_VEC3F, vertices);   
        ospCommit(data);
        ospSetData(mesh, "vertex.position", data);

        data = ospNewData(num_vertices, OSP_VEC4F, colors);
        ospCommit(data);
        ospSetData(mesh, "vertex.color", data);

        data = ospNewData(num_triangles, OSP_VEC3I, triangles);            
        ospCommit(data);
        ospSetData(mesh, "index", data);

    ospCommit(mesh);
    
    delete [] vertices;
    delete [] colors;
    delete [] triangles;    
    
    return mesh;
}

extern "C"
void
create_geometry(GenerateFunctionResult &result, PluginState *state)
{    
    const float size_x = state->parameters["size_x"];
    const float size_y = state->parameters["size_y"];
    
    state->geometry = create_plane(0.0f, 0.0f, 0.0f, size_x, size_y);    
    
    // XXX too large
    state->bbox[0] = -1.0f;
    state->bbox[1] = -1.0f;
    state->bbox[2] = -1.0f;
    
    state->bbox[3] = 1.0f;
    state->bbox[4] = 1.0f;
    state->bbox[5] = 1.0f;    
}

static PluginParameters 
parameters = {
    
    {"size_x",          PARAM_FLOAT,    1, FLAG_GEOMETRY, "Size in X"},
    {"size_y",          PARAM_FLOAT,    1, FLAG_GEOMETRY, "Size in Y"},
        
    PARAMETERS_DONE         // Sentinel (signals end of list)
};

static PluginFunctions
functions = {

    NULL,               // Plugin load
    NULL,               // Plugin unload
    
    create_geometry,    // Generate
    
    NULL,               // Clear data
};


extern "C" bool
initialize(PluginDefinition *def)
{
    def->type = PT_GEOMETRY;
    def->parameters = parameters;
    def->functions = functions;
    
    // Do any other plugin-specific initialization here
    
    return true;
}


