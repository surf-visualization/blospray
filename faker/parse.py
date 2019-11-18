#!/usr/bin/env python
import sys, getopt, json

"""
- Need to keep track of repeatedly used references. I.e. address A is released,
  but then a call to ospNew...() returns the same address, but for a new object.
  Should track the first as A-0, the second as A-1, etc
- Show edge to deleted object with dotted line
- Cascading object deletion not computed correctly

"""

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

type_counts = {}
addr2object = {}
handle2object = {}
addr_counts = {}
deleted_handles = set()

reference_sources = {}

ospdatatype2name = {}
ospframebufferformat2name = {}

renderframe_call_count = 0


def new_object(type, addr):
    """
    Returns a new Object, with reference count ZERO
    """

    if type not in type_counts:
        count = type_counts[type] = 1
    else:
        type_counts[type] += 1
        count = type_counts[type]
        
    if addr in addr_counts:
        print('Address %d is being reused' % addr)
        seqnr = addr_counts[addr] + 1
    else:
        seqnr = 1

    addr_counts[addr] = seqnr
    
    label = '%s#%d' % (type, count)
    
    obj = Object(type, addr, seqnr, label)
    
    handle = obj.handle
    
    assert addr not in addr2object
    assert handle not in handle2object
    
    addr2object[addr] = obj
    handle2object[handle] = obj

    return obj


def get_object_by_addr(addr, call=None):
    if addr not in addr2object:
        if call is not None:
            print('WARNING: unknown address %d in call to %s()' % (addr, call))
        else:
            print('WARNING: unknown address %d' % addr)
        return None
    
    return addr2object[addr]


class Object:
    
    def __init__(self, type, addr, seqnr, label):
        self.type = type
        self.addr = addr
        self.seqnr = seqnr
        self.label = label
        
        self.handle = (addr, seqnr)
        
        self.deleted = False
        self.fields = {}
        self.edges = {}

        self.dirty = True   # All objects must be committed at least once
        
        self.refcount = 0
        self.references = []
        
    def __repr__(self):
        return '<%s refcount=%d | %s-%d>' % (self.label, self.refcount, self.handle[0], self.handle[1])
        
    def set_property(self, field, value, mark_dirty=True):
        self.fields[field] = value
        if mark_dirty:
            self.dirty = True
        
    def add_edge(self, label, other, mark_dirty=True):
        self.edges[label] = other
        if mark_dirty:
            self.dirty = True

    def add_reference(self, other):
        assert isinstance(other, Object)
        assert other not in self.references        
        self.references.append(other)
        other.incref()
        
        if other not in reference_sources:
            reference_sources[other] = set([self])
        else:
            reference_sources[other].add(self)
        
    def commit(self):
        self.dirty = False
        
    def incref(self):
        self.refcount += 1
        
    def decref(self):
        self.refcount -= 1
        
        if self.refcount > 0:
            return
            
        for other in self.references:
            other.decref()
            reference_sources[other].remove(self)
                
        self.deleted = True
        del addr2object[self.addr]
        
        assert self.handle not in deleted_handles
        deleted_handles.add(self.handle)
                
        print('Deleted %s' % self)
        
    def dot(self, g):
        color = 'red' if self.dirty else 'black'
        
        fields = []
        for field in sorted(self.fields.keys()):
            value = self.fields[field]
            if field == 'voxelType':
                value = ospdatatype2name[value]
            fields.append('%s = %s' % (field,value))

        label = self.label
        label += '\n%d-%d\n' % self.handle
        label += 'REFCOUNT=%d' % self.refcount
        if len(fields) > 0:
            label += '\n\n'
            label += '\n'.join(fields)

        shape = 'box' if self.type in ['Data','SharedData'] else 'ellipse'
        style = 'dotted' if self.deleted else 'solid'
        g.write('o%d_%d [label="%s";color="%s";shape="%s";style="%s"];\n' % (self.addr, self.seqnr, label, color, shape, style))
        for label, other in self.edges.items():
            g.write('o%d_%d -> o%d_%d [label="%s"];\n' % (self.addr, self.seqnr, other.addr, other.seqnr, label))
    



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
            
            obj = new_object(type, addr)
            obj.incref()
            
            if call == 'ospNewMaterial':
                obj.set_property('<materialType>', args['materialType'], False)
                obj.set_property('<rendererType>', args['rendererType'], False)
            
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
                        otherobj = get_object_by_addr(otheraddr)
                        assert otherobj is not None
                        obj.add_edge('[%d]' % idx, otherobj, False)
                        obj.add_reference(otherobj)     
                    
            elif call == 'ospNewFrameBuffer':            
                obj.set_property('format', args['format'], False)
                obj.dirty = False
                
            elif call in ['ospNewCamera', 'ospNewGeometry', 'ospNewLight', 'ospNewRenderer', 'ospNewTexture', 'ospNewTransferFunction', 'ospNewVolume']:            
                obj.set_property('<type>', args['type'], False)
                
            elif call == 'ospNewGeometricModel':   
                geomaddr = args['geometry']
                geomobj = get_object_by_addr(geomaddr, call)
                if geomobj is not None:
                    obj.add_edge('geometry', geomobj, False)
                    obj.add_reference(geomobj)                
                
            elif call == 'ospNewVolumetricModel':
                voladdr = args['volume']
                volobj = get_object_by_addr(voladdr, call)
                if volobj is not None:
                    obj.add_edge('volume', volobj, False)
                    obj.add_reference(volobj)
                
            elif call == 'ospNewInstance':
                groupaddr = args['group']
                groupobj = get_object_by_addr(groupaddr, call)
                if groupobj is not None:
                    obj.add_edge('instance', groupobj, False)
                    obj.add_reference(groupobj)

        elif call == 'ospCopyData':                    
            
            source_addr = args['source']
            dest_addr = args['destination']
            dest_idx1 = args['destinationIndex1']
            assert dest_idx1 == 0
            
            source_obj = get_object_by_addr(source_addr, call)
            dest_obj = get_object_by_addr(dest_addr, call)
            
            assert source_obj is not None
            assert dest_obj is not None
            
            if source_obj.fields['type'] in REFERENCE_DATA_TYPES and hasattr(source_obj, 'items'):
                for idx, otheraddr in enumerate(source_obj.items):
                    otherobj = get_object_by_addr(otheraddr, call)
                    dest_obj.add_edge('[%d]' % idx, otherobj, False)
                    dest_obj.add_reference(otherobj)     
        
        elif call == 'ospRelease':
            objaddr = args['obj']
            if objaddr == 0:
                print('WARNING: ospRelease(0)')
            else:
                obj = get_object_by_addr(objaddr)
                if obj is not None:
                    obj.decref()
            
        elif call in ['ospRenderFrame', 'ospRenderFrameBlocking']:

            renderframe_call_count += 1

            if renderframe_call_count == stop_at_renderframe:

                obj = new_object('<%s>' % call, 0)  
                obj.dirty = False
                      
                for name in ['renderer', 'world', 'camera', 'framebuffer']:
                    argaddr = args[name]
                    ohterobj = get_object_by_addr(argaddr, call)
                    if otherobj is not None:
                        obj.add_edge(name, addr2object[argaddr], False)
                        
                result_addr = e['result']
                result = new_object('OSPFuture', result_addr)
                result.dirty = False
                
                obj.add_edge('result', result, False)

                break
            
        elif call == 'ospCommit':
            
            objaddr = args['obj']
            obj = get_object_by_addr(objaddr, call)
            if obj is not None:
                obj.commit()
            
        elif call.startswith('ospSet'):
            
            objaddr = args['obj']
            obj = get_object_by_addr(objaddr, call)
            if obj is None:
                continue
            
            if call == 'ospSetParam':
                data_type_name = args['type']
                if data_type_name in REFERENCE_DATA_TYPES:
                    otheraddr = args['mem']
                    
                    otherobj = get_object_by_addr(otheraddr, 'ospSetParam')
                    if otherobj is not None:
                        obj.add_edge(args['id'], otherobj)
                        obj.add_reference(otherobj)                
                    else:
                        print('ospSetParam(): unknown mem %d' % otheraddr)
                else:
                    # XXX handle if mem not found
                    if 'mem' in args:
                        obj.set_property(args['id'], args['mem'])
            
            else:
                raise ValueError(call)
        
        else:
            print('WARNING: unhandled call to "%s"' % call)
                
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

for handle, obj in handle2object.items():
    obj.dot(g)

"""
for addr, objs in addr2deletedobjects.items():    
    for obj in objs:
        obj.dot(g)
   """
   
g.write('}\n')
g.close()

for target, sources in reference_sources.items():
    print('%s <- %s' % (target, sources))
