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
#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>
#include "json.hpp"
#include "plugin.h"

using json = nlohmann::json;

extern "C"
void
generate(GenerateFunctionResult &result, PluginState *state)
{
    const json& parameters = state->parameters;
    
    // Open file
    
    std::string fname = parameters["file"].get<std::string>();
    
    // Create object
    openvdb::io::File file(fname.c_str());

    // Open the file.  This reads the file header, but not any grids.
    file.open();
    
    /*if (!f)
    {
        char msg[1024];
        snprintf(msg, 1024, "Could not open file '%s'", fname.c_str());
        result.set_success(false);
        result.set_message(msg);
        fprintf(stderr, "ERROR: %s\n", msg);
        return;
    }
    */
    
    // Read the density grid
    openvdb::GridBase::Ptr baseGrid = file.readGrid("density");
    
    // Done with the file already
    file.close();
    
    // Cast grid to floag grid
    openvdb::FloatGrid::Ptr grid = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);
    
    /*
    for (openvdb::MetaMap::MetaIterator iter = grid->beginMeta(); iter != grid->endMeta(); ++iter)
    {
        const std::string& name = iter->first;
        openvdb::Metadata::Ptr value = iter->second;
        std::string valueAsString = value->str();
        std::cout << name << " = " << valueAsString << std::endl;
    }
    */

    //openvdb::MetaMap::Ptr metadata = grid->getMetadata("file_bbox_min");
    auto& file_bbox_min = grid->metaValue<openvdb::Vec3i>("file_bbox_min");
    auto& file_bbox_max = grid->metaValue<openvdb::Vec3i>("file_bbox_max");

    const openvdb::Vec3d voxel_size = grid->voxelSize();

#if 0
    // Create AMR volume
    
    OSPVolume volume = ospNewVolume("amr_volume");
    
    ospSetInt(volume, "voxelType", OSP_FLOAT);
    
    float origin[3] = { 0.0f, 0.0f, 0.0f };
    float spacing[3] = { 1.0f, 1.0f, 1.0f };
    
    if (parameters.find("grid_origin") != parameters.end())
    {
        auto o = parameters["grid_origin"];
        origin[0] = o[0];
        origin[1] = o[1];
        origin[2] = o[2];
    }

    if (parameters.find("grid_spacing") != parameters.end())
    {
        auto s = parameters["grid_spacing"];
        spacing[0] = s[0];
        spacing[1] = s[1];
        spacing[2] = s[2];
    }
    
    ospSetVec3f(volume, "gridOrigin", origin[0], origin[1], origin[2]);
    ospSetVec3f(volume, "gridSpacing", spacing[0], spacing[1], spacing[2]);
    
    // Iterate over leaf nodes in the volume and fill the necessary data arrays
    
    std::vector<float>      block_bounds;
    std::vector<float>      block_cellwidth;
    std::vector<float>      block_level;
    std::vector<OSPData>    block_data;
    OSPData                 voxel_values;
    int                     num_leafs = 0;
    
    
    using TreeType = openvdb::FloatGrid::TreeType;
    
    // XXX keep track of min/max range here
    for (TreeType::LeafCIter iter = grid->tree().cbeginLeaf(); iter; ++iter) 
    {
        const TreeType::LeafNodeType& leaf = *iter;
        
        /*printf("--- leaf ---\n");
        std::cout << "origin " << leaf.origin() << std::endl;
        std::cout << "dim " << leaf.dim() << std::endl;
        std::cout << "size " << leaf.size() << std::endl;
        std::cout << "isDense " << leaf.isDense() << std::endl;
        std::cout << "offLeafVoxelCount " << leaf.offLeafVoxelCount() << std::endl;*/
        
        const auto& buffer = leaf.buffer();
        const float *values = buffer.data();
        //printf("%.6f\n", values[0]);
        
        int D = leaf.dim();
        const int N = D*D*D;
        auto O = leaf.origin();
        
        block_bounds.push_back(O.x());
        block_bounds.push_back(O.y());
        block_bounds.push_back(O.z());
        block_bounds.push_back(O.x()+D);
        block_bounds.push_back(O.y()+D);
        block_bounds.push_back(O.z()+D);
        
        block_level.push_back(0);
        block_cellwidth.push_back(1.0f);
        
        voxel_values = ospNewCopiedData(N, OSP_FLOAT, values);
        ospCommit(voxel_values);
        
        block_data.push_back(voxel_values);
        
        num_leafs++;
    }
    
    OSPData data = ospNewCopiedData(num_leafs, OSP_DATA, &block_data[0]);
    ospCommit(data);
    ospSetObject(volume, "block.data", data);
    ospRelease(data);

    data = ospNewCopiedData(num_leafs, OSP_BOX3F, &block_bounds[0]);
    ospCommit(data);
    ospSetObject(volume, "block.bounds", data);
    ospRelease(data);

    data = ospNewCopiedData(num_leafs, OSP_INT, &block_level[0]);
    ospCommit(data);
    ospSetObject(volume, "block.level", data);
    ospRelease(data);
        
    data = ospNewCopiedData(num_leafs, OSP_FLOAT, &block_level[0]);
    ospCommit(data);
    ospSetObject(volume, "block.cellWidth", data);
    ospRelease(data);
    
    //ospSetVec2f(volume, "voxelRange", 0.0f, 1.0f);
    
    ospCommit(volume);
    
    state->volume = volume;
    state->volume_data_range[0] = 0.0f;
    state->volume_data_range[1] = 1.0f;    
    
    float bbox[6] = { -70, -70, -70, 70, 70, 70 };
    
    state->bound = BoundingMesh::bbox(
        bbox[0], bbox[1], bbox[2],
        bbox[3], bbox[4], bbox[5]
    );
#else
    // Create regular volume

    int nx = file_bbox_max.x() - file_bbox_min.x() + 1;
    int ny = file_bbox_max.y() - file_bbox_min.y() + 1;
    int nz = file_bbox_max.z() - file_bbox_min.z() + 1;
    printf("... Volume is %d x %d x %d\n", nx, ny, nz);

    const int min_x = file_bbox_min.x();
    const int min_y = file_bbox_min.y();
    const int min_z = file_bbox_min.z();
    printf("... Origin is %d, %d, %d\n", min_x, min_y, min_z);

    printf("... Voxel size %.6f\n", voxel_size);

    OSPVolume volume = ospNewVolume("structured_volume");
    
    ospSetInt(volume, "voxelType", OSP_FLOAT);
    ospSetVec3i(volume, "dimensions", nx, ny, nz);
    ospSetVec3f(volume, "gridOrigin", min_x*voxel_size.x(), min_y*voxel_size.y(), min_z*voxel_size.z());
    ospSetVec3f(volume, "gridSpacing", voxel_size.x(), voxel_size.y(), voxel_size.z());

    // This is used in mitsuba-vdb to sample at an arbitrary location
    //openvdb::tools::GridSampler<openvdb::FloatTree, openvdb::tools::BoxSampler> interpolator(floatGrid->constTree() , floatGrid->transform());
    //data = interpolator.wsSample(openvdb::math::Vec3d(p.x, p.y, p.z));

    // Use index-space sampler instead
    
    float *data = new float[nx*ny*nz];
    openvdb::FloatGrid::ConstAccessor accessor = grid->getConstAccessor();

    for (int k = 0; k < nz; k++)
    {
        for (int j = 0; j < ny; j++)
        {
            for (int i = 0; i < nx; i++)
            {
                const openvdb::Vec3R ijk(i+min_x, j+min_y, k+min_z);
                data[(k*ny + j)*nx + i] = openvdb::tools::PointSampler::sample(accessor, ijk);
            }
        }    
    }

    //OSPData voxel_data = ospNewCopiedData(nx*ny*nz, OSP_FLOAT, data);
    OSPData voxel_data = ospNewSharedData(data, OSP_FLOAT, nx*ny*nz);
    ospSetObject(volume, "voxelData", voxel_data);
    ospRelease(voxel_data);

    //delete [] data;

    ospCommit(volume);
    
    state->volume = volume;
    state->volume_data_range[0] = 0.0f;
    state->volume_data_range[1] = 1.0f;    
    
    float bbox[6] = { 
        file_bbox_min.x()*voxel_size.x(), file_bbox_min.y()*voxel_size.y(), file_bbox_min.z()*voxel_size.z(),
        file_bbox_max.x()*voxel_size.x(), file_bbox_max.y()*voxel_size.y(), file_bbox_max.z()*voxel_size.z()
     };
    
    state->bound = BoundingMesh::bbox(
        bbox[0], bbox[1], bbox[2],
        bbox[3], bbox[4], bbox[5]
    );

#endif
}


// XXX header_skip -> header-skip?
static PluginParameters 
parameters = {
        
    {"file",                PARAM_STRING,   1, FLAG_VOLUME, "File to read"},
        
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
    
    openvdb::initialize();
    
    return true;
}

