#!/usr/bin/env python
import sys, json

f = open(sys.argv[1], 'rt')

OBJECT_ARGUMENTS = {
    'ospNewGeometricModel': ['geometry'],
    'ospNewInstance': ['group'],
    'ospNewVolumetricModel': ['volume'],
    'ospCommit': ['obj'],
    'ospRelease': ['obj'],
    'ospSetData': ['obj', 'data'],
    'ospSetObject': ['obj', 'other'],
    'ospSetBool': ['obj'],
    'ospSetFloat': ['obj'],
    'ospSetInt': ['obj'],
    'ospSetLinear3fv': ['obj'],
    'ospSetAffine3fv': ['obj'],
    'ospSetString': ['obj'],
    'ospSetVoidPtr': ['obj'],
    'ospSetVec2f': ['obj'],
    'ospSetVec2fv': ['obj'],
    'ospSetVec2i': ['obj'],
    'ospSetVec2iv': ['obj'],
    'ospSetVec3f': ['obj'],
    'ospSetVec3fv': ['obj'],
    'ospSetVec3i': ['obj'],
    'ospSetVec3iv': ['obj'],
    'ospSetVec4f': ['obj'],
    'ospSetVec4fv': ['obj'],
    'ospSetVec4i': ['obj'],
    'ospSetVec4iv': ['obj'],
    'ospRenderFrame': ['framebuffer', 'renderer', 'camera', 'world'] 
}

type_counts = {}
pointers = set()
addr2object = {}

class Object:
    
    def __init__(self, type, addr):
        self.addr = addr
        self.type = type
        
        if type not in type_counts:
            type_counts[type] = count = 1
        else:
            type_counts[type] += 1
            count = type_counts[type]
        self.label = '%s#%d' % (type, count)
        
        self.fields = {}
        self.edges = {}
        self.dirty = False
        
    def set_property(self, field, value, dirty=True):
        self.fields[field] = value
        self.dirty = dirty
        
    def add_edge(self, label, other, dirty=True):
        self.edges[label] = other
        if dirty:
            self.dirty = True
        
    def commit(self):
        self.dirty = False
        
    def dot(self, g):
        color = 'red' if self.dirty else 'black'
        
        fields = []
        for field in sorted(self.fields.keys()):
            value = self.fields[field]
            fields.append('%s = %s' % (field,value))

        label = self.label
        if len(fields) > 0:
            label += '\n\n'
            label += '\n'.join(fields)

        shape = 'box' if self.type == 'Data' else 'ellipse'
        g.write('%d [label="%s";color="%s";shape="%s"];\n' % (self.addr, label, color, shape))
        for label, other in self.edges.items():
            g.write('%d -> %d [label="%s"];\n' % (self.addr, other.addr, label))
    
ospdatatype2name = {}
ospframebufferformat2name = {}

for line in f:
    e = json.loads(line)
    
    call = e['call']
    try:
        args = e['arguments']
    except KeyError:
        args = None

    if call == '<enums>':
        for name, value in e['result']['OSPDataType'].items():
            ospdatatype2name[value] = name
        for name, value in e['result']['OSPFrameBufferFormat'].items():
            ospframebufferformat2name[value] = name

    elif call.startswith('ospNew'):

        type = call[6:]
        addr = e['result']
        
        if addr in addr2object:
            print('Address %d is being reused' % addr)
        
        obj = Object(type, addr)
        
        addr2object[addr] = obj
        pointers.add(addr)
        
        if call == 'ospNewMaterial':
            obj.set_property('<materialType>', args['materialType'], False)
            obj.set_property('<rendererType>', args['rendererType'], False)
        elif call == 'ospNewData':
            data_type = args['type']
            data_type_name = ospdatatype2name[data_type]
            obj.set_property('numItems', args['numItems'], False)
            obj.set_property('type', data_type_name, False)
            if data_type_name == 'OSP_OBJECT' and 'source' in e:                
                for idx, objaddr in enumerate(e['source']):
                    obj.add_edge('[%d]' % idx, addr2object[objaddr], False)
        elif call == 'ospNewFrameBuffer':            
            obj.set_property('format', ospframebufferformat2name[args['format']], False)
        elif call in ['ospNewCamera', 'ospNewGeometry', 'ospNewLight', 'ospNewRenderer', 'ospNewTexture', 'ospNewTransferFunction', 'ospNewVolume']:            
            obj.set_property('<type>', args['type'], False)
        elif call == 'ospNewGeometricModel':            
            obj.add_edge('geometry', addr2object[args['geometry']], False)
        elif call == 'ospNewVolumetricModel':
            obj.add_edge('volume', addr2object[args['volume']], False)
        elif call == 'ospNewInstance':
            obj.add_edge('instance', addr2object[args['group']], False)

    elif call == 'ospRelease':
        #obj = args['obj']
        #del addr2object[obj]
        pass
        
    elif call == 'ospRenderFrame':

        obj = Object('<ospRenderFrame>', 0)  
        addr2object[0] = obj
              
        for name in ['renderer', 'world', 'camera', 'framebuffer']:
            obj.add_edge(name, addr2object[args[name]], False)

        break
        
    elif call == 'ospCommit':
        obj = addr2object[args['obj']]
        obj.commit()
        
    elif call.startswith('ospSet'):
        
        obj = addr2object[args['obj']]
        
        if call == 'ospSetData':
            data = addr2object[args['data']]
            obj.add_edge(args['id'], data)
        elif call == 'ospSetObject':
            other = addr2object[args['other']]
            obj.add_edge(args['id'], other)
        elif call in ['ospSetBool', 'ospSetString', 'ospSetFloat', 'ospSetInt']:
            obj.set_property(args['id'], args['x'])
            
g = open('dump.dot', 'wt')
g.write('digraph {\n')

for addr, obj in addr2object.items():
    obj.dot(g)
    
g.write('}\n')
g.close()
    


"""
typedef std::map<void*, int>            ReferenceCountMap;
typedef std::map<void*, std::string>    ReferenceTypeMap;

typedef std::map<OSPDataType, std::string>  OSPDataTypeMap;
typedef std::map<OSPFrameBufferFormat, std::string>  OSPFrameBufferFormatMap;




static ReferenceCountMap   reference_counts;
static ReferenceTypeMap    reference_types;

static OSPDataTypeMap           ospdatatype_names;
static OSPFrameBufferFormatMap  ospframebufferformat_names;


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


    log_message("ospNewData(numItems=%ld, type=%d [%s], source=0x%016x, dataCreationFlags=0x%08x)\n", 
        numItems, type, ospdatatype_name(type).c_str(), source, dataCreationFlags);    
    
    
    //log_message("ospNewFrameBuffer(x=%d, y=%d, format=0x%04x [%s], frameBufferChannels=0x%08x)\n", 
    //    x, y, format, ospframebufferformat_name(format).c_str(), frameBufferChannels);    
    
    
        
    //log_message("ospNewGeometricModel(geometry=0x%016x [%s])\n", geometry, objinfo(geometry).c_str());
    
    
    log_message("OSPInstance(group=0x%016x [%s])\n", group, objinfo(group).c_str());
    
    
    log_message("ospNewMaterial(rendererType=\"%s\", materialType=\"%s\")\n", 
        rendererType, materialType);    
    
    
    log_message("ospNewVolumetricModel(volume=0x%016x [%s])\n", volume, objinfo(volume).c_str());
    
    
    log_message("ospCommit(object=0x%016x [%s])\n", obj, objinfo(obj).c_str());
    
    
    log_message("ospRelease(object=0x%016x [%s])\n", obj, objinfo(obj).c_str());
    
    
    log_message("ospSetData(object=0x%016x [%s], id=\"%s\", data=0x%016x [%s])\n", 
        obj, objinfo(obj).c_str(), id, data, objinfo(data).c_str());



    log_message("ospSetObject(object=0x%016x [%s], id=\"%s\", other=0x%016x [%s])\n", 
        obj, objinfo(obj).c_str(), id, other, objinfo(other).c_str());


    log_message("ospSetBool(object=0x%016x [%s], id=\"%s\", x=%d)\n", 
        obj, objinfo(obj).c_str(), id, x);


    log_message("ospSetFloat(object=0x%016x [%s], id=\"%s\", x=%.6f)\n", 
        obj, objinfo(obj).c_str(), id, x);


    log_message("ospSetInt(object=0x%016x [%s], id=\"%s\", x=%d)\n", 
        obj, objinfo(obj).c_str(), id, x);


    log_message("ospSetLinear3fv(object=0x%016x [%s], id=\"%s\", v=0x%016x [%.6f])\n", 
        obj, objinfo(obj).c_str(), id, v);



    char msg[1024];
    
    log_message("ospSetAffine3fv(object=0x%016x [%s], id=\"%s\", v=0x%016x [%.6f, %.6f, %.6f | %.6f, %.6f, %.6f | %.6f, %.6f, %.6f | %.6f, %.6f, %.6f ])\n", 
        obj, objinfo(obj).c_str(), id, v,
        v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11]);



    log_message("ospSetString(object=0x%016x [%s], id=\"%s\", s=\"%s\")\n", 
        obj, objinfo(obj).c_str(), id, s);



    log_message("ospSetVoidPtr(object=0x%016x [%s], id=\"%s\", v=0x%016x [%s])\n", 
        obj, objinfo(obj).c_str(), id, v, objinfo(v));



    log_message("ospSetVec2f(object=0x%016x [%s], id=\"%s\", x=%.6f, y=%.6f)\n", 
        obj, objinfo(obj).c_str(), id, x, y);


    log_message("ospSetVec2fv(object=0x%016x [%s], id=\"%s\", xy=0x%016x [%.6f, %.6f])\n", 
        obj, objinfo(obj).c_str(), id, xy, xy[0], xy[1]);


    log_message("ospSetVec2i(object=0x%016x [%s], id=\"%s\", x=%d, y=%d)\n", 
        obj, objinfo(obj).c_str(), id, x, y);


    log_message("ospSetVec2fv(object=0x%016x [%s], id=\"%s\", xy=0x%016x [%d, %d])\n", 
        obj, objinfo(obj).c_str(), id, xy, xy[0], xy[1]);


    log_message("ospSetVec3f(object=0x%016x [%s], id=\"%s\", x=%.6f, y=%.6f, z=%.6f)\n", 
        obj, objinfo(obj).c_str(), id, x, y, z);



    log_message("ospSetVec3fv(object=0x%016x [%s], id=\"%s\", xyz=0x%016x [%.6f, %.6f, %.6f])\n", 
        obj, objinfo(obj).c_str(), id, xyz, xyz[0], xyz[1], xyz[2]);


    
    log_message("ospSetVec3i(object=0x%016x [%s], id=\"%s\", x=%d, y=%d, z=%d)\n", 
        obj, objinfo(obj).c_str(), id, x, y, z);


    log_message("ospSetVec3fv(object=0x%016x [%s], id=\"%s\", xyz=0x%016x [%d, %d, %d])\n", 
        obj, objinfo(obj).c_str(), id, xyz, xyz[0], xyz[1], xyz[2]);



    log_message("ospSetVec4f(object=0x%016x [%s], id=\"%s\", x=%.6f, y=%.6f, z=%.6f, w=%.6f)\n", 
        obj, objinfo(obj).c_str(), id, x, y, z, w);



    log_message("ospSetVec4fv(object=0x%016x [%s], id=\"%s\", xyzw=0x%016x [%.6f, %.6f, %.6f, %.6f])\n", 
        obj, objinfo(obj).c_str(), id, xyzw, xyzw[0], xyzw[1], xyzw[2], xyzw[3]);


    log_message("ospSetVec4i(object=0x%016x [%s], id=\"%s\", x=%d, y=%d, z=%d, w=%d)\n", 
        obj, objinfo(obj).c_str(), id, x, y, z, w);



    log_message("ospSetVec4fv(object=0x%016x [%s], id=\"%s\", xyzw=0x%016x [%d, %d, %d, %d])\n", 
        obj, objinfo(obj).c_str(), id, xyzw, xyzw[0], xyzw[1], xyzw[2], xyzw[3]);



    log_message("ospRenderFrame(framebuffer=0x%016x [%s], renderer=0x%016x [%s], camera=0x%016x [%s], world=0x%016x [%s])\n", 
        framebuffer, objinfo(framebuffer).c_str(), 
        renderer, objinfo(renderer).c_str(),
        camera, objinfo(camera).c_str(),
        world, objinfo(world).c_str());   
"""