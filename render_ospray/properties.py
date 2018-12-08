import bpy

from bpy.props import (
    BoolProperty,
    EnumProperty,
    FloatProperty,
    IntProperty,
    PointerProperty,
    StringProperty,
)

from bpy.types import PropertyGroup
  
class RenderOspraySettingsScene(PropertyGroup):
    
    renderer: EnumProperty(
        name='Renderer:',
        description='OSPRay renderer.',
        items=[ ('scivis', 'SciVis', ''),
                ('pathtracer', 'Path Tracer', ''),
               ]
        )                       

classes = (
    RenderOspraySettingsScene,
)

def register():
    from bpy.utils import register_class
    
    for cls in classes:
        register_class(cls)
        
    bpy.types.Scene.ospray = PointerProperty(type=RenderOspraySettingsScene)
    
    
def unregister():
    from bpy.utils import unregister_class
    
    for cls in classes:
        unregister_class(cls)
        
    del bpy.types.Scene.ospray
    