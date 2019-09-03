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

static FILE *log_file = NULL;
// 0 = no, 1 = short arrays, 2 = all
static int dump_arrays = getenv("FAKER_DUMP_ARRAYS") ? atol(getenv("FAKER_DUMP_ARRAYS")) : 0;

typedef std::map<std::string, void*>    PointerMap;
typedef std::map<void*, int>            ReferenceCountMap;
typedef std::map<void*, std::string>    ReferenceTypeMap;

typedef std::map<OSPDataType, std::string>  OSPDataTypeMap;
typedef std::map<OSPFrameBufferFormat, std::string>  OSPFrameBufferFormatMap;

static PointerMap          library_pointers;
static ReferenceCountMap   reference_counts;
static ReferenceTypeMap    reference_types;

static OSPDataTypeMap           ospdatatype_names;
static OSPFrameBufferFormatMap  ospframebufferformat_names;
static bool                     enum_mapping_initialized=false;

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

static void
init_enum_mapping()
{
    ospdatatype_names[OSP_DEVICE] = "OSP_DEVICE";
    ospdatatype_names[OSP_VOID_PTR] = "OSP_VOID_PTR";
    ospdatatype_names[OSP_OBJECT] = "OSP_OBJECT";
    ospdatatype_names[OSP_CAMERA] = "OSP_CAMERA";
    ospdatatype_names[OSP_DATA] = "OSP_DATA";
    ospdatatype_names[OSP_FRAMEBUFFER] = "OSP_FRAMEBUFFER";
    ospdatatype_names[OSP_GEOMETRY] = "OSP_GEOMETRY";
    ospdatatype_names[OSP_GEOMETRIC_MODEL] = "OSP_GEOMETRIC_MODEL";
    ospdatatype_names[OSP_LIGHT] = "OSP_LIGHT";
    ospdatatype_names[OSP_MATERIAL] = "OSP_MATERIAL";
    ospdatatype_names[OSP_RENDERER] = "OSP_RENDERER";
    ospdatatype_names[OSP_TEXTURE] = "OSP_TEXTURE";
    ospdatatype_names[OSP_TRANSFER_FUNCTION] = "OSP_TRANSFER_FUNCTION";
    ospdatatype_names[OSP_VOLUME] = "OSP_VOLUME";
    ospdatatype_names[OSP_VOLUMETRIC_MODEL] = "OSP_VOLUMETRIC_MODEL";
    ospdatatype_names[OSP_INSTANCE] = "OSP_INSTANCE";
    ospdatatype_names[OSP_WORLD] = "OSP_WORLD";
    ospdatatype_names[OSP_IMAGE_OP] = "OSP_IMAGE_OP";   
    
    ospdatatype_names[OSP_STRING] = "OSP_STRING";
    ospdatatype_names[OSP_CHAR] = "OSP_CHAR";
    ospdatatype_names[OSP_UCHAR] = "OSP_UCHAR";
    ospdatatype_names[OSP_VEC2UC] = "OSP_VEC2UC";
    ospdatatype_names[OSP_VEC3UC] = "OSP_VEC3UC";
    ospdatatype_names[OSP_VEC4UC] = "OSP_VEC4UC";
    ospdatatype_names[OSP_BYTE] = "OSP_BYTE";
    ospdatatype_names[OSP_RAW] = "OSP_RAW";
    ospdatatype_names[OSP_SHORT] = "OSP_SHORT";
    ospdatatype_names[OSP_USHORT] = "OSP_USHORT";
    ospdatatype_names[OSP_INT] = "OSP_INT";
    ospdatatype_names[OSP_VEC2I] = "OSP_VEC2I";
    ospdatatype_names[OSP_VEC3I] = "OSP_VEC3I";
    ospdatatype_names[OSP_VEC4I] = "OSP_VEC4I";
    ospdatatype_names[OSP_UINT] = "OSP_UINT";
    ospdatatype_names[OSP_VEC2UI] = "OSP_VEC2UI";
    ospdatatype_names[OSP_VEC3UI] = "OSP_VEC3UI";
    ospdatatype_names[OSP_VEC4UI] = "OSP_VEC4UI";
    ospdatatype_names[OSP_LONG] = "OSP_LONG";
    ospdatatype_names[OSP_VEC2L] = "OSP_VEC2L";
    ospdatatype_names[OSP_VEC3L] = "OSP_VEC3L";
    ospdatatype_names[OSP_VEC4L] = "OSP_VEC4L";
    ospdatatype_names[OSP_ULONG] = "OSP_ULONG";
    ospdatatype_names[OSP_VEC2UL] = "OSP_VEC2UL";
    ospdatatype_names[OSP_VEC3UL] = "OSP_VEC3UL";
    ospdatatype_names[OSP_VEC4UL] = "OSP_VEC4UL";
    ospdatatype_names[OSP_FLOAT] = "OSP_FLOAT";
    ospdatatype_names[OSP_VEC2F] = "OSP_VEC2F";
    ospdatatype_names[OSP_VEC3F] = "OSP_VEC3F";
    ospdatatype_names[OSP_VEC4F] = "OSP_VEC4F";
    ospdatatype_names[OSP_DOUBLE] = "OSP_DOUBLE";
    ospdatatype_names[OSP_BOX1I] = "OSP_BOX1I";
    ospdatatype_names[OSP_BOX2I] = "OSP_BOX2I";
    ospdatatype_names[OSP_BOX3I] = "OSP_BOX3I";
    ospdatatype_names[OSP_BOX4I] = "OSP_BOX4I";
    ospdatatype_names[OSP_BOX1F] = "OSP_BOX1F";
    ospdatatype_names[OSP_BOX2F] = "OSP_BOX2F";
    ospdatatype_names[OSP_BOX3F] = "OSP_BOX3F";
    ospdatatype_names[OSP_BOX4F] = "OSP_BOX4F";
    ospdatatype_names[OSP_LINEAR2F] = "OSP_LINEAR2F";
    ospdatatype_names[OSP_LINEAR3F] = "OSP_LINEAR3F";
    ospdatatype_names[OSP_AFFINE2F] = "OSP_AFFINE2F";
    ospdatatype_names[OSP_AFFINE3F] = "OSP_AFFINE3F";
    ospdatatype_names[OSP_UNKNOWN] = "OSP_UNKNOWN";

    ospframebufferformat_names[OSP_FB_NONE] = "OSP_FB_NONE";
    ospframebufferformat_names[OSP_FB_RGBA8] = "OSP_FB_RGBA8";
    ospframebufferformat_names[OSP_FB_SRGBA] = "OSP_FB_SRGBA";
    ospframebufferformat_names[OSP_FB_RGBA32F] = "OSP_FB_RGBA32F";
    
    enum_mapping_initialized = true;
}

static std::string
ospdatatype_name(OSPDataType type)
{
    if (!enum_mapping_initialized)
        init_enum_mapping();
    
    return ospdatatype_names[type];
}

static std::string
ospframebufferformat_name(OSPFrameBufferFormat type)
{
    if (!enum_mapping_initialized)
        init_enum_mapping();
    
    return ospframebufferformat_names[type];
}

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
log_message(const char *fmt, ...)
{
    static char msg[1024];

    if (!ensure_logfile()) return;

    va_list arglist;
    va_start(arglist, fmt);
    vsprintf(msg, fmt, arglist);
    va_end(arglist);

    fprintf(log_file, "[%.06lf] %s", timestamp(), msg);
    fflush(log_file);
}


static void
log_warning(const char *fmt, ...)
{
    static char msg[1024];
    
    if (!ensure_logfile()) return;

    va_list arglist;
    va_start(arglist, fmt);
    vsprintf(msg, fmt, arglist);
    va_end(arglist);

    fprintf(log_file, "[%.06lf] WARNING: %s", timestamp(), msg);
    fflush(log_file);
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

typedef std::map<std::string, int>  TypeCountMap;

TypeCountMap    type_counts;

void
newobj(void *ptr, const std::string &type)
{
    int count; 

    TypeCountMap::iterator it = type_counts.find(type);
    if (it == type_counts.end())
        count = type_counts[type] = 1;
    else
        count = it->second++;
        
    reference_counts[ptr] = 1;
    reference_types[ptr] = type + "#" + std::to_string(count);
}

std::string 
objinfo(void *ptr)
{
    if (reference_types.find(ptr) == reference_types.end())
        return std::string("???");
    
    char s[256];
    sprintf(s, "%s:%d", reference_types[ptr].c_str(), reference_counts[ptr]);
    
    return std::string(s);
}
    
#define NEW_FUNCTION_1(TYPE) \
    OSP ## TYPE \
    ospNew ## TYPE(const char *type) \
    { \
        ospNew ## TYPE ## _ptr libcall = GET_PTR(ospNew ## TYPE); \
        \
        log_message("ospNew" #TYPE "(type=\"%s\")\n", type); \
        \
        OSP ## TYPE res = libcall(type); \
        \
        if (reference_counts.find(res) != reference_counts.end()) \
            log_warning("Lost reference count to object at 0x%016x!\n", res); \
        \
        newobj(res, "OSP" #TYPE); \
        \
        log_message("-> 0x%016x [%s]\n", res, objinfo(res).c_str()); \
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
    ospNewData_ptr libcall = GET_PTR(ospNewData);
    
    log_message("ospNewData(numItems=%ld, type=%d [%s], source=0x%016x, dataCreationFlags=0x%08x)\n", 
        numItems, type, ospdatatype_name(type).c_str(), source, dataCreationFlags);    
    
    OSPData res = libcall(numItems, type, source, dataCreationFlags);

    newobj(res, "OSPData");

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

    log_message("-> 0x%016x [%s]\n", res, objinfo(res).c_str());
    
    return res;
}

OSPFrameBuffer 
ospNewFrameBuffer(int x, int y, OSPFrameBufferFormat format, uint32_t frameBufferChannels)
{
    ospNewFrameBuffer_ptr libcall = GET_PTR(ospNewFrameBuffer);

    log_message("ospNewFrameBuffer(x=%d, y=%d, format=0x%04x [%s], frameBufferChannels=0x%08x)\n", 
        x, y, format, ospframebufferformat_name(format).c_str(), frameBufferChannels);    
    
    OSPFrameBuffer res = libcall(x, y, format, frameBufferChannels);
    
    newobj(res, "OSPFrameBuffer");

    log_message("-> 0x%016x [%s]\n", res, objinfo(res).c_str());

    return res;
}

OSPGeometricModel 
ospNewGeometricModel(OSPGeometry geometry)
{
    ospNewGeometricModel_ptr libcall = GET_PTR(ospNewGeometricModel);
    
    log_message("ospNewGeometricModel(geometry=0x%016x [%s])\n", geometry, objinfo(geometry).c_str());
    
    OSPGeometricModel res = libcall(geometry);    
    
    if (reference_counts.find(res) != reference_counts.end())
        log_warning("Lost reference count to object at 0x%016x!\n", res);

    newobj(res, "OSPGeometricModel");
    reference_counts[geometry] += 1;
    
    log_message("-> 0x%016x [%s]\n", res, objinfo(res).c_str());
    
    return res;
}

OSPGroup 
ospNewGroup()
{
    ospNewGroup_ptr libcall = GET_PTR(ospNewGroup);
    
    log_message("ospNewGroup()\n");    
    
    OSPGroup res = libcall();    
    
    if (reference_counts.find(res) != reference_counts.end())
        log_warning("Lost reference count to object at 0x%016x!\n", res);

    newobj(res, "OSPGroup");

    log_message("-> 0x%016x [%s]\n", res, objinfo(res).c_str());
    
    return res;
}

OSPInstance 
ospNewInstance(OSPGroup group)
{
    ospNewInstance_ptr libcall = GET_PTR(ospNewInstance);
    
    log_message("OSPInstance(group=0x%016x [%s])\n", group, objinfo(group).c_str());
    
    OSPInstance res = libcall(group);    
    
    if (reference_counts.find(res) != reference_counts.end())
        log_warning("Lost reference count to object at 0x%016x!\n", res);

    newobj(res, "OSPInstance");
    reference_counts[group] += 1;
    
    log_message("-> 0x%016x [%s]\n", res, objinfo(res).c_str());
    
    return res;    
}


OSPMaterial
ospNewMaterial(const char *rendererType, const char *materialType)
{
    ospNewMaterial_ptr libcall = GET_PTR(ospNewMaterial);
    
    log_message("ospNewMaterial(rendererType=\"%s\", materialType=\"%s\")\n", 
        rendererType, materialType);    
    
    OSPMaterial res = libcall(rendererType, materialType);
    
    if (reference_counts.find(res) != reference_counts.end())
        log_warning("Lost reference count to object at 0x%016x!\n", res);

    newobj(res, "OSPMaterial");
    
    log_message("-> 0x%016x [%s]\n", res, objinfo(res).c_str());
    
    return res;
}

OSPVolumetricModel 
ospNewVolumetricModel(OSPVolume volume)
{
    ospNewVolumetricModel_ptr libcall = GET_PTR(ospNewVolumetricModel);
    
    log_message("ospNewVolumetricModel(volume=0x%016x [%s])\n", volume, objinfo(volume).c_str());
    
    OSPVolumetricModel res = libcall(volume);    
    
    if (reference_counts.find(res) != reference_counts.end())
        log_warning("Lost reference count to object at 0x%016x!\n", res);

    newobj(res, "OSPVolumetricModel");
    reference_counts[volume] += 1;
    
    log_message("-> 0x%016x [%s]\n", res, objinfo(res).c_str());
    
    return res;
}


OSPWorld 
ospNewWorld()
{
    ospNewWorld_ptr libcall = GET_PTR(ospNewWorld);
    
    log_message("ospNewWorld()\n");    
    
    OSPWorld res = libcall();    
    
    if (reference_counts.find(res) != reference_counts.end())
        log_warning("Lost reference count to object at 0x%016x!\n", res);

    newobj(res, "OSPWorld");
    
    log_message("-> 0x%016x [%s]\n", res, objinfo(res).c_str());
    
    return res;
}

void
ospCommit(OSPObject obj)
{
    ospCommit_ptr libcall = GET_PTR(ospCommit);
    
    log_message("ospCommit(object=0x%016x [%s])\n", obj, objinfo(obj).c_str());
    
    libcall(obj);
}

void
ospRelease(OSPObject obj)
{
    ospRelease_ptr libcall = GET_PTR(ospRelease);
    
    log_message("ospRelease(object=0x%016x [%s])\n", obj, objinfo(obj).c_str());
    
    bool found = true;
    
    if (reference_counts.find(obj) == reference_counts.end())
    {
        found = false;
        log_warning("No previous reference count for 0x%016x!\n", obj);
    }

    libcall(obj);
    
    //log_message("Refcount is now %lld\n", ospRefCount(obj));
}

void ospSetData(OSPObject obj, const char *id, OSPData data)
{
    ospSetData_ptr libcall = GET_PTR(ospSetData);
    
    log_message("ospSetData(object=0x%016x [%s], id=\"%s\", data=0x%016x [%s])\n", 
        obj, objinfo(obj).c_str(), id, data, objinfo(data).c_str());

    libcall(obj, id, data);
    
    reference_counts[data] += 1;
}

void 
ospSetObject(OSPObject obj, const char *id, OSPObject other)
{
    ospSetObject_ptr libcall = GET_PTR(ospSetObject);
    
    log_message("ospSetObject(object=0x%016x [%s], id=\"%s\", other=0x%016x [%s])\n", 
        obj, objinfo(obj).c_str(), id, other, objinfo(other).c_str());

    libcall(obj, id, other);
    
    reference_counts[other] += 1;
}

void 
ospSetBool(OSPObject obj, const char *id, int x)
{
    ospSetBool_ptr libcall = GET_PTR(ospSetBool);
    
    log_message("ospSetBool(object=0x%016x [%s], id=\"%s\", x=%d)\n", 
        obj, objinfo(obj).c_str(), id, x);

    libcall(obj, id, x);
}

void 
ospSetFloat(OSPObject obj, const char *id, float x)
{
    ospSetFloat_ptr libcall = GET_PTR(ospSetFloat);
    
    log_message("ospSetFloat(object=0x%016x [%s], id=\"%s\", x=%.6f)\n", 
        obj, objinfo(obj).c_str(), id, x);

    libcall(obj, id, x);
}

void 
ospSetInt(OSPObject obj, const char *id, int x)
{
    ospSetInt_ptr libcall = GET_PTR(ospSetInt);
    
    log_message("ospSetInt(object=0x%016x [%s], id=\"%s\", x=%d)\n", 
        obj, objinfo(obj).c_str(), id, x);

    libcall(obj, id, x);
}

void 
ospSetLinear3fv(OSPObject obj, const char *id, const float *v)
{
    ospSetLinear3fv_ptr libcall = GET_PTR(ospSetLinear3fv);
    
    log_message("ospSetLinear3fv(object=0x%016x [%s], id=\"%s\", v=0x%016x [%.6f])\n", 
        obj, objinfo(obj).c_str(), id, v);

    libcall(obj, id, v);    
}

void 
ospSetAffine3fv(OSPObject obj, const char *id, const float *v)
{
    ospSetAffine3fv_ptr libcall = GET_PTR(ospSetAffine3fv);
    
    char msg[1024];
    
    log_message("ospSetAffine3fv(object=0x%016x [%s], id=\"%s\", v=0x%016x [%.6f, %.6f, %.6f | %.6f, %.6f, %.6f | %.6f, %.6f, %.6f | %.6f, %.6f, %.6f ])\n", 
        obj, objinfo(obj).c_str(), id, v,
        v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11]);

    libcall(obj, id, v);       
}

void 
ospSetString(OSPObject obj, const char *id, const char *s)
{
    ospSetString_ptr libcall = GET_PTR(ospSetString);
    
    log_message("ospSetString(object=0x%016x [%s], id=\"%s\", s=\"%s\")\n", 
        obj, objinfo(obj).c_str(), id, s);

    libcall(obj, id, s);
}

void
ospSetVoidPtr(OSPObject obj, const char *id, void *v)
{
    ospSetVoidPtr_ptr libcall = GET_PTR(ospSetVoidPtr);
    
    log_message("ospSetVoidPtr(object=0x%016x [%s], id=\"%s\", v=0x%016x [%s])\n", 
        obj, objinfo(obj).c_str(), id, v, objinfo(v));

    libcall(obj, id, v);    
}

// Vec2
void 
ospSetVec2f(OSPObject obj, const char *id, float x, float y)
{
    ospSetVec2f_ptr libcall = GET_PTR(ospSetVec2f);
    
    log_message("ospSetVec2f(object=0x%016x [%s], id=\"%s\", x=%.6f, y=%.6f)\n", 
        obj, objinfo(obj).c_str(), id, x, y);

    libcall(obj, id, x, y);
}

void 
ospSetVec2fv(OSPObject obj, const char *id, const float *xy)
{
    ospSetVec2fv_ptr libcall = GET_PTR(ospSetVec2fv);
    
    log_message("ospSetVec2fv(object=0x%016x [%s], id=\"%s\", xy=0x%016x [%.6f, %.6f])\n", 
        obj, objinfo(obj).c_str(), id, xy, xy[0], xy[1]);

    libcall(obj, id, xy);
}

void 
ospSetVec2i(OSPObject obj, const char *id, int x, int y)
{
    ospSetVec2i_ptr libcall = GET_PTR(ospSetVec2i);
    
    log_message("ospSetVec2i(object=0x%016x [%s], id=\"%s\", x=%d, y=%d)\n", 
        obj, objinfo(obj).c_str(), id, x, y);

    libcall(obj, id, x, y);    
}

void 
ospSetVec2iv(OSPObject obj, const char *id, const int *xy)
{
    ospSetVec2iv_ptr libcall = GET_PTR(ospSetVec2iv);
    
    log_message("ospSetVec2fv(object=0x%016x [%s], id=\"%s\", xy=0x%016x [%d, %d])\n", 
        obj, objinfo(obj).c_str(), id, xy, xy[0], xy[1]);

    libcall(obj, id, xy);
}


// Vec3
void 
ospSetVec3f(OSPObject obj, const char *id, float x, float y, float z)
{
    ospSetVec3f_ptr libcall = GET_PTR(ospSetVec3f);
    
    log_message("ospSetVec3f(object=0x%016x [%s], id=\"%s\", x=%.6f, y=%.6f, z=%.6f)\n", 
        obj, objinfo(obj).c_str(), id, x, y, z);

    libcall(obj, id, x, y, z);
}

void 
ospSetVec3fv(OSPObject obj, const char *id, const float *xyz)
{
    ospSetVec3fv_ptr libcall = GET_PTR(ospSetVec3fv);
    
    log_message("ospSetVec3fv(object=0x%016x [%s], id=\"%s\", xyz=0x%016x [%.6f, %.6f, %.6f])\n", 
        obj, objinfo(obj).c_str(), id, xyz, xyz[0], xyz[1], xyz[2]);

    libcall(obj, id, xyz);
}

void 
ospSetVec3i(OSPObject obj, const char *id, int x, int y, int z)
{
    ospSetVec3i_ptr libcall = GET_PTR(ospSetVec3i);
    
    log_message("ospSetVec3i(object=0x%016x [%s], id=\"%s\", x=%d, y=%d, z=%d)\n", 
        obj, objinfo(obj).c_str(), id, x, y, z);

    libcall(obj, id, x, y, z);    
}

void 
ospSetVec3iv(OSPObject obj, const char *id, const int *xyz)
{
    ospSetVec3iv_ptr libcall = GET_PTR(ospSetVec3iv);
    
    log_message("ospSetVec3fv(object=0x%016x [%s], id=\"%s\", xyz=0x%016x [%d, %d, %d])\n", 
        obj, objinfo(obj).c_str(), id, xyz, xyz[0], xyz[1], xyz[2]);

    libcall(obj, id, xyz);
}


// Vec4
void 
ospSetVec4f(OSPObject obj, const char *id, float x, float y, float z, float w)
{
    ospSetVec4f_ptr libcall = GET_PTR(ospSetVec4f);
    
    log_message("ospSetVec4f(object=0x%016x [%s], id=\"%s\", x=%.6f, y=%.6f, z=%.6f, w=%.6f)\n", 
        obj, objinfo(obj).c_str(), id, x, y, z, w);

    libcall(obj, id, x, y, z, w);
}

void 
ospSetVec4fv(OSPObject obj, const char *id, const float *xyzw)
{
    ospSetVec4fv_ptr libcall = GET_PTR(ospSetVec4fv);
    
    log_message("ospSetVec4fv(object=0x%016x [%s], id=\"%s\", xyzw=0x%016x [%.6f, %.6f, %.6f, %.6f])\n", 
        obj, objinfo(obj).c_str(), id, xyzw, xyzw[0], xyzw[1], xyzw[2], xyzw[3]);

    libcall(obj, id, xyzw);
}

void 
ospSetVec4i(OSPObject obj, const char *id, int x, int y, int z, int w)
{
    ospSetVec4i_ptr libcall = GET_PTR(ospSetVec4i);
    
    log_message("ospSetVec4i(object=0x%016x [%s], id=\"%s\", x=%d, y=%d, z=%d, w=%d)\n", 
        obj, objinfo(obj).c_str(), id, x, y, z, w);

    libcall(obj, id, x, y, z, w);    
}

void 
ospSetVec4iv(OSPObject obj, const char *id, const int *xyzw)
{
    ospSetVec4iv_ptr libcall = GET_PTR(ospSetVec4iv);
    
    log_message("ospSetVec4fv(object=0x%016x [%s], id=\"%s\", xyzw=0x%016x [%d, %d, %d, %d])\n", 
        obj, objinfo(obj).c_str(), id, xyzw, xyzw[0], xyzw[1], xyzw[2], xyzw[3]);

    libcall(obj, id, xyzw);
}


float 
ospRenderFrame(OSPFrameBuffer framebuffer, OSPRenderer renderer, OSPCamera camera, OSPWorld world)
{
    ospRenderFrame_ptr libcall = GET_PTR(ospRenderFrame);

    log_message("ospRenderFrame(framebuffer=0x%016x [%s], renderer=0x%016x [%s], camera=0x%016x [%s], world=0x%016x [%s])\n", 
        framebuffer, objinfo(framebuffer).c_str(), 
        renderer, objinfo(renderer).c_str(),
        camera, objinfo(camera).c_str(),
        world, objinfo(world).c_str());   

    float res = libcall(framebuffer, renderer, camera, world);

    log_message("-> %.6f\n", res);   

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

