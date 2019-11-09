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

// Debug plugin that generates a large geometry, for checking correct memory clearing, etc.
#include <cstdio>
#include <stdint.h>
#include <stdlib.h>
#include <ospray/ospray.h>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "plugin.h"

OSPGeometry
create_triangles(int num_triangles, float max_edge_len)
{
    uint32_t  num_vertices = 3 * num_triangles;
    float     *vertices, *colors;  
    uint32_t  *triangles;    
    float     base_vertex[3], d[3];
    float     len;
    
    vertices = new float[num_vertices*3];
    triangles = new uint32_t[num_triangles*3];
    colors = new float[num_vertices*4];

    for (int t = 0; t < num_triangles; t++)
    {
        for (int j = 0; j < 3; j++)
        {
            float *v = &vertices[(t*3+j)*3];
            float *c = &colors[(t*3+j)*4];

            v[0] = (float) drand48();
            v[1] = (float) drand48();
            v[2] = (float) drand48();

            if (j == 0)
                memcpy(base_vertex, v, 3*sizeof(float));
            else
            {
                for (int i = 0; i < 3; i++)
                    d[i] = v[i] - base_vertex[i];

                len = sqrtf(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
                len = max_edge_len / len;

                for (int i = 0; i < 3; i++)
                    v[i] = base_vertex[i] + d[i] * len;
            }

            c[0] = (float) drand48();
            c[1] = (float) drand48();
            c[2] = (float) drand48();
            c[3] = 1.0f;
        }

        triangles[3*t+0] = 3*t+0;
        triangles[3*t+1] = 3*t+1;
        triangles[3*t+2] = 3*t+2;
    }
    
    OSPGeometry mesh = ospNewGeometry("triangles");
  
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
    
    return mesh;
}

extern "C"
void
create_geometry(GenerateFunctionResult &result, PluginState *state)
{
    const json& parameters = state->parameters;
    const int n = state->parameters["n"];
    
    float max_edge_len = 0.05f;
    if (parameters.find("max_edge_len") != parameters.end())
        max_edge_len = parameters["max_edge_len"];
    
    state->geometry = create_triangles(n, max_edge_len);    
    
    state->bound = BoundingMesh::bbox(
        0.0f, 0.0f, 0.0f, 
        1.0f, 1.0f, 1.0f,
        true);
}

static PluginParameters 
parameters = {
    
    {"n",               PARAM_INT,    1, FLAG_NONE, "Number of triangles"},
    {"max_edge_len",    PARAM_FLOAT,  1, FLAG_OPTIONAL, "Maximum edge length (XXX within the domain)"},
        
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
    def->uses_renderer_type = false;
    def->parameters = parameters;
    def->functions = functions;
    
    // Do any other plugin-specific initialization here
    
    return true;
}


