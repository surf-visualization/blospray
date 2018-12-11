#ifndef PLUGIN_H
#define PLUGIN_H

using json = nlohmann::json;

typedef OSPVolume   (*volume_load_function)(float *bbox, VolumeLoadResult &result, const json &parameters, const float *object2world);

#endif
