import bpy, bmesh

from .connection import Connection

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
