#ifndef PLUGIN2_H
#define PLUGIN2_H

#include <vector>
#include <ospray/ospray.h>
#include <ospray/ospray_util.h>
#include <glm/matrix.hpp>
#include "bounding_mesh.h"
#include "util.h"
#include "json.hpp"

using json = nlohmann::json;

// XXX move into Plugin?
typedef std::pair<OSPGroup, glm::mat4>      GroupInstance;
typedef std::vector<GroupInstance>          GroupInstances;
typedef std::vector<OSPLight>               Lights;

// XXX better name
struct PluginResult
{
    bool            success;
    std::string     message;

    PluginResult()
    {
        success = true;
        message = "";
    }

    // XXX replace these two by more informative methods, like
    // generation_failed(), update_failed(), etc
    void set_success(bool s)
    {
        success = s;
    }

    void set_message(const std::string &msg)
    {
        message = msg;
    }
};

//
// Plugin definition (parameters, options, etc)
//

enum PluginType
{
    PT_UNKNOWN = 0,
    PT_GEOMETRY = 1,
    PT_VOLUME = 2,
    PT_SCENE = 3
};

class PluginDefinition
{
public:
    
    /*
    enum Renderer
    {
        PR_ANY = 0,
        PR_SCIVIS,
        PR_PATHTRACER
    };
    */
    
    enum ParameterType
    {  
        PARAM_INT,
        PARAM_FLOAT,
        //PARAM_BOOL,               // XXX disabled for now, as Blender custom properties don't suppport bool values, use integer 0 or 1 instead
        PARAM_STRING,
        PARAM_USER,                 // User-defined, value is passed verbatim as JSON value
        
        PARAM_LAST
    };
    
    // XXX revise the flags stuff
    enum ParameterFlags
    {
        FLAG_NONE       = 0x0,
        FLAG_OPTIONAL   = 0x1,     // Parameter is optional
    };
    
    struct ParameterDefinition
    {
        std::string     name;      
        ParameterType   type;
        int             length;     // XXX check that length > 0
        int             flags;
        std::string     description;
    };
    
public:
    // Note: don't add a constructor/destructor to your user-derived class.
    // Only implement the method(s) below marked as such.
    PluginDefinition();
    ~PluginDefinition();

    void set_type_and_name(PluginType type, const std::string& name);

public:
    // To be overriden by user classes. Set uses_renderer_type and call 
    // add_parameter() as appropriate.
    virtual void initialize() =0;

protected:
    
    // Only to be used from configure()
    void add_parameter(const std::string& name, ParameterType type, int length, 
        int flags, const std::string& description);

protected:
    bool            uses_renderer_type;

    PluginType      type;
    std::string     name;
    std::string     so_name;     // XXX internal_name;
    // Are the generated OSPRay elements dependent on the renderer type (e.g. materials)?

    std::vector<ParameterDefinition>   parameter_definitions;
};

PluginDefinition::PluginDefinition():
    type(PT_UNKNOWN),
    name(""),
    so_name(""),
    uses_renderer_type(true)
{
}

void
PluginDefinition::add_parameter(const std::string& name, ParameterType type, int length, 
    int flags, const std::string& description)
{
    // XXX check length value
    // XXX name should be non-empty
    
    ParameterDefinition pdef;
    pdef.name = name;
    pdef.type = type;
    pdef.length = length;
    pdef.description = description;
    pdef.flags = flags;
    
    // XXX check for already defined param name
    parameter_definitions.push_back(pdef);
}


void 
PluginDefinition::set_type_and_name(PluginType type, const std::string& name)
{
    this->type = type;
    this->name = name;
    
    switch (type)
    {
    case PT_GEOMETRY:
        so_name = "geometry_" + name + ".so";
        break;
    case PT_VOLUME:
        so_name = "volume_" + name + ".so";
        break;
    case PT_SCENE:
        so_name = "scene_" + name + ".so";
        break;
    }
}

//
// An instance of a plugin (base class)
// 

class Plugin
{   
public:
    const static PluginType type = PT_UNKNOWN;

public:
    // Note: don't add a constructor/destructor to your user-derived class.
    // Only implement the methods below marked as such.
    Plugin();
    virtual ~Plugin();

    virtual void configure(PluginDefinition *pdef, const std::string& name);

public:
    // To be implemented by user-written plugin classes

    // Create OSPRay scene elements based on the given parameters
    // Should return true if successful or use set_error() and return false
    // if this failed (for example because a data file could not be opened).
    virtual bool create(const json& parameters) =0;

    // Update the OSPRay elements based on the given updated parameters
    // and return true. If updating is not feasible the method should return false.
    // XXX renderer type changed
    // If not overridden returns false.
    virtual bool update(const json& parameters/*, changes*/);

    // Perform any cleanup the plugin needs to do when an instance of it 
    // is about to be deleted. 
    //
    // Note: you should NOT clean up the generated OSPRay elements that where set,
    // as these are managed automatically. Only cleanup things allocated in
    // create()/update() that blospray doesn't know about.
    virtual void cleanup();

public:
    
    // Used to set the bounding mesh to show in Blender
    void set_bound(BoundingMesh *bound)
    {
        if (this->bound)
            delete this->bound;
        this->bound = bound;
    }

public:

    // For signaling an error during plugin execution. The method were this is
    // used should return false directly after.
    void signal_error(const std::string& message);

    // For producing output during plugin execution
    // XXX return?
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    
protected:
    // Last used parameter values and the hash of the original JSON string before parsing
    json                parameters;
    std::string         parameter_hash;

    // Bounding geometry, may be nullptr
    BoundingMesh        *bound;

    std::string         name;           // Instance name in the scene
    PluginDefinition    *definition;
};

Plugin::Plugin():
    name(""),
    bound(nullptr),
    definition(nullptr)
{
}

Plugin::~Plugin()
{
    if (bound)
        delete bound;
}

void 
Plugin::configure(PluginDefinition *pdef, const std::string& name)
{
    definition = pdef;
    this->name = name;
}

bool
Plugin::update(const json& parameters)
{
    return false;
}

void
Plugin::cleanup()
{
}

//
// Geometry plugin
//

class GeometryPlugin : public Plugin
{
public:
    const static PluginType type = PT_GEOMETRY;

public:
    GeometryPlugin()
    {
        geometry = nullptr;
    }
    
    ~GeometryPlugin()
    {
        if (geometry)
            ospRelease(geometry);
    }
    
public:
    // To be used in user-written plugins
    
    // XXX rename to ProxyMesh?
    void set_geometry(OSPGeometry geometry)
    {
        if (this->geometry)
            ospRelease(this->geometry);
        this->geometry = geometry;
        ospRetain(geometry);
    }
    
protected:
    OSPGeometry     geometry;
};

//
// Volume plugin
//

class VolumePlugin : public Plugin
{
public:
    const static PluginType type = PT_VOLUME;

public:
    VolumePlugin()
    {
        volume = nullptr;
    }
    
    ~VolumePlugin()
    {
        if (volume)
            ospRelease(volume);
    }
    
public:
    // To be used in user-written plugins
    
    void set_volume(OSPVolume volume, float minval, float maxval)
    {
        if (this->volume)
            ospRelease(this->volume);
        this->volume = volume;
        ospRetain(volume);
        
        volume_data_range[0] = minval;
        volume_data_range[1] = maxval;
    }
    
protected:
    OSPVolume       volume;
    float           volume_data_range[2];
    // XXX could add optional TF
};

//
// Scene plugin
//

class ScenePlugin : public Plugin
{
public:
    const static PluginType type = PT_SCENE;

public:
    ScenePlugin()
    {
    }
    
    ~ScenePlugin()
    {
        clear_instances();
        clear_lights();
    }
    
public:
    // To be used in user-written plugins
    
    void add_instance(OSPGroup group, glm::mat4& xform)
    {
        group_instances.push_back(std::make_pair(group, xform));
        ospRetain(group);
    }
    
    // XXX extension: specify transform and transparently transform the light
    void add_light(OSPLight light)
    {
        lights.push_back(light);
        ospRetain(light);
    }
    
    void clear_instances()
    {
        for (auto kv : group_instances)
            ospRelease(kv.first);
    }
    
    void clear_lights()
    {
        for (auto light : lights)
            ospRelease(light);
    }
    
protected:
    GroupInstances  group_instances;
    Lights          lights;
};


#define BLOSPRAY_REGISTER_PLUGIN(plugin_name, def_cls, plugin_cls) \
    \
    extern "C" PluginDefinition* \
    create_definition() \
    { \
        PluginDefinition *definition = new def_cls(); \
        definition->set_type_and_name(plugin_cls::type, #plugin_name); \
        definition->initialize(); \
        return definition; \
    } \
    \
    extern "C" Plugin* \
    create_instance(PluginDefinition *pdef, const char *instance_name) \
    { \
        Plugin *instance = new plugin_cls(); \
        instance->configure(pdef, std::string(instance_name)); \
        return instance; \
    }

// https://stackoverflow.com/questions/9472519/shared-library-constructor-not-working
// Not recommended to use these?
//void func() __attribute__((constructor));
//void func() __attribute__((destructor));



#if 0

class PlaceDefinition : PluginDefinition
{
public:
    void configure()
    {
        uses_renderer_type = true;
        
        add_parameter("file",       PARAM_STRING,   1, FLAG_NONE,       "File");
        add_parameter("box_size",   PARAM_FLOAT,    1, FLAG_NONE,       "Size of the box");
        add_parameter("subset",     PARAM_INT,      1, FLAG_OPTIONAL,   "Generate only a subset of the pixels, [0,subset[ x [0,subset[");
        add_parameter("index",      PARAM_FLOAT,    1, FLAG_NONE,       "Index (normalized) in the sequence at which to extract pixels");  
        add_parameter("use_time",   PARAM_INT,      1, FLAG_OPTIONAL,   "Interpret the index as normalized time");
    }
}

#endif

#endif


