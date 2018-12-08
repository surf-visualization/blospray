import bpy

from bpy.types import (Panel,
                       Operator,
                       PropertyGroup,
                       )                       

        
class RENDER_PT_OSPRAY(Panel):
    bl_idname = 'RENDER_PT_OSPRAY'
    bl_label = 'Renderer'
    bl_space_type = 'PROPERTIES'   
    bl_region_type = 'WINDOW'    
    bl_context = 'render'  
    
    COMPAT_ENGINES = {'OSPRAY'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return rd.engine in cls.COMPAT_ENGINES

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        #layout.use_property_decorate = False
        
        scene = context.scene
        ospray = scene.ospray
        
        col = layout.column(align=True)
        col.prop(ospray, 'renderer', text='Renderer') 
        col.prop(ospray, 'samples', text='Samples') 
        col.prop(ospray, 'ao_samples', text='AO Samples') 
        col.prop(ospray, 'shadows_enabled', text='Shadows') 


  
class WORLD_PT_OSPRAY(Panel):
    bl_idname = 'WORLD_PT_OSPRAY'
    bl_label = 'World'
    bl_space_type = 'PROPERTIES'   
    bl_region_type = 'WINDOW'    
    bl_context = 'world'  
    
    COMPAT_ENGINES = {'OSPRAY'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return rd.engine in cls.COMPAT_ENGINES

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        #layout.use_property_decorate = False
        
        world = context.world
        ospray = world.ospray
        
        col = layout.column(align=True)
        col.prop(ospray, 'background_color', text='Background') 
        col.prop(ospray, 'ambient_color', text='Ambient color') 
        col.prop(ospray, 'ambient_intensity', text='Ambient intensity') 
        

classes = (
    RENDER_PT_OSPRAY,
    WORLD_PT_OSPRAY,
)

def register():
    from bpy.utils import register_class
    
    print(classes)
    for cls in classes:
        register_class(cls)
    
    
def unregister():
    from bpy.utils import unregister_class
    
    for cls in classes:
        unregister_class(cls)

