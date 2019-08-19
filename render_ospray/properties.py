# ======================================================================== #
# BLOSPRAY - OSPRay as a Blender render engine                             #
# Paul Melis, SURFsara <paul.melis@surfsara.nl>                            #
# Property definition                                                      #
# ======================================================================== #
# Copyright 2018-2019 SURFsara                                             #
#                                                                          #
# Licensed under the Apache License, Version 2.0 (the "License");          #
# you may not use this file except in compliance with the License.         #
# You may obtain a copy of the License at                                  #
#                                                                          #
#     http://www.apache.org/licenses/LICENSE-2.0                           #
#                                                                          #
# Unless required by applicable law or agreed to in writing, software      #
# distributed under the License is distributed on an "AS IS" BASIS,        #
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. #
# See the License for the specific language governing permissions and      #
# limitations under the License.                                           #
# ======================================================================== #

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
        
    background_color: FloatVectorProperty(  
        name="Background color",
        subtype='COLOR',
        size=4,
        default=(1.0, 1.0, 1.0, 1.0),        
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
        
        
class RenderOspraySettingsObject(PropertyGroup):
    
    representation: EnumProperty(
        name='Representation',
        description='Representation',
        items=[ ('regular', 'Regular (Blender)', 'Regular Blender geometry'),
                ('geometry', 'Geometry', 'Linked data must have geometry plugin set'),
                ('volume', 'Volume', 'Linked data must have volume plugin set'),
                ('volume_slices', 'Slices', 'Linked data must have volume plugin set'),
                ('volume_isosurfaces', 'Isosurfaces', 'Linked data must have volume plugin set'),
                #('spheres', 'Spheres', ''),
                #('cylinders', 'Cylinders', ''),                
               ]
        )   
        
        
class RenderOspraySettingsMesh(PropertyGroup):
        
    plugin: StringProperty(
        name='Plugin',
        description='Plugin to use server-side for creating this "object"',
        default='',
        maxlen=64,
        ) 
        
    # Properties for a mesh representing an OSPRay volume
        
    #voxelrange
    
    gradient_shading: BoolProperty(
        name='Gradient shading',
        description='Render with surface shading wrt. to normalized gradient',
        default = False
        )
        
    pre_integration: BoolProperty(
        name='Pre-integration',
        description='Use pre-integration for transfer function lookups',
        default = False
        )
   
    single_shade: BoolProperty(
        name='Single shade',
        description='Shade only at the point of maximum intensity',
        default = True
        )           
    
    sampling_rate: FloatProperty(
        name='Sampling rate',
        description='Sampling rate of the volume as a fraction of the cell size. This is the average number of cells per-sample taken along the ray. This is also sets the minimum step size for adaptive sampling',
        default = 0.125,
        min = 0.001,
        max = 100000
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
    RenderOspraySettingsObject,
    RenderOspraySettingsMesh,
    RenderOspraySettingsLight,
)

def register():
    from bpy.utils import register_class
    
    for cls in classes:
        register_class(cls)
        
    bpy.types.Scene.ospray = PointerProperty(type=RenderOspraySettingsScene)
    bpy.types.World.ospray = PointerProperty(type=RenderOspraySettingsWorld)
    bpy.types.Object.ospray = PointerProperty(type=RenderOspraySettingsObject)
    bpy.types.Mesh.ospray = PointerProperty(type=RenderOspraySettingsMesh)
    bpy.types.Light.ospray = PointerProperty(type=RenderOspraySettingsLight)
    
    
def unregister():
    from bpy.utils import unregister_class
    
    for cls in classes:
        unregister_class(cls)
        
    del bpy.types.Scene.ospray
    del bpy.types.World.ospray
    del bpy.types.Object.ospray
    del bpy.types.Mesh.ospray
    del bpy.types.Light.ospray
    
    