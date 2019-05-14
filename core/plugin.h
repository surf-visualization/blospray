// ======================================================================== //
// BLOSPRAY - OSPRay as a Blender render engine                             //
// Paul Melis, SURFsara <paul.melis@surfsara.nl>                            //
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

#ifndef PLUGIN_H
#define PLUGIN_H

#include <vector>
#include <ospray/ospray.h>
#include <glm/matrix.hpp>

#include "messages.pb.h"        
#include "json.hpp"

using json = nlohmann::json;

typedef std::pair<OSPModel, glm::mat4>  ModelInstance;
typedef std::vector<ModelInstance>      ModelInstances;

//
// Functions
//
 
// XXX what are the object2world parameters for? Merely to directly
// transform an unstructured volume ourselves (which OSPRAY doesn't support?)

// Functions should set
// - bbox (min * 3, max * 3)
// - result to false and set appropriate message. return false resp. NULL

typedef OSPVolume (*volume_load_function_t)(
    float *bbox, 
    float *data_range,
    LoadFunctionResult &result, 
    const json &parameters, 
    const glm::mat4 &object2world
);

typedef bool (*volume_extent_function_t)(
    float *bbox, 
    LoadFunctionResult &result, 
    const json &parameters, 
    const glm::mat4 &object2world
);

typedef void (*geometry_load_function_t)(
    ModelInstances& model_instances, 
    float *bbox, 
    LoadFunctionResult &result, 
    const json &parameters, 
    const glm::mat4 &object2world
);

typedef struct 
{
    // Volume plugins
    volume_extent_function_t    volume_extent_function;
    volume_load_function_t      volume_load_function;
    
    // Geometry plugins
    geometry_load_function_t    geometry_load_function;
    
    
    
    //compute_volume_bound_function    -> bbox or mesh
    //generate_volume_function -> OSPVolume
    
    //compute_geometry_bound_function -> bbox or mesh
    //generate_geometry_function -> list of (OSPModel, xform)
}
PluginFunctions;

//
// Parameters
//

enum ParameterType
{  
    PARAM_INT,
    PARAM_FLOAT,
    //PARAM_BOOL,               // XXX disabled for now, as Blender custom properties don't suppport bool values, use integer 0 or 1 instead
    PARAM_STRING,
    PARAM_USER,                 // User-defined, value is passed verbatim as JSON value
    
    PARAM_LAST
};

enum ParameterFlags
{
    FLAG_NONE       = 0x0,
    
    FLAG_VOLUME     = 0x1,      // Parameter applies to volume generation
    FLAG_GEOMETRY   = 0x2,      // Parameter applies to geometry generation
    
    //FLAG_OPTIONAL   = 0x10,     // Parameter is optional
};


// List of parameters understood by the plugin. This is both for
// documentation for users of the plugin, as well as doing checks
// on parameters to/by the plugin.
// XXX how about using a piece of json text to specify these parameters?
// pros: easy to write, can send directly to client, can easily include default values
// cons: would need something like c++11 raw strings to easily include in the source code (or keep them in a separate file, but that's nasty)

// XXX don't need the typedef in C++?
typedef struct 
{
    const char      *name;
    ParameterType   type;
    int             length;     // XXX check that length > 0
    int             flags;
    const char      *description;
} 
PluginParameter;

typedef PluginParameter PluginParameters[];

#define PARAMETERS_DONE     {NULL, PARAM_LAST, 0, FLAG_NONE, NULL}      

//
// Initialization
//

typedef struct
{
    PluginParameter     *parameters;
    PluginFunctions     functions;
}
PluginDefinition;

typedef bool plugin_initialization_function(PluginDefinition *def);


/*
Some notes regarding our plugins:

- Need to pick a better name for Registry, how about PluginFunctions

- Look in more detail into improving error handling and return.
  VolumeLoadResult and GeometryLoadResult return the exact same
  lvalues.

- Do we really need to distinguish between volume and geometry plugins?
  Obviously they generate a different kind of scene element, so we can't
  use the same load() calls. But is there a need to have separate 
  volume_XXX.so and geometry_XXX.so types of libs? We could use only a
  a single blospray_XXX.so type library and have the plugin specify wether
  it generates geometry, volume or both and fill in the corresponding
  function in the registry
  How often does it occur that a plugin would generate both geometry
  and volumes *from the same input data*?

- How feasible is it for a plugin to compute a bound on the data without
  actually loading that data? Or you can derive the bound from the 
  plugin parameters then it's easy, or if you can read only a file 
  header to determine volume extents. Another option would be to give
  the plugin the option not to specify the extent and represent that
  on the Blender side with some default geometry (e.g. a sphere at the
  location of the data). If the extent becomes available after the
  actual loading of the data it can then be set later in Blender.
  The plugin could return a loose bound in reponse to an extent()
  query while returning a tight bound for load(), which would then update
  the bound geometry in Blender
  
- Should we store any state in plugins or only pass it through the
  parameters? Plugins only work like functions in the current scheme in that they
  provide data when called that gets owned and managed by the server. 
  
- Can we get rid of the object2world xform in the API calls? The only
  reason we have them is because a structured volume in ospray can't
  be arbitrarily transformed, which should get fixed at some point.
  Are there other uses for the parameter outside of ospray?

- Should we add a framenumber/timestamp parameter to the plugin API?

*/


#endif
