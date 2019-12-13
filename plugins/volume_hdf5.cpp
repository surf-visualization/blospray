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
#include "uhdf5.h"

#include "plugin.h"

extern "C" 
void
generate(PluginResult &result, PluginState *state)
{    
    const json& parameters = state->parameters;
    
    if (parameters.find("hdf5_file") == parameters.end())
    {
        fprintf(stderr, "ERROR: hdf5_file not set!\n");
        result.set_success(false);
        result.set_message("ERROR: hdf5_file not set!");
        return;
    }

    if (parameters.find("dataset") == parameters.end())
    {
        fprintf(stderr, "ERROR: dataset not set!\n");
        result.set_success(false);
        result.set_message("ERROR: dataset not set!");
        return;
    }    

    const std::string& hdf5_file = parameters["hdf5_file"];
    const std::string& dataset = parameters["dataset"];

    h5::File        file;
    h5::Dataset     *dset;
    h5::Attribute   *attr;
    h5::Type        *type;

    file.open(hdf5_file.c_str(), true);

    dset = file.open_dataset(dataset.c_str());

    // Assume Xdmf's Z,Y,X order
    h5::dimensions dims;
    dset->get_dimensions(dims);
    
    int t = dims[0];
    dims[0] = dims[2];
    dims[2] = t;
    
    printf("N=%d: %d x %d x %d\n", dims.size(), dims[0], dims[1], dims[2]);

    if (dims.size() != 3)
    {
        fprintf(stderr, "ERROR: dataset dimension is not 3!\n");
        result.set_success(false);
        result.set_message("ERROR: dataset dimension is not 3!");
        return;  
    }

    type = dset->get_type();
    printf("Dataset data class = %d, order = %d, size = %d, precision = %d, signed = %d\n",
        type->get_class(), type->get_order(), type->get_size(), type->get_precision(), type->is_signed());

    if (!type->matches<float>())
    {
        fprintf(stderr, "ERROR: type doesn't match float!\n");
        result.set_success(false);
        result.set_message("ERROR: type doesn't match float!");
        delete type;
        return;        
    }

    delete type;

    const int n = dims[0]*dims[1]*dims[2];
    float *grid_field_values = new float[n];
    float minval, maxval;

    dset->read<float>(grid_field_values);
    
    minval = std::numeric_limits<float>::max();
    maxval = std::numeric_limits<float>::min();

    for (int i = 0; i < n; i++)
    {
        minval = std::min(minval, grid_field_values[i]);
        maxval = std::max(maxval, grid_field_values[i]);
    }

    printf("... Data range: %.6f, %.6f\n", minval, maxval);
    
    if (parameters.find("fill") != parameters.end())
    {
        const json &fill = parameters["fill"];
        
        const int axis = fill[0].get<int>();
        const int min_index = fill[1].get<int>();
        const int max_index = fill[2].get<int>();
        const float value = fill[3].get<float>();
        
        printf("... Filling %c=%d..%d with %.6f\n", 'X'+axis, min_index, max_index, value);
        
        int i, j, k;
        
        // XXX oh, the horror....
        switch (axis)
        {
        case 2:
            for (k = min_index; k <= max_index; k++)
            {
                for (j = 0; j < dims[1]; j++)
                {
                    for (i = 0;  i < dims[0]; i++)
                    {
                        grid_field_values[(k*dims[1] + j)*dims[0] + i] = value;
                    }
                }
            }
            break;
        }
    }

    const json& p_origin = parameters["origin"];
    const json& p_spacing = parameters["spacing"];

    float origin[3] = {p_origin[0], p_origin[1], p_origin[2]};
    float spacing[3] = {p_spacing[0], p_spacing[1], p_spacing[2]};

    OSPDataType dataType = OSP_FLOAT;

    OSPVolume volume = ospNewVolume("structured_regular");
    
        OSPData voxelData = ospNewCopiedData(n, dataType, grid_field_values);   
        ospCommit(voxelData);
    
        ospSetObject(volume, "voxelData", voxelData);
        ospRelease(voxelData);

        ospSetInt(volume, "voxelType", dataType);
        ospSetVec3i(volume, "dimensions", dims[0], dims[1], dims[2]);
    
        ospSetParam(volume, "gridOrigin", OSP_VEC3F, origin);
        ospSetParam(volume, "gridSpacing", OSP_VEC3F, spacing);

    ospCommit(volume);

    state->volume = volume;
    
    if (parameters.find("value_range") != parameters.end())
    {
        const json& vrange = parameters["value_range"];
        state->volume_data_range[0] = vrange[0].get<float>();
        state->volume_data_range[1] = vrange[1].get<float>();        
    }
    else
    {    
        state->volume_data_range[0] = minval;
        state->volume_data_range[1] = maxval;
    }
    
    state->bound = BoundingMesh::bbox(
        origin[0], origin[1], origin[2],
        origin[0]+spacing[0]*dims[0], origin[1]+spacing[1]*dims[1], origin[2]+spacing[2]*dims[2],
        true
    ); 
}


static PluginParameters 
parameters = {
    
    {"hdf5_file",   PARAM_STRING,   1, FLAG_NONE, 
        "Path to HDF5 file"},
        
    {"dataset",     PARAM_STRING,   1, FLAG_NONE, 
        "Path of dataset to read"},
        
    {"origin",      PARAM_FLOAT,    3, FLAG_NONE, 
        "Origin of the volume"},
        
    {"spacing",     PARAM_FLOAT,    3, FLAG_NONE, 
        "Spacing of the volume"},

    {"fill",        PARAM_INT,      3, FLAG_OPTIONAL, 
        "Fill (overwrite) part of the volume (axis, minindex, maxindex, value)"},
        
    {"value_range", PARAM_FLOAT,    2, FLAG_OPTIONAL, 
        "Data range of the volume (derived from the data if not specified)"},        

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
    def->uses_renderer_type = false;
    def->parameters = parameters;
    def->functions = functions;
    
    // Do any other plugin-specific initialization here
    
    return true;
}
