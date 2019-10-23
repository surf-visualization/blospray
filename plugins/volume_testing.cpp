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
    const json& parameters = state->parameters;
    
    OSPTestingVolume test_data = ospTestingNewVolume("gravity_spheres_volume");
    
    OSPVolume volume = test_data.volume;

    const osp_vec2f& voxelRange = test_data.voxelRange;
    const osp_box3f& bounds = test_data.bounds;

    printf("volume data range: %.6f %.6f\n", voxelRange.x, voxelRange.y);
    printf("volume bound: %.6f %.6f %.6f -> %.6f %.6f %.6f\n",
        bounds.lower.x, bounds.lower.y, bounds.lower.z,
        bounds.upper.x, bounds.upper.y, bounds.upper.z
        );
    
    /*
    OSPTransferFunction tfn =
      ospTestingNewTransferFunction(test_data.voxelRange, "jet");

    OSPVolumetricModel volumeModel = ospNewVolumetricModel(volume);
        ospSetObject(volumeModel, "transferFunction", tfn);
        ospSetFloat(volumeModel, "samplingRate", 0.5f);
    ospCommit(volumeModel);

    ospRelease(tfn);
    */
        
    state->volume = volume;
    state->volume_data_range[0] = voxelRange.x;
    state->volume_data_range[1] = voxelRange.y;
    state->bound = BoundingMesh::bbox(
        bounds.lower.x, bounds.lower.y, bounds.lower.z,
        bounds.upper.x, bounds.upper.y, bounds.upper.z
    );
}


// XXX header_skip -> header-skip?
static PluginParameters 
parameters = {
    
    // Name, type, length, flags, description        
    //{"voxel_type",          PARAM_STRING,   1, FLAG_NONE, 
    //    "Voxel data type (uchar, float)"},
        
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
    def->type = PT_VOLUME;
    def->uses_renderer_type = false;
    def->parameters = parameters;
    def->functions = functions;
    
    // XXXX Do any other plugin-specific initialization here
    
    return true;
}

