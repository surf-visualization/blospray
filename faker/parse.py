#!/usr/bin/env python
import sys, getopt, json

def usage():
    print('usage: %s [options] faker.log' % sys.argv[0])
    print()
    print('Options:')
    print('  -r     Stop at ospRenderFrame (may be repeated)')
    print()

try:
    options, args = getopt.getopt(sys.argv[1:], 'r')
except getopt.GetoptError as e:
    raise
    
if len(args) == 0:
    usage()
    sys.exit(-1)   

stop_at_renderframe = 0

for opt, value in options:
    if opt == '-r':
        stop_at_renderframe += 1

f = open(args[0], 'rt')

# XXX unused
OBJECT_ARGUMENTS = {
    'ospNewGeometricModel': ['geometry'],
    'ospNewInstance': ['group'],
    'ospNewVolumetricModel': ['volume'],
    'ospCommit': ['obj'],
    'ospRelease': ['obj'],
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
reference_counts = {}
addr2deletedobjects = {}

ospdatatype2name = {}
ospframebufferformat2name = {}

renderframe_call_count = 0

class Object:
    
    def __init__(self, type, addr):
        self.addr = addr
        self.type = type
        self.deleted = False
        
        if type not in type_counts:
            type_counts[type] = count = 1
        else:
            type_counts[type] += 1
            count = type_counts[type]
        self.label = '%s#%d' % (type, count)
        
        self.fields = {}
        self.edges = {}
        self.dirty = True   # All objects must be committed at least once
        
        self.references = []
        
    def set_property(self, field, value, dirty=True):
        self.fields[field] = value
        self.dirty = dirty
        
    def add_edge(self, label, other, dirty=True):
        self.edges[label] = other
        if dirty:
            self.dirty = True

    def add_reference(self, other):
        assert other not in self.references        
        self.references.append(other)
        
    def commit(self):
        self.dirty = False
        
    def dot(self, g):
        color = 'red' if self.dirty else 'black'
        
        fields = []
        for field in sorted(self.fields.keys()):
            value = self.fields[field]
            if field == 'voxelType':
                value = ospdatatype2name[value]
            fields.append('%s = %s' % (field,value))

        c = '?' if self.addr not in reference_counts else str(reference_counts[self.addr])
        label = '%s [%s]' % (self.label, c)
        label += '\n%d\n' % self.addr
        if len(fields) > 0:
            label += '\n\n'
            label += '\n'.join(fields)

        shape = 'box' if self.type == 'Data' else 'ellipse'
        style = 'dotted' if self.deleted else 'solid'
        g.write('%d [label="%s";color="%s";shape="%s";style="%s"];\n' % (self.addr, label, color, shape, style))
        for label, other in self.edges.items():
            g.write('%d -> %d [label="%s"];\n' % (self.addr, other.addr, label))
    
def decref(objaddr):

    left_to_decref = [objaddr]

    while len(left_to_decref) > 0:

        objaddr = left_to_decref.pop()
        
        if objaddr not in reference_counts:
            print('decref(): unknown address %d!' % objaddr)
            continue
        
        c = reference_counts[objaddr] - 1
        assert c >= 0
        if c == 0:
            obj = addr2object[objaddr]
            print('Object %d (%s) deleted' % (objaddr, obj.label))            
            obj.deleted = True

            del addr2object[objaddr]
            del reference_counts[objaddr]
            if objaddr not in addr2deletedobjects:
                addr2deletedobjects[objaddr] = [obj]
            else:
                addr2deletedobjects[objaddr].append(obj)            

            for refobj in obj.references:
                left_to_decref.append(refobj.addr)

        else:
            reference_counts[objaddr] = c


REFERENCE_DATA_TYPES = [
    'OSP_CAMERA',    
    'OSP_GEOMETRY',
    'OSP_GEOMETRIC_MODEL', 
    'OSP_GROUP',    
    'OSP_INSTANCE',
    'OSP_LIGHT', 
    'OSP_MATERIAL',     
    'OSP_OBJECT', 
    'OSP_TEXTURE', 
    'OSP_VOLUME', 
    'OSP_VOLUMETRIC_MODEL'
]

try:

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
            reference_counts[addr] = 1
            pointers.add(addr)
            
            if call == 'ospNewMaterial':
                obj.set_property('<materialType>', args['materialType'], False)
                obj.set_property('<rendererType>', args['rendererType'], False)
                
            elif call == '_Z10ospNewDatam11OSPDataTypePKvj':
                # ospNewData(unsigned long, OSPDataType, void const*, unsigned int)
                data_type_name = args['type']
                obj.set_property('type', data_type_name, False)
                obj.set_property('numItems', args['numItems'], False)                                
                if data_type_name in REFERENCE_DATA_TYPES and 'source' in e:         
                    obj.items = e['source'][:]
                    for idx, otheraddr in enumerate(e['source']):                        
                        reference_counts[otheraddr] += 1
                        otherobj = addr2object[otheraddr]
                        obj.add_edge('[%d]' % idx, otherobj, False)
                        obj.add_reference(otherobj)
                        
            elif call == 'ospNewData':
                data_type_name = args['type']
                obj.set_property('type', data_type_name, False)
                obj.set_property('numItems1', args['numItems1'], False)
                obj.items = [None]*args['numItems1']
                if args['numItems2'] > 1:
                    obj.set_property('numItems2', args['numItems2'], False)
                if args['numItems3'] > 1:
                    obj.set_property('numItems3', args['numItems3'], False)
                    
            elif call == 'ospNewSharedData':
                data_addr = args['sharedData']
                data_type_name = args['type']
                obj.set_property('type', data_type_name, False)
                obj.set_property('numItems1', args['numItems1'], False)
                if args['numItems2'] > 1:
                    obj.set_property('numItems2', args['numItems2'], False)
                if args['numItems3'] > 1:
                    obj.set_property('numItems3', args['numItems3'], False)
                if args['byteStride1'] > 0:
                    obj.set_property('byteStride1', args['byteStride1'], False)
                if args['byteStride2'] > 0:
                    obj.set_property('byteStride2', args['byteStride2'], False)
                if args['byteStride3'] > 0:
                    obj.set_property('byteStride3', args['byteStride3'], False)
                    
                if data_type_name in REFERENCE_DATA_TYPES and 'source' in e:  
                    obj.items = e['source'][:]
                    for idx, otheraddr in enumerate(e['source']):
                        reference_counts[otheraddr] += 1
                        otherobj = addr2object[otheraddr]
                        obj.add_edge('[%d]' % idx, otherobj, False)
                        obj.add_reference(otherobj)     
                    
            elif call == 'ospNewFrameBuffer':            
                obj.set_property('format', args['format'], False)
                
            elif call in ['ospNewCamera', 'ospNewGeometry', 'ospNewLight', 'ospNewRenderer', 'ospNewTexture', 'ospNewTransferFunction', 'ospNewVolume']:            
                obj.set_property('<type>', args['type'], False)
                
            elif call == 'ospNewGeometricModel':   
                geomaddr = args['geometry']
                if geomaddr not in addr2object:
                    print('WARNING: Unknown geometry %d for call to ospNewGeometricModel()' % geomaddr)
                    continue
                geomobj = addr2object[geomaddr]
                obj.add_edge('geometry', geomobj, False)
                obj.add_reference(geomobj)                
                reference_counts[args['geometry']] += 1
                
            elif call == 'ospNewVolumetricModel':
                voladdr = args['volume']
                if voladdr not in addr2object:
                    print('WARNING: Unknown volume %d for call to ospNewVolumetricModel()' % voladdr)
                    continue
                volobj = addr2object[voladdr]
                obj.add_edge('volume', volobj, False)
                obj.add_reference(volobj)
                reference_counts[args['volume']] += 1
                
            elif call == 'ospNewInstance':
                groupaddr = args['group']
                if groupaddr not in addr2object:
                    print('WARNING: Unknown group %d for call to ospNewInstance()' % groupaddr)
                    continue
                groupobj = addr2object[groupaddr]
                obj.add_edge('instance', groupobj, False)
                obj.add_reference(groupobj)
                reference_counts[args['group']] += 1

        elif call == 'ospCopyData':                    
            
            dest_addr = args['destination']
            source_addr = args['source']
            dest_idx1 = args['destinationIndex1']
            assert dest_idx1 == 0
            
            source_obj = addr2object[source_addr]
            dest_obj = addr2object[dest_addr]
            
            if source_obj.fields['type'] in REFERENCE_DATA_TYPES and hasattr(source_obj, 'items'):
                for idx, otheraddr in enumerate(source_obj.items):
                    reference_counts[otheraddr] += 1
                    otherobj = addr2object[otheraddr]
                    dest_obj.add_edge('[%d]' % idx, otherobj, False)
                    dest_obj.add_reference(otherobj)     
        
        elif call == 'ospRelease':
            objaddr = args['obj']
            if objaddr == 0:
                print('WARNING: ospRelease(0)')
            else:
                decref(objaddr)   
            
        elif call in ['ospRenderFrame', 'ospRenderFrameBlocking']:

            renderframe_call_count += 1

            if renderframe_call_count == stop_at_renderframe:

                obj = Object('<%s>' % call, 0)  
                assert 0 not in addr2object
                addr2object[0] = obj    # XXX 0
                      
                for name in ['renderer', 'world', 'camera', 'framebuffer']:
                    argaddr = args[name]
                    if argaddr not in addr2object:
                        print('WARNING: cannot add edge "%s" to unknown object %d' % (name, argaddr))
                        continue
                    obj.add_edge(name, addr2object[argaddr], False)

                break
            
        elif call == 'ospCommit':
            
            objaddr = args['obj']
            if objaddr not in addr2object:
                print('WARNING: Unknown object %d for call to ospCommit()' % objaddr)
                continue
            
            obj = addr2object[objaddr]
            obj.commit()
            
        elif call.startswith('ospSet'):
            
            objaddr = args['obj']
            if objaddr not in addr2object:
                print('WARNING: Unknown object %d for call to %s()' % (objaddr, call))
                continue
                
            obj = addr2object[objaddr]
            
            if call == 'ospSetParam':
                data_type_name = args['type']
                if data_type_name in REFERENCE_DATA_TYPES:
                    otheraddr = args['mem']
                    if otheraddr in addr2object:      
                        otherobj = addr2object[otheraddr]
                        obj.add_edge(args['id'], otherobj)
                        obj.add_reference(otherobj)                
                        reference_counts[otheraddr] += 1            
                    else:
                        print('ospSetParam(): unknown mem %d' % otheraddr)
                else:
                    # XXX handle if mem not found
                    if 'mem' in args:
                        obj.set_property(args['id'], args['mem'])
            
            else:
                raise ValueError(call)
                
            """
            elif call == 'ospSetObject':
                otheraddr = args['other']
                otherobj = addr2object[otheraddr]
                obj.add_edge(args['id'], otherobj)
                obj.add_reference(otherobj)
                reference_counts[otheraddr] += 1
            elif call in ['ospSetBool', 'ospSetString', 'ospSetFloat', 'ospSetInt']:
                obj.set_property(args['id'], args['x'])
            """
            
except KeyError as e:
    raise

g = open('dump.dot', 'wt')
g.write('digraph {\n')

for addr, obj in addr2object.items():    
    obj.dot(g)

for addr, objs in addr2deletedobjects.items():    
    for obj in objs:
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