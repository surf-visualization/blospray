#ifndef PLUGIN_H
#define PLUGIN_H

#include <vector>
#include <ospray/ospray.h>
#include <glm/matrix.hpp>

#include "messages.pb.h"        

using json = nlohmann::json;

typedef std::pair<OSPModel, glm::mat4>  ModelInstance;
typedef std::vector<ModelInstance>      ModelInstances;
 
// XXX what are the object2world parameters for? Merely to directly
// transform an unstructured volume ourselves (which OSPRAY doesn't support?)

typedef OSPVolume (*volume_load_function_t)(
    float *bbox, 
    VolumeLoadResult &result, 
    const json &parameters, 
    const glm::mat4 &object2world
);

typedef void (*geometry_load_function_t)(
    ModelInstances& model_instances, 
    float *bbox, 
    GeometryLoadResult &result, 
    const json &parameters, 
    const glm::mat4 &object2world
);

typedef struct 
{
    // Volume plugins
    // volume_extent_function
    volume_load_function_t      volume_load_function;
    
    // Geometry plugins
    geometry_load_function_t    geometry_load_function;
}
Registry;

#endif
