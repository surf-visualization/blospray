import bpy

from .connection import Connection

class OSPRayUpdateMeshVolumeExtents(bpy.types.Operator):
    
    """Get volume extent from server"""             # Tooltip
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
        connection = Connection(None, ospray.host, ospray.port)
        connection.update_volume_mesh(msh)

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
