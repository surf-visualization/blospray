#ifndef PLUGIN_H
#define PLUGIN_H

#include <vector>

#include <ospray/ospray.h>

#include "messages.pb.h"        // VolumeLoadResult

using json = nlohmann::json;

typedef std::pair<OSPModel, osp::affine3f>  ModelInstance;
typedef std::vector<ModelInstance>          ModelInstances;

typedef OSPVolume   (*volume_load_function_t)(float *bbox, VolumeLoadResult &result, const json &parameters, const float *object2world);
typedef void        (*geometry_load_function_t)(ModelInstances& model_instances, float *bbox, GeometryLoadResult &result, const json &parameters, const float *object2world);

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
