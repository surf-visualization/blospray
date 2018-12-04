#include <cstdio>
#include <stdint.h>
#include <ospray/ospray.h>
#include "json.hpp"

using json = nlohmann::json;

extern "C"
OSPVolume
load(json &parameters, const float *object2world, float *bbox)
{
    //assert parameters["voltype"] == "raw2";
    
    // XXX check for existence of "file", "header_skip", etc. entries in parameters[]
    
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
    uint32_t num_grid_points, num_hexahedrons;
    uint32_t data_size;
    
    dims[0] = parameters["dimensions"][0];
    dims[1] = parameters["dimensions"][1];
    dims[2] = parameters["dimensions"][2];
    
    num_grid_points = dims[0] * dims[1] * dims[2];
    num_hexahedrons = (dims[0]-1) * (dims[1]-1) * (dims[2]-1);
    
    void        *grid_field_values;
    std::string voxelType = parameters["voxel_type"].get<std::string>();
    OSPDataType dataType;
    
    if (voxelType == "uchar")
    {
        grid_field_values = new uint8_t[num_grid_points];
        data_size = num_grid_points;
        dataType = OSP_UCHAR;
    }
    // else XXX
    
    // XXX check bytes read
    fread(grid_field_values, 1, data_size, f);
    
    fclose(f);

    // We use an unstructured volume for now, as we can transform its
    // vertices with the object2world matrix, as volumes currently don't
    // support affine transformations in ospray.
    
    // Set (transformed) vertices
    
    float   *vertices = new float[num_grid_points*4];
    float   *v;
    float   x, y, z;
    float   xx, yy, zz;
    
    v = vertices;
    for (int k = 0; k < dims[2]; k++)
    {
        z = k;
        
        for (int j = 0; j < dims[1]; j++)
        {
            y = j;
            
            for (int i = 0; i < dims[0]; i++)
            {
                x = i;
                
                xx = x*object2world[0] + y*object2world[1] + z*object2world[2] + object2world[3];
                yy = x*object2world[4] + y*object2world[5] + z*object2world[6] + object2world[7];
                yy = x*object2world[8] + y*object2world[9] + z*object2world[10] + object2world[11];
                
                v[0] = xx;
                v[1] = yy;
                v[2] = zz;
                v[3] = 0.0f;
                v += 4;
            }
        }
    }
    
    // Set up hexahedral cells
    
    int     *indices = new int[num_hexahedrons*8];   
    
    // XXX...

    // Set up volume object
    
    OSPData verticesData = ospNewData(num_grid_points, OSP_FLOAT3A, vertices);   
    OSPData fieldData = ospNewData(num_grid_points, dataType, grid_field_values);   
    OSPData indicesData = ospNewData(num_hexahedrons*2, OSP_INT4, indices);
    
    OSPVolume volume = ospNewVolume("unstructured_volume");
    
    ospSetData(volume,      "vertices", verticesData);
    ospSetData(volume,      "field",    fieldData);
    ospSetData(volume,      "indices",  indicesData);    
    ospRelease(verticesData);
    ospRelease(fieldData);
    ospRelease(indicesData);
    
    ospSetString(volume,    "hexMethod",  "planar");
    // XXX allow voxelRange to be set in json
    ospSet2f(volume,        "voxelRange", 0.0f, 255.0f);

    ospCommit(volume);
    
    // Note that the volume bounding box is based on the *untransformed*
    // volume, i.e. without applying object2world
    
    bbox[0] = bbox[1] = bbox[2] = 0.0f;
    bbox[3] = dims[0];
    bbox[4] = dims[1];
    bbox[5] = dims[2];
    
    return volume;
}
