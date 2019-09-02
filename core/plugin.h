// ======================================================================== //
// BLOSPRAY - OSPRay as a Blender render engine                             //
// Paul Melis, SURFsara <paul.melis@surfsara.nl>                            //
// Plugin API                                                               //
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

/*
It's a bit tricky to decide what a geometry/volume plugin should return
exactly. Depending on user needs one might want to return just an OSPVolume
and let all the appearance settings get handled on the Blender side. Perhaps 
a plugin might occassionally also want to specify the transfer function used
(which would need to go in the VolumetricModel), but that's mostly it.

For geometry the situation is worse. Our cosmogrid plugin returns a 
"spheres" OSPGeometry and nothing else. But if per-sphere colors would have
been needed, these would have to go in the OSPGeometricModel parameters "prim.color".
So the plugin could either provide the color array only, or provide a complete
OSPGeometry + OSPGeomtricModel setup.

Our rbc plugin returns two OSPGeometry base meshes plus a list of *instances* 
of those meshes, the latter need to be turned into an OSPInstance each (with an
OSPGroup of a single mesh in between :-/). Vertex colors are set on the two base 
geometries. Per-instance colors can not easily be set, other than on two
OSPGeometricModel's, one for each of the two base meshes.

One way to cope with this is to allow the plugin to choose at what
"level" in the scene hierarchy it returns data. A plugin could choose
to return a GeometricModel, in which case the system would need to add
a Group and Instance before adding to the scene. Or a plugin can return a
set of OSPInstance's that can directly be added to the OSPWorld.

This does cause a bit of trouble with handling materials set on the Blender
side, as this can only be applied to an OSPGeometricModel. So if a plugin
returns an OSPInstance there's no easy way to the material lower in the
tree, as no iteration/inspection methods are provided by the OSPRAY API.
It could be overcome by having a plugin method set_material() that
makes the plugin set a specific user-chosen material to the scene part
the plugin defined. But it would also mean having a scene_release()
plugin method to make sure the plugin doesn't hold onto references
that need to be cleared, as plugins now need to be keep track of scene 
elements they generated.

Should a plugin specify materials at all? Or should they always be specified
by the user in Blender, and then passed to the plugin during scene
construction? But what if the material is altered by the user in Blender?

Materials, especially color, should be editable from the Blender side.
A plugin can still ignore any color set from Blender, but applying
user-chosen colors should be the default. The unclear point is how to
handle per-primitive colors (e.g. cosmogrid particles)? A user could
specify a color ramp to use (although the ColorRamp node doesn't allow
values outside the [0,1] range) which then gets applied by the plugin
for mapping values to colors. It would be an option to add OSPRAY-specific
shader nodes, but a lot of work...

Plugins should also handle changes in those parts of the scene they
are responsible for. For example, a user may change a property set 
on a mesh that is handled by a plugin that would the generated
geometry or volume to change. 

A set of event handlers is then needed (XXX still not clear at what
level the plugins generate data)

plugin_loaded
plugin_unloaded

create_geometry
update_geometry
destroy_geometry

create_volume
update_volume
destroy_volume

create_scene
update_scene
destroy_scene
*/

// For scene plugins: one or more transformed OSPGroup's.
// We don't pass OSPInstance's here, as the transformation might need
// to be updated when the corresponding scene element is transformed in
// Blender, as OSPRay currently doesn't support layering instance transforms.

typedef std::pair<OSPGroup, glm::mat4>      GroupInstance;
typedef std::vector<GroupInstance>          GroupInstances;
typedef std::vector<OSPLight>               Lights;

// A plugin can return a bounding mesh, to be used as proxy object
// in the blender scene. The mesh geometry is defined in the same way
// as in Blender: vertices, edges and polygons.

struct BoundingMesh
{
    // Convenience method for constructing an axis-aligned bounding box (edges only)
    static BoundingMesh *bbox_edges(float xmin, float ymin, float zmin, float xmax, float ymax, float zmax);
    static BoundingMesh *bbox_mesh(float xmin, float ymin, float zmin, float xmax, float ymax, float zmax);
    
    // Deserialize
    static BoundingMesh *deserialize(const uint8_t *buffer, uint32_t size);
    
    BoundingMesh();
    ~BoundingMesh();
    
    uint8_t *serialize(uint32_t &size) const;    
    
    std::vector<float>      vertices;       // x, y, z, ...
    std::vector<uint32_t>   edges;          // v0, v1, ...
    std::vector<uint32_t>   faces;          // i, j, k, l, ...
    std::vector<uint32_t>   loop_start;     
    std::vector<uint32_t>   loop_total;     
};

// Yuck
#include "bounding_mesh.impl"

//
// Functions
//
 
// XXX rename, as it is not the state of the plugin, but state of one
// of the "instances" managed by the plugin
struct PluginState
{   
    // Renderer type the plugin is called for
    std::string     renderer;

    // XXX need to store renderer type as well, for scene plugins,  
    // as an OSPGroup can indirectly link to an OSPMaterial (which is 
    // tied to specific renderer type)
    // geometry/volume are renderer-independent, want to keep that
    
    // Custom properties set on the Blender mesh data.
    // XXX Will be updated by the server when needed.
    json            parameters;
    
    // Bounding geometry, may be NULL
    BoundingMesh    *bound;
    
    // Plugin-specific data for this instance, managed by the plugin
    void            *data;        
    
    // Depending on the type of plugin, one of these three must
    // be filled in by the plugin.
    
    // Volume plugin:
    OSPVolume       volume;
    float           volume_data_range[2];
    // XXX could add optional TF
    
    // Geometry plugin:
    OSPGeometry     geometry;    
    
    // Scene plugin:
    GroupInstances  group_instances;    // Need a refcount of at least 1 to survive in the list
    Lights          lights;

    PluginState()
    {
        bound = nullptr;
        data = nullptr;
        volume = nullptr;
        geometry = nullptr;
    }
};


typedef void (*plugin_load_function_t)(
);

typedef void (*plugin_unload_function_t)(    
);

typedef void (*generate_function_t)(
    GenerateFunctionResult &result,
    PluginState *state
);

typedef void (*clear_data_function_t)(
    PluginState *state
);

typedef struct 
{
    // One-time plugin loading/unloading. Both may be NULL.
    plugin_load_function_t      plugin_load_function;
    plugin_unload_function_t    plugin_unload_function;
    
    // Create/destroy the scene element(s) this plugin provides.
    // Depending on the type of plugin the corresponding fields in
    // PluginState must be set.
    // This function may not be NULL.
    generate_function_t         generate_function;    
    
    // Clear any plugin-specific data from PluginState. May be NULL
    clear_data_function_t       clear_data_function;
    
    // Optimization later: allow light-weight updating (if we can
    // reliably detect what exactly changed)
    // Properties were updated, update elements
    //object_update_function_t    object_update_function;
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
    FLAG_SCENE      = 0x3,      // Parameter applies to scene generation            // XXX rename to "groups" plugin, to avoid confusion on the word "scene"?
    
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

enum PluginType
{
    PT_GEOMETRY = 1,
    PT_VOLUME = 2,
    PT_SCENE = 3
};

enum PluginRenderer
{
    PR_ANY = 0,
    PR_SCIVIS,
    PR_PATHTRACER
};

typedef struct
{
    PluginType          type;
    //PluginRenderer      renderer;
    bool                uses_renderer_type;

    PluginParameter     *parameters;
    PluginFunctions     functions;    
}
PluginDefinition;

// XXX how does this relate with plugin_load_function and plugin_unload_function?
typedef bool plugin_initialization_function(PluginDefinition *def);


/*
Some notes regarding our plugins:

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
  A plugin might also get called more than once for different objects
  in the scene. Managing these multiple instances should be handled
  by the server, not the plugin.
  
- Can we get rid of the object2world xform in the API calls? The only
  reason we have them is because a structured volume in ospray can't
  be arbitrarily transformed, which should get fixed at some point.
  Are there other uses for the parameter outside of ospray?

- How to add a framenumber/timestamp parameter to the plugin API?

*/


#endif
