import bpy, bmesh

from .connection import Connection

# XXX generalize to object mesh

class OSPRayUpdateMeshVolumeExtents(bpy.types.Operator):
    
    # XXX unfinished

    """Update volume bounding geometry with plugin bound"""             
    bl_idname = "ospray.volume_update_mesh"
    bl_label = "Update extent mesh"
    bl_options = {'REGISTER'}#, 'UNDO'}             # Enable undo for the operator?

    def execute(self, context):

        obj = context.active_object
        # XXX check is mesh
        msh = obj.data

        scene = context.scene
        ospray = scene.ospray
        
        print(msh)
                
        #connection = Connection(None, ospray.host, ospray.port)
        #connection.update_volume_mesh(msh)
        
        # XXX use fake geometry for now
        bm = bmesh.new()        
        verts = []
        verts.append(bm.verts.new((0, 0, 0)))
        verts.append(bm.verts.new((1, 1, 1)))
        verts.append(bm.verts.new((0, 2, 0)))
        verts.append(bm.verts.new((2, 0, 0)))        
        bm.faces.new(verts)        
        bm.to_mesh(msh)
        msh.update()
        
        return {'FINISHED'}
        
    def update_volume_mesh(self, mesh):
        """
        Get volume extent from render server, update mesh

        XXX unfinished, copied from connection.py
        """

        self.sock.connect((self.host, self.port))
        # XXX hello message

        # Volume data (i.e. mesh)

        msg = 'Getting extent for mesh %s (ospray volume)' % mesh.name
        print(msg)

        # Properties

        properties = {}
        properties['plugin'] = mesh.ospray.plugin
        self._process_properties(mesh, properties)

        print('Sending properties:')
        print(properties)

        # Request

        client_message = ClientMessage()
        client_message.type = ClientMessage.QUERY_VOLUME_EXTENT
        send_protobuf(self.sock, client_message)

        request = VolumeExtentRequest()
        request.name = mesh.name
        request.properties = json.dumps(properties)
        send_protobuf(self.sock, request)

        # Get result
        extent_result = VolumeExtentFunctionResult()

        receive_protobuf(self.sock, extent_result)

        if not extent_result.success:
            print('ERROR: volume extent query failed:')
            print(extent_result.message)
            return

        # XXX send bye
        self.sock.close()

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
