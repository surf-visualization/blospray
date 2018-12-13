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
        
        
class RenderOspraySettingsLight(PropertyGroup):
    
    # Common
    
    color: FloatVectorProperty(  
        name="Color",
        subtype='COLOR',
        default=(1.0, 1.0, 1.0),
        min=0.0, max=1.0,
        description='Color of the light (color)'
        )
    
    intensity: FloatProperty(
        name='Intensity',
        description='Intensity of the light (intensity)',
        default = 1,
        min = 0,
        max = 100
        )    
        
    is_visible: BoolProperty(
        name="Visible",
        description="Whether the light can be directly seen (isVisible)",
        default = True
        )        
        
    # Directional/distant light
    
    angular_diameter: FloatProperty(
        name='Angular diameter',
        description='Apparent size (angle) of the light (angularDiameter)',
        default = 0,
        min = 0,
        max = 180
        )   

    # Point/sphere light, spot light
    
    # XXX use blender light's shadow_soft_size, as this will show the size
    # in the 3D light representation
    """
    radius: FloatProperty(
        name='Radius',
        description='Size of (sphere) light (radius)',
        default = 0,
        min = 0,
        max = 100
        )  
    """
        
    # Spot light
        
    opening_angle: FloatProperty(
        name='Opening angle',
        description='Full opening angle of the spot (openingAngle)',
        default = 45,
        min = 0,
        max = 180
        )  

    penumbra_angle: FloatProperty(
        name='Penumbra angle',
        description='Size of the "penumbra", the region between the rim (of the illumination cone) and full intensity of the spot; should be smaller than half of the opening angle (penumbraAngle)',
        default = 0,
        min = 0,
        max = 180   
        )      
        
        

classes = (
    RenderOspraySettingsScene,
    RenderOspraySettingsWorld,
    RenderOspraySettingsLight,
)

def register():
    from bpy.utils import register_class
    
    for cls in classes:
        register_class(cls)
        
    bpy.types.Scene.ospray = PointerProperty(type=RenderOspraySettingsScene)
    bpy.types.World.ospray = PointerProperty(type=RenderOspraySettingsWorld)
    bpy.types.Light.ospray = PointerProperty(type=RenderOspraySettingsLight)
    
    
def unregister():
    from bpy.utils import unregister_class
    
    for cls in classes:
        unregister_class(cls)
        
    del bpy.types.Scene.ospray
    del bpy.types.World.ospray
    
    