#include <dlfcn.h>
#include <cstdio>
#include <stdint.h>
#include <ospray/ospray.h>
#include "json.hpp"

OSPVolume
load(nlohmann::json &parameters, float *bbox)
{
    //assert parameters["voltype"] == "raw";
    
    // XXX check for presence of "file", "header_skip", etc. entries
    std::string fname = parameters["file"].get<std::string>();
    
    FILE *f = fopen(fname.c_str(), "rb");
    if (!f)
    {
        fprintf(stderr, "Could not open file '%s'\n", fname.c_str());
        return NULL;
    }
    
    // XXX 
    fseek(f, parameters["header_skip"].get<int>(), SEEK_SET);
    
    int32_t dims[3];
    uint32_t num_voxels;
    uint32_t voxels_size;
    
    dims[0] = parameters["dimensions"][0];
    dims[1] = parameters["dimensions"][1];
    dims[2] = parameters["dimensions"][2];
    num_voxels = dims[0] * dims[1] * dims[2];
    
    
    void        *voxels;
    std::string voxelType = parameters["voxel_type"].get<std::string>();
    OSPDataType dataType;
    
    if (voxelType == "uchar")
    {
        voxels = new uint8_t[num_voxels];
        voxels_size = num_voxels;
        dataType = OSP_UCHAR;
    }
    // else XXX
    
    // XXX check bytes read
    fread(voxels, 1, voxels_size, f);
    fclose(f);

    // Set up ospray volume 
    
    OSPData voxelData = ospNewData(num_voxels, dataType, voxels);   
    
    // XXX could keep voxels around if we use shared flag
    if (voxelType == "uchar")
        delete [] ((uint8_t*)voxels);
    
    OSPVolume volume = ospNewVolume("shared_structured_volume");
    
    ospSetData(volume,  "voxelData", voxelData);

    ospSetString(volume,"voxelType", voxelType.c_str());
    // XXX allow voxelRange to be set in json
    ospSet2f(volume,    "voxelRange", 0.0f, 255.0f);
    ospSet3i(volume,    "dimensions", dims[0], dims[1], dims[2]);
    // XXX allow grid settings to be set in json
    ospSet3f(volume,    "gridOrigin", 0.0f, 0.0f, 0.0f);
    ospSet3f(volume,    "gridSpacing", 1.0f, 1.0f, 1.0f);
    
    // Transfer function
    osp::vec3f  colors[2] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
    float       opacities[2] = { 0.0f, 1.0f };
    
    OSPTransferFunction tf = ospNewTransferFunction("piecewise_linear");
    // XXX allow voxelRange to be set in json
    ospSet2f(tf,   "valueRange", 0.0f, 255.0f);
    ospSetData(tf, "colors", ospNewData(2, OSP_FLOAT3, colors));
    ospSetData(tf, "colors", ospNewData(2, OSP_FLOAT, opacities));
    ospCommit(tf);
    
    ospSetObject(volume,"transferFunction", tf);

    ospCommit(volume);
    
    // XXX update bbox entries
    
    bbox[0] = bbox[1] = bbox[2] = 0.0f;
    bbox[3] = dims[0];
    bbox[4] = dims[1];
    bbox[5] = dims[2];
    
    return volume;
}
