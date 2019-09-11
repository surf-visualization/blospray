# render_povray addon
# https://devtalk.blender.org/t/custom-nodes-not-showing-as-updated-in-interactive-mode/6762/2 (appleseed addon)
# https://github.com/appleseedhq/blenderseed
import bpy
import nodeitems_utils
from nodeitems_utils import NodeCategory, NodeItem

class OSPRayShaderNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        print(context.space_data.tree_type)
        #return context.scene.render.engine == 'OSPRAY'
        return context.space_data.tree_type == 'ObjectNodeTree'


class PovraySocketFloat_0_1(bpy.types.NodeSocket):
    bl_idname = 'PovraySocketFloat_0_1'
    bl_label = 'Povray Socket'
    default_value: bpy.props.FloatProperty(description="Input node Value_0_1",min=0,max=1,default=0)
    def draw(self, context, layout, node, text):
        if self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text, slider=True)

    def draw_color(self, context, node):
        return (0.5, 0.7, 0.7, 1)


class MyCustomSocket(bpy.types.NodeSocket):
    # Description string
    '''Custom node socket type'''
    # Optional identifier string. If not explicitly defined, the python class name is used.
    bl_idname = 'CustomSocketType'
    # Label for nice name display
    bl_label = 'Custom Node Socket'
    # Socket color
    bl_color = (1.0, 0.4, 0.216, 0.5)

    def draw(self, context, layout, node, text):
        pass
    
    def draw_color(self, context, node):
        return (1, 0, 0, 1)    

class MyCustomTree(bpy.types.NodeTree):
    # Description string
    '''A custom node tree type that will show up in the node editor header'''
    # Optional identifier string. If not explicitly defined, the python class name is used.
    bl_idname = 'CustomTreeType'
    # Label for nice name display
    bl_label = 'Custom Node Tree'
    # Icon identifier
    # Note: If no icon is defined, the node tree will not show up in the editor header!
    #       This can be used to make additional tree types for groups and similar nodes
    bl_icon = 'NODETREE'
    
    @classmethod
    def poll(cls, context):
        #return context.scene.render.engine == 'OSPRAY'    
        return True
        
class MyCustomNode(bpy.types.Node, MyCustomTree):
    # Description string
    '''A custom node'''
    # Optional identifier string. If not explicitly defined, the python class name is used.
    bl_idname = 'CustomNodeType'
    # Label for nice name display
    bl_label = 'Custom Node'
    # Icon identifier
    bl_icon = 'SOUND'    

    def init(self, context):

        intensity=self.inputs.new('PovraySocketFloat_0_1', "Intensity")
        intensity.default_value=0.8
        albedo=self.inputs.new('NodeSocketBool', "Albedo")
        albedo.default_value=False
        brilliance=self.inputs.new('PovraySocketFloat_0_10', "Brilliance")
        brilliance.default_value=1.8
        self.inputs.new('PovraySocketFloat_0_1', "Crand")
        self.outputs.new('NodeSocketVector', "Diffuse")

    def draw_label(self):
        return "Diffuse"

class OSPRayOutputNode(bpy.types.Node):
    '''Output'''
    bl_idname = 'OSPRayOutputNode'
    bl_label = 'Output'
    bl_icon = 'SOUND'

    def init(self, context):
        #self.inputs.new('PovraySocketTexture', "Texture")
        pass

    def draw_buttons(self, context, layout):

        ob=context.object
        layout.prop(ob.pov, "object_ior",slider=True)

    def draw_buttons_ext(self, context, layout):

        ob=context.object
        layout.prop(ob.pov, "object_ior",slider=True)

    def draw_label(self):
        return "Output"

    
node_classes = (
    PovraySocketFloat_0_1,    
    MyCustomSocket, 
    
    MyCustomNode, 
    OSPRayOutputNode,
    
    MyCustomTree,
)

node_categories = [

    OSPRayShaderNodeCategory("SHADEROUTPUT", "Output", items=[
        NodeItem("OSPRayOutputNode"),
        NodeItem('CustomNodeType'),
    ]),

]


def register():
    from bpy.utils import register_class
    print("!")
    for cls in node_classes:
        register_class(cls)
        
    nodeitems_utils.register_node_categories("OSPRAY_NODES", node_categories)

def unregister():
    from bpy.utils import unregister_class
    
    for cls in classes:
        unregister_class(cls)
        
    nodeitems_utils.unregister_node_categories("OSPRAY_NODES")


if __name__ == "__main__":
    register()

