#include <dlfcn.h>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <sys/time.h>       
#include <map>
#include <stdint.h>
//#include <sys/types.h>
//#include <sys/socket.h>
#include <ospray/ospray.h>
#include <json.hpp>

using json = nlohmann::json;

static FILE *log_file = NULL;
// 0 = no, 1 = short arrays, 2 = all
static int dump_arrays = getenv("FAKER_DUMP_ARRAYS") ? atol(getenv("FAKER_DUMP_ARRAYS")) : 0;

typedef std::map<std::string, void*>    PointerMap;

static PointerMap           library_pointers;
static bool                 enum_mapping_initialized=false;

typedef OSPCamera           (*ospNewCamera_ptr)     (const char *type);
typedef OSPData             (*ospNewData_ptr)       (size_t numItems, OSPDataType, const void *source, uint32_t dataCreationFlags);
typedef OSPDevice           (*ospNewDevice_ptr)     (const char *type);
typedef OSPFrameBuffer      (*ospNewFrameBuffer_ptr)(int x, int y, OSPFrameBufferFormat format, uint32_t frameBufferChannels);
typedef OSPGeometricModel   (*ospNewGeometricModel_ptr) (OSPGeometry geometry);
typedef OSPGeometry         (*ospNewGeometry_ptr)   (const char *type);
typedef OSPGroup            (*ospNewGroup_ptr)      ();
typedef OSPInstance         (*ospNewInstance_ptr)   (OSPGroup group);
typedef OSPLight            (*ospNewLight_ptr)      (const char *type);
typedef OSPMaterial         (*ospNewMaterial_ptr)   (const char *rendererType, const char *materialType);
typedef OSPRenderer         (*ospNewRenderer_ptr)    (const char *type);
typedef OSPTexture          (*ospNewTexture_ptr)    (const char *type);
typedef OSPTransferFunction (*ospNewTransferFunction_ptr) (const char *type);
typedef OSPVolume           (*ospNewVolume_ptr)     (const char *type);
typedef OSPVolumetricModel  (*ospNewVolumetricModel_ptr) (OSPVolume volume);
typedef OSPWorld            (*ospNewWorld_ptr)      ();

typedef void                (*ospCommit_ptr)        (OSPObject obj);
typedef void                (*ospRelease_ptr)       (OSPObject obj);

typedef void                (*ospSetData_ptr)       (OSPObject obj, const char *id, OSPData data);
typedef void                (*ospSetBool_ptr)       (OSPObject obj, const char *id, int x);
typedef void                (*ospSetFloat_ptr)      (OSPObject obj, const char *id, float x);
typedef void                (*ospSetInt_ptr)        (OSPObject obj, const char *id, int x);
typedef void                (*ospSetObject_ptr)     (OSPObject obj, const char *id, OSPObject other);
typedef void                (*ospSetString_ptr)     (OSPObject obj, const char *id, const char *s);
typedef void                (*ospSetVoidPtr_ptr)    (OSPObject obj, const char *id, void *v);

typedef void                (*ospSetVec2f_ptr)      (OSPObject obj, const char *id, float x, float y);
typedef void                (*ospSetVec2fv_ptr)     (OSPObject obj, const char *id, const float *xy);
typedef void                (*ospSetVec2i_ptr)      (OSPObject obj, const char *id, int x, int y);
typedef void                (*ospSetVec2iv_ptr)     (OSPObject obj, const char *id, const int *xy);

typedef void                (*ospSetVec3f_ptr)      (OSPObject obj, const char *id, float x, float y, float z);
typedef void                (*ospSetVec3fv_ptr)     (OSPObject obj, const char *id, const float *xyz);
typedef void                (*ospSetVec3i_ptr)      (OSPObject obj, const char *id, int x, int y, int z);
typedef void                (*ospSetVec3iv_ptr)     (OSPObject obj, const char *id, const int *xyz);

typedef void                (*ospSetVec4f_ptr)      (OSPObject obj, const char *id, float x, float y, float z, float w);
typedef void                (*ospSetVec4fv_ptr)     (OSPObject obj, const char *id, const float *xyzw);
typedef void                (*ospSetVec4i_ptr)      (OSPObject obj, const char *id, int x, int y, int z, int w);
typedef void                (*ospSetVec4iv_ptr)     (OSPObject obj, const char *id, const int *xyzw);

typedef void                (*ospSetLinear3fv_ptr)  (OSPObject obj, const char *id, const float *v);
typedef void                (*ospSetAffine3fv_ptr)  (OSPObject obj, const char *id, const float *v);

typedef float               (*ospRenderFrame_ptr)   (OSPFrameBuffer framebuffer, OSPRenderer renderer, OSPCamera camera, OSPWorld world);

static double 
timestamp()
{
    timeval t0;
    gettimeofday(&t0, NULL);

    return t0.tv_sec + t0.tv_usec/1000000.0;
}

static bool
ensure_logfile()
{
    if (log_file == NULL)
        log_file = fopen("faker.log", "wt");
    
    return log_file != NULL;
}

static void
log_json(const json& j)
{
    if (!ensure_logfile()) return;

    fprintf(log_file, "%s\n", j.dump().c_str());
    fflush(log_file);
}

static void
init_enum_mapping()
{
    if (enum_mapping_initialized)
        return;

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "<enums>";

    json ospdatatype_names;
    json ospframebufferformat_names;

    ospdatatype_names["OSP_DEVICE"] = OSP_DEVICE;
    ospdatatype_names["OSP_VOID_PTR"] = OSP_VOID_PTR;
    ospdatatype_names["OSP_OBJECT"] = OSP_OBJECT;
    ospdatatype_names["OSP_CAMERA"] = OSP_CAMERA;
    ospdatatype_names["OSP_DATA"] = OSP_DATA;
    ospdatatype_names["OSP_FRAMEBUFFER"] = OSP_FRAMEBUFFER;
    ospdatatype_names["OSP_GEOMETRY"] = OSP_GEOMETRY;
    ospdatatype_names["OSP_GEOMETRIC_MODEL"] = OSP_GEOMETRIC_MODEL;
    ospdatatype_names["OSP_LIGHT"] = OSP_LIGHT;
    ospdatatype_names["OSP_MATERIAL"] = OSP_MATERIAL;
    ospdatatype_names["OSP_RENDERER"] = OSP_RENDERER;
    ospdatatype_names["OSP_TEXTURE"] = OSP_TEXTURE;
    ospdatatype_names["OSP_TRANSFER_FUNCTION"] = OSP_TRANSFER_FUNCTION;
    ospdatatype_names["OSP_VOLUME"] = OSP_VOLUME;
    ospdatatype_names["OSP_VOLUMETRIC_MODEL"] = OSP_VOLUMETRIC_MODEL;
    ospdatatype_names["OSP_INSTANCE"] = OSP_INSTANCE;
    ospdatatype_names["OSP_WORLD"] = OSP_WORLD;
    ospdatatype_names["OSP_IMAGE_OP"] = OSP_IMAGE_OP;   
    
    ospdatatype_names["OSP_STRING"] = OSP_STRING;
    ospdatatype_names["OSP_CHAR"] = OSP_CHAR;
    ospdatatype_names["OSP_UCHAR"] = OSP_UCHAR;
    ospdatatype_names["OSP_VEC2UC"] = OSP_VEC2UC;
    ospdatatype_names["OSP_VEC3UC"] = OSP_VEC3UC;
    ospdatatype_names["OSP_VEC4UC"] = OSP_VEC4UC;
    ospdatatype_names["OSP_BYTE"] = OSP_BYTE;
    ospdatatype_names["OSP_RAW"] = OSP_RAW;
    ospdatatype_names["OSP_SHORT"] = OSP_SHORT;
    ospdatatype_names["OSP_USHORT"] = OSP_USHORT;
    ospdatatype_names["OSP_INT"] = OSP_INT;
    ospdatatype_names["OSP_VEC2I"] = OSP_VEC2I;
    ospdatatype_names["OSP_VEC3I"] = OSP_VEC3I;
    ospdatatype_names["OSP_VEC4I"] = OSP_VEC4I;
    ospdatatype_names["OSP_UINT"] = OSP_UINT;
    ospdatatype_names["OSP_VEC2UI"] = OSP_VEC2UI;
    ospdatatype_names["OSP_VEC3UI"] = OSP_VEC3UI;
    ospdatatype_names["OSP_VEC4UI"] = OSP_VEC4UI;
    ospdatatype_names["OSP_LONG"] = OSP_LONG;
    ospdatatype_names["OSP_VEC2L"] = OSP_VEC2L;
    ospdatatype_names["OSP_VEC3L"] = OSP_VEC3L;
    ospdatatype_names["OSP_VEC4L"] = OSP_VEC4L;
    ospdatatype_names["OSP_ULONG"] = OSP_ULONG;
    ospdatatype_names["OSP_VEC2UL"] = OSP_VEC2UL;
    ospdatatype_names["OSP_VEC3UL"] = OSP_VEC3UL;
    ospdatatype_names["OSP_VEC4UL"] = OSP_VEC4UL;
    ospdatatype_names["OSP_FLOAT"] = OSP_FLOAT;
    ospdatatype_names["OSP_VEC2F"] = OSP_VEC2F;
    ospdatatype_names["OSP_VEC3F"] = OSP_VEC3F;
    ospdatatype_names["OSP_VEC4F"] = OSP_VEC4F;
    ospdatatype_names["OSP_DOUBLE"] = OSP_DOUBLE;
    ospdatatype_names["OSP_BOX1I"] = OSP_BOX1I;
    ospdatatype_names["OSP_BOX2I"] = OSP_BOX2I;
    ospdatatype_names["OSP_BOX3I"] = OSP_BOX3I;
    ospdatatype_names["OSP_BOX4I"] = OSP_BOX4I;
    ospdatatype_names["OSP_BOX1F"] = OSP_BOX1F;
    ospdatatype_names["OSP_BOX2F"] = OSP_BOX2F;
    ospdatatype_names["OSP_BOX3F"] = OSP_BOX3F;
    ospdatatype_names["OSP_BOX4F"] = OSP_BOX4F;
    ospdatatype_names["OSP_LINEAR2F"] = OSP_LINEAR2F;
    ospdatatype_names["OSP_LINEAR3F"] = OSP_LINEAR3F;
    ospdatatype_names["OSP_AFFINE2F"] = OSP_AFFINE2F;
    ospdatatype_names["OSP_AFFINE3F"] = OSP_AFFINE3F;
    ospdatatype_names["OSP_UNKNOWN"] = OSP_UNKNOWN;

    ospframebufferformat_names["OSP_FB_NONE"] = OSP_FB_NONE;
    ospframebufferformat_names["OSP_FB_RGBA8"] = OSP_FB_RGBA8;
    ospframebufferformat_names["OSP_FB_SRGBA"] = OSP_FB_SRGBA;
    ospframebufferformat_names["OSP_FB_RGBA32F"] = OSP_FB_RGBA32F;

    j["result"] = {
        {"OSPDataType", ospdatatype_names}, {"OSPFrameBufferFormat", ospframebufferformat_names}
    };
    log_json(j);
    
    enum_mapping_initialized = true;
}

#define GET_PTR(call) \
    (call ## _ptr) find_or_load_call(#call)

static void*
find_or_load_call(const char *callname)
{
    PointerMap::iterator it = library_pointers.find(callname);

    if (it != library_pointers.end())
        return it->second;
    
    void *ptr = dlsym(RTLD_NEXT, callname);
    // XXX check ptr
    
    library_pointers[callname] = ptr;
    
    return ptr;
}
    
#define NEW_FUNCTION_1(TYPE) \
    OSP ## TYPE \
    ospNew ## TYPE(const char *type) \
    { \
        ospNew ## TYPE ## _ptr libcall = GET_PTR(ospNew ## TYPE); \
        \
        json j; \
        j["timestamp"] = timestamp(); \
        j["call"] = "ospNew" #TYPE; \
        j["arguments"] = { {"type", type} }; \
        \
        OSP ## TYPE res = libcall(type); \
        \
        j["result"] = (size_t)res; \
        log_json(j); \
        \
        return res; \
    }

NEW_FUNCTION_1(Camera)
NEW_FUNCTION_1(Geometry)
NEW_FUNCTION_1(Light)
NEW_FUNCTION_1(Renderer)
NEW_FUNCTION_1(Texture)
NEW_FUNCTION_1(TransferFunction)
NEW_FUNCTION_1(Volume)

OSPData
ospNewData(size_t numItems, OSPDataType type, const void *source, uint32_t dataCreationFlags)
{
    init_enum_mapping();
    ospNewData_ptr libcall = GET_PTR(ospNewData);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospNewData";
    j["arguments"] = {
        {"numItems", numItems}, {"type", (int)type}, {"source", (size_t)source}, {"dataCreationFlags", dataCreationFlags}
        // XXX source contents
    };
    
    OSPData res = libcall(numItems, type, source, dataCreationFlags);

    /*
    if (dump_arrays > 0)
    {
        int v;
        char f[32];
        std::string s;
        const float *ptr;

        int n = numItems;
        if (dump_arrays == 1)
            n = std::min(n, 30);

        switch (type)
        {
        case OSP_FLOAT:
        case OSP_VEC2F:
        case OSP_VEC3F:
        case OSP_VEC4F:
            v = type - OSP_FLOAT + 1;
            ptr = (float*)source;

            for (size_t i = 0; i < n; i++)
            {
                sprintf(f, "%6d | ", i);
                s = f;
                for (int c = 0; c < v; c++)
                {
                    sprintf(f, "%.6f ", ptr[v*i+c]);
                    s += f;
                }
                s += "\n";
                log_message(s.c_str());
            }
            break;

        case OSP_OBJECT:
            for (size_t i = 0; i < n; i++)
            {
                OSPObject obj = ((OSPObject*)source)[i];
                log_message("%6d | %s\n", i, objinfo(obj).c_str());
            }
            break;
        }

        if (dump_arrays == 1 && numItems > n)
            log_message("...... | ...\n");
    }
    */

    j["result"] = (size_t)res;
    log_json(j);
    
    return res;
}

OSPFrameBuffer 
ospNewFrameBuffer(int x, int y, OSPFrameBufferFormat format, uint32_t frameBufferChannels)
{
    init_enum_mapping();
    ospNewFrameBuffer_ptr libcall = GET_PTR(ospNewFrameBuffer);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospNewFrameBuffer";
    j["arguments"] = {
        {"x", x}, {"y", y}, {"format", int(format)}, {"frameBufferChannels", frameBufferChannels}
    };

    OSPFrameBuffer res = libcall(x, y, format, frameBufferChannels);

    j["result"] = (size_t)res;
    log_json(j);

    return res;
}

OSPGeometricModel 
ospNewGeometricModel(OSPGeometry geometry)
{
    ospNewGeometricModel_ptr libcall = GET_PTR(ospNewGeometricModel);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospNewGeometricModel";
    j["arguments"] = {
        {"geometry", size_t(geometry)}
    };

    OSPGeometricModel res = libcall(geometry);    
    
    j["result"] = (size_t)res;
    log_json(j);
    
    return res;
}

OSPGroup 
ospNewGroup()
{
    ospNewGroup_ptr libcall = GET_PTR(ospNewGroup);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospNewGroup";
    j["arguments"] = {};
    
    OSPGroup res = libcall();    
    
    j["result"] = (size_t)res;
    log_json(j);
    
    return res;
}

OSPInstance 
ospNewInstance(OSPGroup group)
{
    ospNewInstance_ptr libcall = GET_PTR(ospNewInstance);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospNewInstance";
    j["arguments"] = {
        {"group", size_t(group)}
    };

    OSPInstance res = libcall(group);    

    j["result"] = (size_t)res;
    log_json(j);
    
    return res;    
}


OSPMaterial
ospNewMaterial(const char *rendererType, const char *materialType)
{
    ospNewMaterial_ptr libcall = GET_PTR(ospNewMaterial);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospNewMaterial";
    j["arguments"] = {
        {"rendererType", rendererType}, {"materialType", materialType}
    };
    
    OSPMaterial res = libcall(rendererType, materialType);
    
    j["result"] = (size_t)res;
    log_json(j);
    
    return res;
}

OSPVolumetricModel 
ospNewVolumetricModel(OSPVolume volume)
{
    ospNewVolumetricModel_ptr libcall = GET_PTR(ospNewVolumetricModel);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospNewVolumetricModel";
    j["arguments"] = {
        {"volume", (size_t)volume}
    };
    
    OSPVolumetricModel res = libcall(volume);    
    
    j["result"] = (size_t)res;
    log_json(j);
    
    return res;
}


OSPWorld 
ospNewWorld()
{
    ospNewWorld_ptr libcall = GET_PTR(ospNewWorld);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospNewWorld";
    j["arguments"] = {};
        
    OSPWorld res = libcall();    
    
    j["result"] = (size_t)res;
    log_json(j);
    
    return res;
}

void
ospCommit(OSPObject obj)
{
    ospCommit_ptr libcall = GET_PTR(ospCommit);
    
    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospCommit";
    j["arguments"] = {
        {"obj", (size_t)obj}
    };

    libcall(obj);

    log_json(j);
}

void
ospRelease(OSPObject obj)
{
    ospRelease_ptr libcall = GET_PTR(ospRelease);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospRelease";
    j["arguments"] = {
        {"obj", (size_t)obj}
    };
    
    libcall(obj);

    log_json(j);
}

void 
ospSetData(OSPObject obj, const char *id, OSPData data)
{
    ospSetData_ptr libcall = GET_PTR(ospSetData);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetData";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"data", (size_t)data}
    };
    
    libcall(obj, id, data);
    
    log_json(j);
}

void 
ospSetObject(OSPObject obj, const char *id, OSPObject other)
{
    ospSetObject_ptr libcall = GET_PTR(ospSetObject);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetObject";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"other", (size_t)other}
    };
    
    libcall(obj, id, other);

    log_json(j);
}

void 
ospSetBool(OSPObject obj, const char *id, int x)
{
    ospSetBool_ptr libcall = GET_PTR(ospSetBool);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetBool";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"x", x}
    };
    
    libcall(obj, id, x);

    log_json(j);
}

void 
ospSetFloat(OSPObject obj, const char *id, float x)
{
    ospSetFloat_ptr libcall = GET_PTR(ospSetFloat);
 
    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetFloat";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"x", x}
    };

    libcall(obj, id, x);

    log_json(j);
}

void 
ospSetInt(OSPObject obj, const char *id, int x)
{
    ospSetInt_ptr libcall = GET_PTR(ospSetInt);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetInt";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"x", x}
    };
    
    libcall(obj, id, x);

    log_json(j);
}

void 
ospSetLinear3fv(OSPObject obj, const char *id, const float *v)
{
    ospSetLinear3fv_ptr libcall = GET_PTR(ospSetLinear3fv);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetLinear3fv";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"v", (size_t)v}      // XXX
    };
    
    libcall(obj, id, v);    

    log_json(j);
}

void 
ospSetAffine3fv(OSPObject obj, const char *id, const float *v)
{
    ospSetAffine3fv_ptr libcall = GET_PTR(ospSetAffine3fv);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetAffine3fv";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"v", {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11]}}    // XXX pointer
    };
    
    libcall(obj, id, v);       

    log_json(j);
}

void 
ospSetString(OSPObject obj, const char *id, const char *s)
{
    ospSetString_ptr libcall = GET_PTR(ospSetString);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetString";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"s", s}
    };

    libcall(obj, id, s);

    log_json(j);
}

void
ospSetVoidPtr(OSPObject obj, const char *id, void *v)
{
    ospSetVoidPtr_ptr libcall = GET_PTR(ospSetVoidPtr);
 
    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetVoidPtr";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"v", (size_t)v}
    };   

    libcall(obj, id, v);    

    log_json(j);
}

// Vec2
void 
ospSetVec2f(OSPObject obj, const char *id, float x, float y)
{
    ospSetVec2f_ptr libcall = GET_PTR(ospSetVec2f);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetVec2f";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"x", x}, {"y", y}
    };
    
    libcall(obj, id, x, y);

    log_json(j);
}

void 
ospSetVec2fv(OSPObject obj, const char *id, const float *xy)
{
    ospSetVec2fv_ptr libcall = GET_PTR(ospSetVec2fv);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetVec2fv";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"xy", (size_t)xy}, {"xy:values", {xy[0], xy[1]}}
    };
    
    libcall(obj, id, xy);

    log_json(j);
}

void 
ospSetVec2i(OSPObject obj, const char *id, int x, int y)
{
    ospSetVec2i_ptr libcall = GET_PTR(ospSetVec2i);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetVec2i";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"x", x}, {"y", y}
    };
    
    libcall(obj, id, x, y);    

    log_json(j);
}

void 
ospSetVec2iv(OSPObject obj, const char *id, const int *xy)
{
    ospSetVec2iv_ptr libcall = GET_PTR(ospSetVec2iv);
    
    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetVec2iv";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"xy", (size_t)xy}, {"xy:values", {xy[0], xy[1]}}
    };

    libcall(obj, id, xy);

    log_json(j);
}


// Vec3
void 
ospSetVec3f(OSPObject obj, const char *id, float x, float y, float z)
{
    ospSetVec3f_ptr libcall = GET_PTR(ospSetVec3f);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetVec3f";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"x", x}, {"y", y}, {"z", z}
    };
    
    libcall(obj, id, x, y, z);

    log_json(j);
}

void 
ospSetVec3fv(OSPObject obj, const char *id, const float *xyz)
{
    ospSetVec3fv_ptr libcall = GET_PTR(ospSetVec3fv);
    
    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetVec3fv";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"xyz", (size_t)xyz}, {"xyz:values", {xyz[0], xyz[1], xyz[2]}}
    };

    libcall(obj, id, xyz);

    log_json(j);
}

void 
ospSetVec3i(OSPObject obj, const char *id, int x, int y, int z)
{
    ospSetVec3i_ptr libcall = GET_PTR(ospSetVec3i);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetVec3i";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"x", x}, {"y", y}, {"z", z}
    };

    libcall(obj, id, x, y, z);    

    log_json(j);
}

void 
ospSetVec3iv(OSPObject obj, const char *id, const int *xyz)
{
    ospSetVec3iv_ptr libcall = GET_PTR(ospSetVec3iv);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetVec3iv";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"xyz", (size_t)xyz}, {"xyz:values", {xyz[0], xyz[1], xyz[2]}}
    };
    
    libcall(obj, id, xyz);

    log_json(j);
}


// Vec4
void 
ospSetVec4f(OSPObject obj, const char *id, float x, float y, float z, float w)
{
    ospSetVec4f_ptr libcall = GET_PTR(ospSetVec4f);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetVec4f";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"x", x}, {"y", y}, {"z", z}, {"w", w}
    };
    
    libcall(obj, id, x, y, z, w);

    log_json(j);
}

void 
ospSetVec4fv(OSPObject obj, const char *id, const float *xyzw)
{
    ospSetVec4fv_ptr libcall = GET_PTR(ospSetVec4fv);
    
    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetVec4fv";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"xyzw", (size_t)xyzw}, {"xyzw:values", {xyzw[0], xyzw[1], xyzw[2], xyzw[3]}}
    };

    libcall(obj, id, xyzw);

    log_json(j);
}

void 
ospSetVec4i(OSPObject obj, const char *id, int x, int y, int z, int w)
{
    ospSetVec4i_ptr libcall = GET_PTR(ospSetVec4i);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetVec4i";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"x", x}, {"y", y}, {"z", z}, {"w", w}
    };
    
    libcall(obj, id, x, y, z, w);    

    log_json(j);
}

void 
ospSetVec4iv(OSPObject obj, const char *id, const int *xyzw)
{
    ospSetVec4iv_ptr libcall = GET_PTR(ospSetVec4iv);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospSetVec4iv";
    j["arguments"] = {
        {"obj", (size_t)obj}, {"id", id}, {"xyzw", (size_t)xyzw}, {"xyzw:values", {xyzw[0], xyzw[1], xyzw[2], xyzw[3]}}
    };
    
    libcall(obj, id, xyzw);

    log_json(j);
}


float 
ospRenderFrame(OSPFrameBuffer framebuffer, OSPRenderer renderer, OSPCamera camera, OSPWorld world)
{
    ospRenderFrame_ptr libcall = GET_PTR(ospRenderFrame);

    json j;
    j["timestamp"] = timestamp();
    j["call"] = "ospRenderFrame";
    j["arguments"] = {
        {"framebuffer", (size_t)framebuffer}, {"renderer", (size_t)renderer}, {"camera", (size_t)camera}, {"world", (size_t)world}
    };

    float res = libcall(framebuffer, renderer, camera, world);

    j["result"] = res;      // XXX inf gets turned into null in json 
    log_json(j);

    return res;
}

/* 
  OSPRAY_INTERFACE void ospSetBox1f(OSPObject, const char *id, float lower_x, float upper_x);
  OSPRAY_INTERFACE void ospSetBox1fv(OSPObject, const char *id, const float *lower_x_upper_x);
  OSPRAY_INTERFACE void ospSetBox1i(OSPObject, const char *id, int lower_x, int upper_x);
  OSPRAY_INTERFACE void ospSetBox1iv(OSPObject, const char *id, const int *lower_x_upper_x);

  OSPRAY_INTERFACE void ospSetBox2f(OSPObject, const char *id, float lower_x, float lower_y, float upper_x, float upper_y);
  OSPRAY_INTERFACE void ospSetBox2fv(OSPObject, const char *id, const float *lower_xy_upper_xy);
  OSPRAY_INTERFACE void ospSetBox2i(OSPObject, const char *id, int lower_x, int lower_y, int upper_x, int upper_y);
  OSPRAY_INTERFACE void ospSetBox2iv(OSPObject, const char *id, const int *lower_xy_upper_xy);

  OSPRAY_INTERFACE void ospSetBox3f(OSPObject, const char *id, float lower_x, float lower_y, float lower_z, float upper_x, float upper_y, float upper_z);
  OSPRAY_INTERFACE void ospSetBox3fv(OSPObject, const char *id, const float *lower_xyz_upper_xyz);
  OSPRAY_INTERFACE void ospSetBox3i(OSPObject, const char *id, int lower_x, int lower_y, int lower_z, int upper_x, int upper_y, int upper_z);
  OSPRAY_INTERFACE void ospSetBox3iv(OSPObject, const char *id, const int *lower_xyz_upper_xyz);

  OSPRAY_INTERFACE void ospSetBox4f(OSPObject, const char *id, float lower_x, float lower_y, float lower_z, float lower_w, float upper_x, float upper_y, float upper_z, float upper_w);
  OSPRAY_INTERFACE void ospSetBox4fv(OSPObject, const char *id, const float *lower_xyzw_upper_xyzw);
  OSPRAY_INTERFACE void ospSetBox4i(OSPObject, const char *id, int lower_x, int lower_y, int lower_z, int lower_w, int upper_x, int upper_y, int upper_z, int upper_w);
  OSPRAY_INTERFACE void ospSetBox4iv(OSPObject, const char *id, const int *lower_xyzw_upper_xyzw);
*/

