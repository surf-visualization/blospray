import bpy

from bpy.props import (
    BoolProperty,
    EnumProperty,
    FloatProperty,
    FloatVectorProperty,
    IntProperty,
    PointerProperty,
    StringProperty,
)

from bpy.types import PropertyGroup


"""
 my_bool = BoolProperty(
        name="Enable or Disable",
        description="A bool property",
        default = False
        )

    my_int = IntProperty(
        name = "Int Value",
        description="A integer property",
        default = 23,
        min = 10,
        max = 100
        )

    my_float = FloatProperty(
        name = "Float Value",
        description = "A float property",
        default = 23.7,
        min = 0.01,
        max = 30.0
        )

    my_string = StringProperty(
        name="User Input",
        description=":",
        default="",
        maxlen=1024,
        )

    my_enum = EnumProperty(
        name="Dropdown:",
        description="Apply Data to attribute.",
        items=[ ('OP1', "Option 1", ""),
                ('OP2', "Option 2", ""),
                ('OP3', "Option 3", ""),
               ]
        )
"""
  
class RenderOspraySettingsScene(PropertyGroup):
    
    renderer: EnumProperty(
        name='Renderer',
        description='Renderer type',
        items=[ ('scivis', 'SciVis', ''),
                ('pathtracer', 'Path Tracer', ''),
               ]
        )                 
        
    host: StringProperty(
        name='Host',
        description='Host to connect to',
        default='localhost',
        maxlen=128,
        )
        
    port: IntProperty(
        name='Port',
        description='Port to connect on',
        default=5909,
        min=1024,
        max=65535
        )

    samples: IntProperty(
        name='Samples',
        description='Number of samples per pixel',
        default = 4,
        min = 1,
        max = 256
        )
        
    ao_samples: IntProperty(
        name='AO samples',
        description='Number of AO rays per sample (aoSamples)',
        default = 2,
        min = 0,
        max = 32
        )
        
    shadows_enabled: BoolProperty(
        name="Shadows",
        description="Compute (hard) shadows (shadowsEnabled)",
        default = True
        )
        
        
class RenderOspraySettingsWorld(PropertyGroup):
    
    # XXX add alpha
    background_color: FloatVectorProperty(  
        name="Background color",
        subtype='COLOR',
        default=(1.0, 1.0, 1.0),
        min=0.0, max=1.0,
        description='Background color (bgColor)'
        )

    ambient_color: FloatVectorProperty(  
        name="Ambient color",
        subtype='COLOR',
        default=(1.0, 1.0, 1.0),
        min=0.0, max=1.0,
        description='Ambient color'
        )
   
    ambient_intensity: FloatProperty(
        name='Ambient intensity',
        description='Amount of ambient light',
        default = 0.5,
        min = 0,
        max = 100
        )
        

classes = (
    RenderOspraySettingsScene,
    RenderOspraySettingsWorld,
)

def register():
    from bpy.utils import register_class
    
    for cls in classes:
        register_class(cls)
        
    bpy.types.Scene.ospray = PointerProperty(type=RenderOspraySettingsScene)
    bpy.types.World.ospray = PointerProperty(type=RenderOspraySettingsWorld)
    
    
def unregister():
    from bpy.utils import unregister_class
    
    for cls in classes:
        unregister_class(cls)
        
    del bpy.types.Scene.ospray
    del bpy.types.World.ospray
    
    