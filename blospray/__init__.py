bl_info = {
    "name": "OSPRay",
    "author": "Paul Melis",
    "version": (0, 0, 1),
    "blender": (2, 8, 0),
    "location": "Render > Engine > OSPRay",
    "description": "OSPRay integration for blender",
    "warning": "Alpha quality",
    "category": "Render"}
    
import bpy

if "bpy" in locals():
    import imp
    imp.reload(ui)
    #imp.reload(render)
    #imp.reload(update_files)

else:
    import bpy
    from bpy.types import (AddonPreferences,
                           PropertyGroup,
                           )
    from bpy.props import (StringProperty,
                           BoolProperty,
                           IntProperty,
                           FloatProperty,
                           FloatVectorProperty,
                           EnumProperty,
                           PointerProperty,
                           )
    from . import ui
    #from . import render
    #from . import update_files

class RenderOspraySettingsScene(PropertyGroup):
    
    renderer = EnumProperty(
        name='Renderer:',
        description='OSPRay renderer.',
        items=[ ('scivis', 'Scivis', ''),
                ('pathtracer', 'Path tracer', ''),
               ]
        )                       


def register():
    bpy.utils.register_module(__name__)
    
    bpy.types.Scene.ospray = PointerProperty(type=RenderOspraySettingsScene)
    
def unregister():
    bpy.utils.unregister_module(__name__)
    
    del bpy.types.Scene.ospray
    
if __name__ == "__main__":
    register()        
    