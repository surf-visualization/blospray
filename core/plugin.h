#ifndef PLUGIN_H
#define PLUGIN_H

#include "messages.pb.h"        // VolumeLoadResult

using json = nlohmann::json;

typedef OSPVolume   (*volume_load_function_t)(float *bbox, VolumeLoadResult &result, const json &parameters, const float *object2world);

typedef struct 
{
    // volume_extent_function
    volume_load_function_t  volume_load_function;
    
    // geometry_load_function
}
Registry;

#endif
