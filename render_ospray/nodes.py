# ./release/scripts/templates_py/custom_nodes.py
# render_povray addon
# https://devtalk.blender.org/t/custom-nodes-not-showing-as-updated-in-interactive-mode/6762/2 (appleseed addon)
# https://github.com/appleseedhq/blenderseed
import bpy
import nodeitems_utils
from nodeitems_utils import NodeCategory, NodeItem

"""
o = object
len(o.material_slots)

m = o.data
mat = m.materials[0]
assert mat.use_nodes 

node_tree = mat.node_tree
assert t.type == 'SHADER'
n = t.nodes[...]
n.inputs[]
n.outputs[]
n.type

i.default_value     # local value is updated when user edits
i/o.is_linked


l = t.links[0]

"""

class OSPRayShaderNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        print(context.space_data.tree_type)
        #return context.scene.render.engine == 'OSPRAY'
        return context.space_data.tree_type == 'ShaderNodeTree'


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
        layout.label(text=text)
    
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


class OSPRaySocketFloat_0_1(bpy.types.NodeSocket):
    """Unit float"""

    bl_idname = 'OSPRaySocketFloat_0_1'
    bl_label = 'Unit float socket'

    default_value: bpy.props.FloatProperty(min=0, max=1, default=1)
    
    def draw(self, context, layout, node, text):
        if self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text, slider=True)

    def draw_color(self, context, node):
        return (0.65, 0.65, 0.65, 1)


class OSPRaySocketFloat_NonNegative(bpy.types.NodeSocket):
    """Float >= 0"""

    bl_idname = 'OSPRaySocketFloat_NonNegative'
    bl_label = 'Float >= 0 socket'

    default_value: bpy.props.FloatProperty(min=0, default=1)
    
    def draw(self, context, layout, node, text):
        if self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text, slider=True)

    def draw_color(self, context, node):
        return (0.65, 0.65, 0.65, 1)


class OSPRaySocketFloat_IOR(bpy.types.NodeSocket):
    """Index of refraction"""

    bl_idname = 'OSPRaySocketFloat_IOR'
    bl_label = 'IOR float'

    default_value: bpy.props.FloatProperty(min=1, max=3, default=1.45)
    
    def draw(self, context, layout, node, text):
        if self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "default_value", text=text, slider=True)
            
    def draw_color(self, context, node):
        return (0.65, 0.65, 0.65, 1)



class OSPRayOutputNode(bpy.types.Node):
    """Output"""
    bl_idname = 'OSPRayOutputNode'
    bl_label = 'Output'
    bl_icon = 'SOUND'

    def init(self, context):
        self.inputs.new('NodeSocketShader', 'Material')


class OSPRayOBJMaterial(bpy.types.Node):
    """OBJMaterial"""
    bl_idname = 'OSPRayOBJMaterial'
    bl_label = 'OBJMaterial'
    bl_icon = 'SOUND'
    bl_color = (0, 0.7, 0, 1)           # XXX doesn't work?

    def init(self, context):
        # all inputs, except Tf, can be controled using a texture
        
        diffuse = self.inputs.new('NodeSocketColor', 'Diffuse')
        diffuse.default_value = (0.8, 0.8, 0.8, 1.0)
        
        specular = self.inputs.new('NodeSocketColor', 'Specular')    
        specular.default_value = (0, 0, 0, 1)
        
        shininess = self.inputs.new('NodeSocketFloat', 'Shininess') 
        shininess.default_value = 10
        
        opacity = self.inputs.new('OSPRaySocketFloat_0_1', 'Opacity')  
        opacity.default_value = 1.0
        
        # path tracer only
        transparency_filter_color = self.inputs.new('NodeSocketColor', 'Transparency color (Tf)')    
        transparency_filter_color.default_value = (0, 0, 0, 1)
        
        # texture
        normal_map = self.inputs.new('NodeSocketColor', 'Normal map')    
        normal_map.hide_value = True
        
        self.outputs.new('NodeSocketShader', 'Material')

    """
    def draw_buttons(self, context, layout):
        ob=context.object
        #layout.prop(ob.pov, "object_ior",slider=True)

    def draw_buttons_ext(self, context, layout):
        ob=context.object
        #layout.prop(ob.pov, "object_ior",slider=True)

    def draw_label(self):
        return "OBJMaterial"
    """

class OSPRayGlass(bpy.types.Node):
    """Glass"""
    bl_idname = 'OSPRayGlass'
    bl_label = 'Glass'
    bl_icon = 'SOUND'
    bl_color = (0, 0.7, 0, 1)           # XXX doesn't work?

    #eta: bpy.props.FloatProperty(description='Index of refraction', min=1, max=3, default=1.5)

    def init(self, context):
        # all inputs, except Tf, can be controled using a texture
        
        eta = self.inputs.new('OSPRaySocketFloat_IOR', 'Eta')
        eta.default_value = 1.5
        
        attenuation_color = self.inputs.new('NodeSocketColor', 'Attenuation color')    
        attenuation_color.default_value = (1, 1, 1, 1)
        
        attenuation_distance = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Attenuation distance') 
        attenuation_distance.default_value = 1
        
        self.outputs.new('NodeSocketShader', 'Material')

    """
    def draw(self, context, layout, node, text):
        if self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, "eta", text=text, slider=True)
    """

    """
    def draw_buttons(self, context, layout):
        ob=context.object
        #layout.prop(ob.pov, "object_ior",slider=True)

    def draw_buttons_ext(self, context, layout):
        ob=context.object
        #layout.prop(ob.pov, "object_ior",slider=True)

    def draw_label(self):
        return "OBJMaterial"
    """


class OSPRayLuminous(bpy.types.Node):
    """Luminous"""
    bl_idname = 'OSPRayLuminous'
    bl_label = 'Luminous'
    bl_icon = 'SOUND'
    bl_color = (0, 0.7, 0, 1)           # XXX doesn't work?

    def init(self, context):
        # all inputs, except Tf, can be controled using a texture
        
        color = self.inputs.new('NodeSocketColor', 'Color')
        color.default_value = (1, 1, 1, 1)
        
        intensity = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Intensity')    
        intensity.default_value = 1
        
        transparency = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Transparency')    
        transparency.default_value = 1
        
        self.outputs.new('NodeSocketShader', 'Material')


class OSPRayMetallicPaint(bpy.types.Node):
    """Metallic paint"""
    bl_idname = 'OSPRayMetallicPaint'
    bl_label = 'MetallicPaint'
    bl_icon = 'SOUND'
    bl_color = (0, 0.7, 0, 1)           # XXX doesn't work?

    def init(self, context):
        # all inputs, except Tf, can be controled using a texture
        
        base_color = self.inputs.new('NodeSocketColor', 'Base color')
        base_color.default_value = (0.8, 0.8, 0.8, 1)
        
        flake_amount = self.inputs.new('OSPRaySocketFloat_0_1', 'Flake amount')    
        flake_amount.default_value = 0.3

        flake_color = self.inputs.new('NodeSocketColor', 'Flake color')
        flake_color.default_value = (0.8, 0.8, 0.8, 1)      # aluminum?

        flake_spread = self.inputs.new('OSPRaySocketFloat_0_1', 'Flake spread')    
        flake_spread.default_value = 0.5
        
        eta = self.inputs.new('OSPRaySocketFloat_IOR', 'Eta')    
        eta.default_value = 1.5
        
        self.outputs.new('NodeSocketShader', 'Material')


node_classes = (
    OSPRaySocketFloat_0_1,
    OSPRaySocketFloat_NonNegative,
    OSPRaySocketFloat_IOR,

    OSPRayOutputNode,
    
    OSPRayGlass,
    OSPRayLuminous,
    OSPRayMetallicPaint,
    OSPRayOBJMaterial,
)

node_categories = [

    OSPRayShaderNodeCategory("SHADEROUTPUT", "OSPRay", items=[
        NodeItem("OSPRayOutputNode"),
        
        NodeItem('OSPRayGlass'),
        NodeItem('OSPRayLuminous'),
        NodeItem('OSPRayMetallicPaint'),
        NodeItem('OSPRayOBJMaterial'),
    ]),

]

def register():
    from bpy.utils import register_class
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

