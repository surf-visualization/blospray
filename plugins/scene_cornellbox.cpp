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
#include <ospray/ospray_testing/ospray_testing.h>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "plugin.h"

using json = nlohmann::json;

extern "C" 
void
generate(GenerateFunctionResult &result, PluginState *state)
{    
    //const json& parameters = state->parameters;
    
    GroupInstances &instances = state->group_instances;

    auto builder = ospray::testing::newBuilder("cornell_box");
    ospray::testing::setParam(builder, "rendererType", state->renderer);
    ospray::testing::commit(builder);

    auto cpp_group = ospray::testing::buildGroup(builder);
    ospray::testing::release(builder);
    cpp_group.commit();

    OSPGroup group = cpp_group.handle();
    ospRetain(group);
    instances.push_back(std::make_pair(group, glm::mat4(1.0f)));
    
    // Set up area light in the ceiling
    // XXX this code comes from OSPRay's apps/tutorials/ospTutorialQuadMesh.cpp
    OSPLight light = ospNewLight("quad");
        ospSetVec3f(light, "color", 0.78f, 0.551f, 0.183f);
        ospSetFloat(light, "intensity", 47.f);
        ospSetVec3f(light, "position", -0.23f, 0.98f, -0.16f);
        ospSetVec3f(light, "edge1", 0.47f, 0.0f, 0.0f);
        ospSetVec3f(light, "edge2", 0.0f, 0.0f, 0.38f);
    ospCommit(light);
    state->lights.push_back(light);

    state->bound = BoundingMesh::bbox_from_group(group, true);
}

static PluginParameters 
parameters = {
        
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


