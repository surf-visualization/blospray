import bpy, bmesh
import socket
from struct import unpack
import numpy

from .common import send_protobuf, receive_protobuf, receive_buffer, receive_into_numpy_array
from .connection import Connection
from .messages_pb2 import ClientMessage, QueryBoundResult

# XXX generalize to object mesh, it's not specific to a volume
# XXX if this operator gets called during rendering, then what? :)

class OSPRayUpdateMeshVolumeExtents(bpy.types.Operator):
    
    # XXX unfinished

    """Update bounding geometry with bound provided by plugin"""             
    bl_idname = "ospray.volume_update_mesh"
    bl_label = "Update extent mesh"
    bl_options = {'REGISTER'}#, 'UNDO'}             # Enable undo for the operator?

    def execute(self, context):

        obj = context.active_object
        assert obj.type == 'MESH'
        mesh = obj.data

        scene = context.scene
        ospray = scene.ospray
        
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)                
        sock.connect((ospray.host, ospray.port))
        # XXX hello message

        # Volume data (i.e. mesh)

        print('Getting extent for mesh %s (ospray volume)' % mesh.name)

        # Send request

        client_message = ClientMessage()
        client_message.type = ClientMessage.QUERY_BOUND
        client_message.string_value = mesh.name
        send_protobuf(sock, client_message)

        # Get result
        
        result = QueryBoundResult()        
        receive_protobuf(sock, result)

        if not result.success:
            print('ERROR: extent query failed:')
            print(result.message)
            return {'FAILED'}
            
        # Receive actual geometry
        vertices_len, edges_len, faces_len, loop_len = unpack('<IIII', receive_buffer(sock, 4*4))
        
        vertices = numpy.empty(vertices_len, dtype=numpy.float32)
        edges = numpy.empty(edges_len, dtype=numpy.uint32)
        faces = numpy.empty(faces_len, dtype=numpy.uint32)
        loop_start = numpy.empty(loop_len, dtype=numpy.uint32)
        loop_total = numpy.empty(loop_len, dtype=numpy.uint32)
        
        receive_into_numpy_array(sock, vertices, vertices_len*4)
        receive_into_numpy_array(sock, edges, edges_len*4)
        receive_into_numpy_array(sock, faces, faces_len*4)
        receive_into_numpy_array(sock, loop_start, loop_len*4)
        receive_into_numpy_array(sock, loop_total, loop_len*4)
        
        #print(vertices)
        #print(edges)
        #print(faces)
        #print(loop_start)
        #print(loop_total)

        # XXX send bye
        sock.close()
        
        # XXX use new mesh replace from 2.81 when it becomes available
        
        bm = bmesh.new()        
        
        verts = []
        for x, y, z in vertices.reshape((-1,3)):
            verts.append(bm.verts.new((x, y, z)))
            
        for i, j in edges.reshape((-1,2)):
            bm.edges.new((verts[i], verts[j]))
            
        for start, total in zip(loop_start, loop_total):     
            vv = []
            for i in range(total):
                vi = faces[start+i]
                vv.append(verts[vi])
            bm.faces.new(vv)   
            
        bm.to_mesh(mesh)

        mesh.update()
        
        return {'FINISHED'}
        

classes = (
    OSPRayUpdateMeshVolumeExtents,
)

def register():
    from bpy.utils import register_class

    for cls in classes:
        register_class(cls)

def unregister():
    from bpy.utils import unregister_class

    for cls in classes:
        unregister_class(cls)
