// Need to set data_path for the server process before the plugin is used
#include <cstdio>
#include <stdint.h>
#include "uhdf5.h"

#include "json.hpp"
#include "plugin.h"

using json = nlohmann::json;

const char      *data_file;
OSPModel        model;

bool
load_points(const char *fname, int max_points, float sphere_radius)
{
    uint32_t  num_points;
    float     *colors;  
    
    float       *positions;
    uint32_t    *nbcounts;
    
    // Read points
    
    h5::File        file;
    h5::Dataset     *dset;
    h5::Attribute   *attr;
    h5::Type        *type;

    file.open(fname);
    
    // Positions

    dset = file.open_dataset("/positions");

    h5::dimensions dims;
    dset->get_dimensions(dims);
    printf("N=%d: %d, %d\n", dims.size(), dims[0], dims[1]);
    
    num_points = dims[0];

    type = dset->get_type();
    printf("Dataset data class = %d, order = %d, size = %d, precision = %d, signed = %d\n",
        type->get_class(), type->get_order(), type->get_size(), type->get_precision(), type->is_signed());

    if (!type->matches<float>())
    {
        printf("Type doesn't match float!\n");
        // XXX throw error
        exit(-1);
    }

    delete type;

    positions = new float[dims[0]*dims[1]];
    dset->read<float>(positions);
    
    delete dset;
    
    // Counts

    dset = file.open_dataset("/nbcounts");

    type = dset->get_type();
    printf("Dataset data class = %d, order = %d, size = %d, precision = %d, signed = %d\n",
        type->get_class(), type->get_order(), type->get_size(), type->get_precision(), type->is_signed());

    if (!type->matches<uint32_t>())
    {
        printf("Type doesn't match uint32_t!\n");
        // XXX error
        exit(-1);
    }

    delete type;

    nbcounts = new uint32_t[num_points];
    
    dset->read<uint32_t>(nbcounts);
    delete dset;
    
    file.close();
    
#if 0    
    // Set vertex colors
    
    colors = new float[4*num_vertices];
    
    for (int i = 0; i < num_vertices; i++)
    {
        colors[4*i+0] = 1.0f;   //187
        colors[4*i+1] = 0.0f;   //96
        colors[4*i+2] = 0.0f;   //96
        colors[4*i+3] = 1.0f;
    }
#endif

    // Create spheres
    
    OSPGeometry spheres = ospNewGeometry("spheres");
    
      OSPData data = ospNewData(num_points, OSP_FLOAT3, positions);
      ospCommit(data);
      ospSetData(spheres, "spheres", data);
      
      ospSet1i(spheres, "bytes_per_sphere", 3*sizeof(float));
      ospSet1f(spheres, "radius", sphere_radius);

      //data = ospNewData(num_vertices, OSP_FLOAT4, colors);
      //ospCommit(data);
      //ospSetData(mesh, "vertex.color", data);

      OSPMaterial material = ospNewMaterial2("scivis", "OBJMaterial");
      ospSet3f(material, "Kd", 1.0f, 1.0f, 1.0f);
      ospSet1f(material, "d", 1.0f);
      ospCommit(material);
      ospSetMaterial(spheres, material);
      ospRelease(material);

    ospCommit(spheres);
  
    // Create model (for instancing)
    
    model = ospNewModel();
        ospAddGeometry(model, spheres);
    ospCommit(model);
    
    delete [] positions;
    delete [] nbcounts;
        
    return true;
}


extern "C" 
void
load(ModelInstances& model_instances, float *bbox, GeometryLoadResult &result, const json &parameters, const glm::mat4 &object2world)
{    
    data_file = getenv("COSMOGRID_DATA_FILE");
    if (!data_file)
    {
        fprintf(stderr, "ERROR: COSMOGRID_DATA_FILE not set!\n");
        result.set_success(false);
        result.set_message("Environment variable COSMOGRID_DATA_FILE not set!");
        return;
    }
    
    int max_points = -1;
    float sphere_radius = 0.01f;
    
    if (parameters.find("max_points") != parameters.end())
        max_points = parameters["max_points"].get<int>();
    if (parameters.find("sphere_radius") != parameters.end())
        sphere_radius = parameters["sphere_radius"].get<float>();
    
    if (!load_points(data_file, max_points, sphere_radius))
    {
        result.set_success(false);
        result.set_message("Failed to load points from HDF5 file");
        return;
    }
    
    // Add instance
    model_instances.push_back(std::make_pair(model, glm::mat4(1.0f)));
    
    printf("Data loaded...\n");

    bbox[0] = 0.0f;
    bbox[1] = 0.0f;
    bbox[2] = 0.0f;
    
    bbox[3] = 1.0f;
    bbox[4] = 1.0f;
    bbox[5] = 1.0f;
}

Registry    
registry = {

    //NULL,
    NULL,

    load
};

