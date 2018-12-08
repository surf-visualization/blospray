import bpy

from bpy.types import (Panel,
                       Operator,
                       PropertyGroup,
                       )

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
        
        scene = context.scene
        ospray = scene.ospray
        
        layout.prop(ospray, 'renderer', text='') 
