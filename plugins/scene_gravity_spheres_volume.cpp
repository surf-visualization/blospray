// ======================================================================== //
// BLOSPRAY - OSPRay as a Blender render engine                             //
// Paul Melis, SURFsara <paul.melis@surfsara.nl>                            //
// Raw volume loading plugin                                                //
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
#include "json.hpp"
#include "plugin.h"

using json = nlohmann::json;

extern "C"
void
generate(GenerateFunctionResult &result, PluginState *state)
{
    //const json& parameters = state->parameters;

    GroupInstances &instances = state->group_instances;

    auto builder = ospray::testing::newBuilder("gravity_spheres_volume");
    ospray::testing::setParam(builder, "rendererType", state->renderer);
    ospray::testing::commit(builder);

    auto cpp_group = ospray::testing::buildGroup(builder);
    ospray::testing::release(builder);
    cpp_group.commit();

    OSPGroup group = cpp_group.handle();
    ospRetain(group);
    instances.push_back(std::make_pair(group, glm::mat4(1.0f)));

    state->bound = BoundingMesh::bbox_from_group(group, true);
}

static PluginParameters 
parameters = {
    
    // Name, type, length, flags, description        

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
    
    // XXXX Do any other plugin-specific initialization here
    
    return true;
}

