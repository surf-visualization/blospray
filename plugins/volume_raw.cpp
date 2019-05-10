#include <cstdio>
#include <stdint.h>
#include <ospray/ospray.h>
#include "json.hpp"
#include "plugin.h"
#include "util.h"       // float_swap()

// XXX check for existence of "file", "header_skip", etc. entries in parameters[]

static OSPVolume
load_as_structured(float *bbox, LoadFunctionResult &result,
    const json &parameters, const glm::mat4 &/*object2world*/, 
    const int32_t *dims, const std::string& voxelType, OSPDataType dataType, void *grid_field_values)
{
    fprintf(stderr, "WARNING: structured volumes in OSPRay currently don't support object-to-world transformations\n");
    
    OSPVolume volume = ospNewVolume("shared_structured_volume");
    
    OSPData voxelData = ospNewData(dims[0]*dims[1]*dims[2], dataType, grid_field_values, OSP_DATA_SHARED_BUFFER);   
    ospCommit(voxelData);
    
    ospSetData(volume, "voxelData", voxelData);
    ospRelease(voxelData);

    ospSetString(volume, "voxelType", voxelType.c_str());
    ospSet3i(volume, "dimensions", dims[0], dims[1], dims[2]);
    
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
    
    ospSet3f(volume, "gridOrigin", origin[0], origin[1], origin[2]);
    ospSet3f(volume, "gridSpacing", spacing[0], spacing[1], spacing[2]);

    ospCommit(volume);
    
    // XXX in the unstructured load function below we pass the bbox of the UNTRANSFORMED volume
    
    bbox[0] = origin[0];
    bbox[1] = origin[1];
    bbox[2] = origin[2];
    
    bbox[3] = origin[0] + dims[0] * spacing[0];
    bbox[4] = origin[1] + dims[1] * spacing[1];
    bbox[5] = origin[2] + dims[2] * spacing[2];
    
    return volume;
}

static OSPVolume
load_as_unstructured(
    float *bbox, LoadFunctionResult &result,
    const json &parameters, const glm::mat4 &object2world, 
    const int32_t *dims, const std::string& voxelType, OSPDataType dataType, void *grid_field_values)
{    
    if (voxelType != "float")
    {
        fprintf(stderr, "ERROR: OSPRay currently only supports unstructured volumes of 'float', not '%s'\n", voxelType.c_str());
        return NULL;
    }
    
    uint32_t num_grid_points = dims[0] * dims[1] * dims[2];
    uint32_t num_hexahedrons = (dims[0]-1) * (dims[1]-1) * (dims[2]-1);
        
    // Set (transformed) vertices
    
    float       *vertices = new float[num_grid_points*3];
    float       *v;
    float       x, y, z;
    glm::vec4   p;
    
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
                
                p = object2world * glm::vec4(x, y, z, 1);
                
                v[0] = p[0];
                v[1] = p[1];
                v[2] = p[2];
                
                v += 3;
            }
        }
    }
    
    // Set up hexahedral cells
    
    int     *indices = new int[num_hexahedrons*8];   
    int     *hex = indices;
    
    const int ystep = dims[0];
    const int zstep = dims[0] * dims[1];
    
    for (int k = 0; k < dims[2]-1; k++)
    {
        for (int j = 0; j < dims[1]-1; j++)
        {
            for (int i = 0; i < dims[0]-1; i++)
            {
                // VTK_HEXAHEDRON ordering
                
                int baseidx = k * zstep + j * ystep + i;
                
                hex[0] = baseidx;
                hex[1] = baseidx + 1;
                hex[2] = baseidx + ystep + 1;
                hex[3] = baseidx + ystep;
                
                baseidx += zstep;
                hex[4] = baseidx;
                hex[5] = baseidx + 1;
                hex[6] = baseidx + ystep + 1;
                hex[7] = baseidx + ystep;
                
                hex += 8;
            }
        }
    }

    // Set up volume object
    
    // XXX need to look closer at the specific alignment requirements of using OSP_FLOAT3A
    OSPData verticesData = ospNewData(num_grid_points, OSP_FLOAT3, vertices);       
    ospCommit(verticesData);
    
    OSPData fieldData = ospNewData(num_grid_points, dataType, grid_field_values);   
    ospCommit(fieldData);
    
    OSPData indicesData = ospNewData(num_hexahedrons*2, OSP_INT4, indices);
    ospCommit(indicesData);
    
    OSPVolume volume = ospNewVolume("unstructured_volume");
    
        ospSetData(volume, "vertices", verticesData);
        ospRelease(verticesData);
        
        ospSetData(volume, "field", fieldData);
        ospRelease(fieldData);
        
        ospSetData(volume, "indices", indicesData);    
        ospRelease(indicesData);
        
        ospSetString(volume, "hexMethod", "planar");

    ospCommit(volume);
    
    // Note that the volume bounding box is based on the *untransformed*
    // volume, i.e. without applying object2world
    // XXX we ignore gridorigin and gridspacing here, and always assume
    // origin 0,0,0 and spacing 1,1,1
    
    bbox[0] = bbox[1] = bbox[2] = 0.0f;
    bbox[3] = dims[0];
    bbox[4] = dims[1];
    bbox[5] = dims[2];
    
    return volume;    
}


extern "C"
OSPVolume
load(float *bbox, LoadFunctionResult &result, const json &parameters, const glm::mat4 &object2world)
{
    char msg[1024];
        
    if (parameters.find("volume") == parameters.end() || parameters["volume"].get<std::string>() != "raw")
    {
        fprintf(stderr, "WARNING: volume_raw.load() called without property 'volume' set to 'raw'!\n");
    }
    
    // Dimensions
    
    int32_t dims[3];            // XXX why int and not uint?
    uint32_t num_grid_points;
    
    dims[0] = parameters["dimensions"][0];
    dims[1] = parameters["dimensions"][1];
    dims[2] = parameters["dimensions"][2];
    
    num_grid_points = dims[0] * dims[1] * dims[2];
    
    // Open file
    
    std::string fname = parameters["file"].get<std::string>();
    
    FILE *f = fopen(fname.c_str(), "rb");
    if (!f)
    {
        snprintf(msg, 1024, "Could not open file '%s'", fname.c_str());
        result.set_success(false);
        result.set_message(msg);
        fprintf(stderr, "%s\n", msg);
        return NULL;
    }

    // XXX check return value?
    fseek(f, parameters["header_skip"].get<int>(), SEEK_SET);
    
    // Prepare data array and read data from file
    
    OSPDataType dataType;
    void *grid_field_values;
    uint32_t read_size;
    
    std::string voxelType = parameters["voxel_type"].get<std::string>();    // XXX rename parameter?
    
    if (voxelType == "uchar")
    {
        grid_field_values = new uint8_t[num_grid_points];
        dataType = OSP_UCHAR;
        read_size = num_grid_points;
    }
    else if (voxelType == "ushort")
    {
        grid_field_values = new uint16_t[num_grid_points];
        dataType = OSP_USHORT;
        read_size = num_grid_points*2;
    }    
    else if (voxelType == "float")
    {
        grid_field_values = new float[num_grid_points];
        dataType = OSP_FLOAT;
        read_size = num_grid_points*sizeof(float);
    }
    else 
    {
        snprintf(msg, 1024, "ERROR: unhandled voxel data type '%s'!\n", voxelType);
        result.set_success(false);
        result.set_message(msg);
        fprintf(stderr, "%s\n", msg);
        fclose(f);
        return NULL;
    }
    
    // XXX check bytes read
    fread(grid_field_values, 1, read_size, f);   
    
    fclose(f);
    
    // Endian-flip if needed

    if (parameters.find("endian_flip") != parameters.end() && parameters["endian_flip"].get<int>())
    {
        if (voxelType == "float")
        {
            float *falues = (float*)grid_field_values;
            for (int i = 0; i < num_grid_points; i++)
                falues[i] = float_swap(falues[i]);        
        }
        else
        {
            fprintf(stderr, "WARNING: no endian flip available for data type '%s'!\n", voxelType);
        }
    }

    // Set up volume object
    
    OSPVolume volume;
    
    if (parameters.find("make_unstructured") != parameters.end() && parameters["make_unstructured"].get<int>())
    {
        // We support using an unstructured volume for now, as we can transform its
        // vertices with the object2world matrix, as volumes currently don't
        // support affine transformations in ospray themselves.
        volume = load_as_unstructured(  
                    bbox, result,
                    parameters, object2world, 
                    dims, voxelType, dataType, grid_field_values);
    }
    else
    {
        volume = load_as_structured(
                    bbox, result,
                    parameters, object2world, 
                    dims, voxelType, dataType, grid_field_values);
    }
    
    if (!volume)
    {
        fprintf(stderr, "Volume preparation failed!\n");
        return NULL;
    }
    
    if (parameters.find("data_range") != parameters.end())
    {
        float minval = parameters["data_range"][0];
        float maxval = parameters["data_range"][1];
        ospSet2f(volume, "voxelRange", minval, maxval);
    }

    ospCommit(volume);
    
    return volume;
}

// XXX header_skip -> header-skip?
static PluginParameters 
parameters = {
    
    // Name, type, length, flags, description
    {"dimensions",          PARAM_FLOAT,    3, FLAG_VOLUME, 
        "Dimension of the volume in number of voxels per axis"},
        
    {"header_skip",         PARAM_INT,      1, FLAG_VOLUME,//|FLAG_OPTIONAL, 
        "Number of header bytes to skip"},
        
    {"file",                PARAM_STRING,   1, FLAG_VOLUME, 
        "File to read"},
        
    {"voxel_type",          PARAM_STRING,   1, FLAG_VOLUME, 
        "Voxel data type (uchar, float)"},
        
    {"data_range",          PARAM_FLOAT,    2, FLAG_VOLUME, 
        "Data range of the volume"},
        
    {"endian_flip",         PARAM_BOOL,     1, FLAG_VOLUME,//|FLAG_OPTIONAL, 
        "Endian-flip the data during reading"},
        
    {"make_unstructured",   PARAM_BOOL,     1, FLAG_VOLUME,//|FLAG_OPTIONAL, 
        "Create an OSPRay unstructured volume (which can be transformed)"},
        
    PARAMETERS_DONE         // Sentinel (signals end of list)
};

static PluginFunctions
functions = {

    NULL,   // Volume extent
    load,   // Volume load

    NULL    // Geometry extent
};

extern "C" bool
initialize(PluginDefinition *def)
{
    def->parameters = parameters;
    def->functions = functions;
    
    // Do any other plugin-specific initialization here
    
    return true;
}

