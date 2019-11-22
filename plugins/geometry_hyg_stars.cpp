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

#include <cstdio>
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <ospray/ospray.h>

#include "plugin.h"

void
create_spheres(PluginState *state, json& j, bool project, float scale, float radius, int bound_subsampling=10)
{
    std::vector<float> positions;
    std::vector<float> radii;

    BoundingMesh *bound = new BoundingMesh;
    std::vector<float>& bound_vertices = bound->vertices;

    float x, y, z;
    float mag, brightness;
    float min[3] = {1e6, 1e6, 1e6}, max[3] = {-1e6, -1e6, -1e6};    // XXX use float min/max
    float minmag = 1e6, maxmag = -1e6;
    int idx = 0;

    for (json::iterator it = j.begin(); it != j.end(); ++it) 
    {
        const json &e = *it;

        //printf("%s\n", e["x"].dump().c_str());

        x = e["x"].get<float>();
        y = e["y"].get<float>();
        z = e["z"].get<float>();

        if (project)
            scale = 1.0f / sqrt(x*x + y*y + z*z);

        x *= scale;
        y *= scale;
        z *= scale;

        positions.push_back(x);
        positions.push_back(y);
        positions.push_back(z);

        // Faintest stars naked to the visible eye are around +6.5 magnitude.
        // A magnitude of 5 units higher means 100 times dimmer
        // Magnitude 0 maps to radius.
        mag = e["mag"].get<float>();

        brightness = 1.0f;
        if (mag >= 0.0f)
            brightness = 1.0f / (powf(2.512f, mag));

        radii.push_back(radius*sqrt(brightness));

        min[0] = std::min(min[0], x);
        min[1] = std::min(min[1], y);
        min[2] = std::min(min[2], z);

        max[0] = std::max(max[0], x);
        max[1] = std::max(max[1], y);
        max[2] = std::max(max[2], z);

        minmag = std::min(minmag, mag);
        maxmag = std::max(maxmag, mag);

        if (idx % bound_subsampling == 0)
        {
            bound_vertices.push_back(x);
            bound_vertices.push_back(y);
            bound_vertices.push_back(z);
        }

        idx++;
    }

    printf("... Bounds %.6f %.6f %.6f; %.6f %.6f %.6f\n", 
        min[0], min[1], min[2], max[0], max[1], max[2]);
    printf("... Magnitude range %.6f %.6f\n", minmag, maxmag);

    std::vector<float>::iterator minr, maxr;

    minr = std::min_element(radii.begin(), radii.end());
    maxr = std::max_element(radii.begin(), radii.end());

    printf("... Radius range %.6f %.6f\n", *minr, *maxr);

    OSPData data;

    OSPGeometry spheres = ospNewGeometry("spheres");
    
        data = ospNewCopiedData(positions.size()/3, OSP_VEC3F, positions.data());
        ospCommit(data);
        ospSetObject(spheres, "sphere.position", data);

        //ospSetFloat(spheres, "radius", radius);
        data = ospNewCopiedData(radii.size(), OSP_FLOAT, radii.data());
        ospCommit(data);
        ospSetObject(spheres, "sphere.radius", data);

        //data = ospNewCopiedData(num_vertices, OSP_VEC4F, colors);
        //ospCommit(data);
        //ospSetData(mesh, "vertex.color", data);
      
    ospCommit(spheres);
  
    state->geometry = spheres;
    state->bound = bound;
}

extern "C"
void
create_geometry(PluginResult &result, PluginState *state)
{    
    const float radius = state->parameters["radius"];
    const float scale = state->parameters["scale"];
    const int project = state->parameters["project"];
    const std::string& file = state->parameters["file"];

    std::ifstream fs;
    json j;

    fs.open(file);

    if (!fs.is_open())
    {
        char msg[1024];
        sprintf(msg, "Could not open file '%s'", file.c_str());
        result.set_success(false);
        result.set_message(msg);
        return;
    }

    fs >> j;
    fs.close();

    create_spheres(state, j, project, scale, radius);
}

static PluginParameters 
parameters = {
    
    {"file",    PARAM_STRING,   1, FLAG_NONE, "File to load"},
    {"scale",   PARAM_FLOAT,    1, FLAG_NONE, "Scale factor to apply during reading"},
    {"radius",  PARAM_FLOAT,    1, FLAG_NONE, "Base sphere radius (unscaled by magnitude)"},
    {"project",  PARAM_INT,    1, FLAG_NONE, "Project positions on a unit sphere"},
        
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


