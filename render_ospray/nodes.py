# ./release/scripts/templates_py/custom_nodes.py
# render_povray addon
# https://devtalk.blender.org/t/custom-nodes-not-showing-as-updated-in-interactive-mode/6762/2 (appleseed addon)
# https://blenderartists.org/t/custom-cycles-nodes-with-python/670539/3
# https://github.com/appleseedhq/blenderseed
# https://blenderartists.org/t/bpy-color-ramp/545236/9
import bpy
from bpy.props import EnumProperty

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

# Node category

class OSPRayShaderNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.scene.render.engine == 'OSPRAY' \
                and context.space_data.tree_type == 'ShaderNodeTree'

# Sockets

class OSPRaySocketFloat_0_1(bpy.types.NodeSocket):
    """Float in [0,1]"""

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


class OSPRaySocketFloat_0_2(bpy.types.NodeSocket):
    """Float in [0,2]"""

    bl_idname = 'OSPRaySocketFloat_0_2'
    bl_label = 'Restricted float socket'

    default_value: bpy.props.FloatProperty(min=0, max=2, default=1)
    
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


def volume_data_callback(dummy, context):

    items = []

    for ob in context.scene.objects:
        if ob.type != 'MESH':
            continue 
        mesh = ob.data
        ospray = mesh.ospray
        if ospray.plugin_enabled and ospray.plugin_type == 'volume':
            items.append((mesh.name, mesh.name, ''))

    return items

# https://blender.stackexchange.com/a/10919/46067
# XXX how to only show the enum dropdown and not the input socket?
class OSPRaySocketVolumeData(bpy.types.NodeSocket):
    """Volume data"""

    bl_idname = 'OSPRaySocketVolumeData'
    bl_label = 'Volume data'

    volume_data: EnumProperty(
            name='Volume data',
            description='Volume data to use',
            items=volume_data_callback
            )
    
    def draw(self, context, layout, node, text):
        if self.is_linked:
            layout.label(text)
        else:
            layout.prop(self, 'volume_data')
            
    def draw_color(self, context, node):
        return (0.65, 0.65, 0.65, 1)


# Nodes

class OSPRayOutputNode(bpy.types.Node):
    """Output"""
    bl_idname = 'OSPRayOutputNode'
    bl_label = 'Output (OSPRay)'
    bl_icon = 'SOUND'

    def init(self, context):
        self.inputs.new('NodeSocketShader', 'Material')
        self.inputs.new('NodeSocketShader', 'Transfer Function')

class OSPRayVolumeTexture(bpy.types.Node):
    """Volumetric texture"""
    bl_idname = 'OSPRayVolumeTexture'
    bl_label = 'Volume texture (OSPRay)'
    bl_icon = 'SOUND'

    def init(self, context):
        # Get volume data implicitly from parent object
        #self.inputs.new('OSPRaySocketVolumeData', 'Volume data')

        sampling_rate = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Sampling rate')
        sampling_rate.default = 1

        self.inputs.new('NodeSocketColor', 'Transfer function')
        
        self.outputs.new('NodeSocketColor', 'Color')

# Materials


class OSPRayCarPaint(bpy.types.Node):
    """Car paint material"""
    bl_idname = 'OSPRayCarPaint'
    bl_label = 'Car paint (OSPRay)'
    bl_icon = 'SOUND'
    bl_color = (0, 0.7, 0, 1)           # XXX doesn't work?

    def init(self, context):
        # all inputs can be controlled using a texture
        
        base_color = self.inputs.new('NodeSocketColor', 'Base color')
        base_color.default_value = (0.8, 0.8, 0.8, 1)

        roughness = self.inputs.new('OSPRaySocketFloat_0_1', 'Roughness')    
        roughness.default_value = 0

        normal = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Normal')    
        normal.default_value = 1

        flake_density = self.inputs.new('OSPRaySocketFloat_0_1', 'Flake density')    
        flake_density.default_value = 0

        flake_scale = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Flake scale')
        flake_scale.default_value = 100

        flake_spread = self.inputs.new('OSPRaySocketFloat_0_1', 'Flake spread')    
        flake_spread.default_value = 0.3

        flake_jitter = self.inputs.new('OSPRaySocketFloat_0_1', 'Flake jitter')    
        flake_jitter.default_value = 0.75

        flake_roughness = self.inputs.new('OSPRaySocketFloat_0_1', 'Flake roughness')    
        flake_roughness.default_value = 0.3

        coat = self.inputs.new('OSPRaySocketFloat_0_1', 'Coat')    
        coat.default_value = 0

        coat_ior = self.inputs.new('OSPRaySocketFloat_IOR', 'Coat IOR')    
        coat_ior.default_value = 1.5

        coat_color = self.inputs.new('NodeSocketColor', 'Coat color')    
        coat_color.default_value = (1, 1, 1, 1)

        coat_thickness = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Coat thickness')    
        coat_thickness.default_value = 1

        coat_rougness = self.inputs.new('OSPRaySocketFloat_0_1', 'Coat roughness')    
        coat_rougness.default_value = 0

        coat_normal = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Coat normal')    
        coat_normal.default_value = 1

        flipflop_color = self.inputs.new('NodeSocketColor', 'Flipflop color')    
        flipflop_color.default_value = (1, 1, 1, 1)        

        flipflop_falloff = self.inputs.new('OSPRaySocketFloat_0_1', 'Flipflop falloff')    
        flipflop_falloff.default_value = 1
        
        self.outputs.new('NodeSocketShader', 'Material')


class OSPRayGlass(bpy.types.Node):
    """Glass"""
    bl_idname = 'OSPRayGlass'
    bl_label = 'Glass (OSPRay)'
    bl_icon = 'SOUND'
    bl_color = (0, 0.7, 0, 1)           # XXX doesn't work?

    def init(self, context):
        # all inputs, except Tf, can be controlled using a texture
        
        eta = self.inputs.new('OSPRaySocketFloat_IOR', 'Eta')
        eta.default_value = 1.5
        
        attenuation_color = self.inputs.new('NodeSocketColor', 'Attenuation color')    
        attenuation_color.default_value = (1, 1, 1, 1)
        
        attenuation_distance = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Attenuation distance') 
        attenuation_distance.default_value = 1
        
        self.outputs.new('NodeSocketShader', 'Material')


class OSPRayThinGlass(bpy.types.Node):
    """ThinGlass"""
    bl_idname = 'OSPRayThinGlass'
    bl_label = 'ThinGlass (OSPRay)'
    bl_icon = 'SOUND'
    bl_color = (0, 0.7, 0, 1)           # XXX doesn't work?

    def init(self, context):
        # all inputs, except Tf, can be controlled using a texture
        
        eta = self.inputs.new('OSPRaySocketFloat_IOR', 'Eta')
        eta.default_value = 1.5
        
        attenuation_color = self.inputs.new('NodeSocketColor', 'Attenuation color')    
        attenuation_color.default_value = (1, 1, 1, 1)
        
        attenuation_distance = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Attenuation distance') 
        attenuation_distance.default_value = 1

        thickness = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Thickness') 
        thickness.default_value = 1
        
        self.outputs.new('NodeSocketShader', 'Material')


class OSPRayLuminous(bpy.types.Node):
    """Luminous"""
    bl_idname = 'OSPRayLuminous'
    bl_label = 'Luminous (OSPRay)'
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
    bl_label = 'MetallicPaint (OSPRay)'
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


class OSPRayOBJMaterial(bpy.types.Node):
    """OBJMaterial"""
    bl_idname = 'OSPRayOBJMaterial'
    bl_label = 'OBJMaterial (OSPRay)'
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
        transparency_filter_color = self.inputs.new('NodeSocketColor', 'Transparency color')    
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


class OSPRayPrincipled(bpy.types.Node):
    """Principled material"""
    bl_idname = 'OSPRayPrincipled'
    bl_label = 'Principled (OSPRay)'
    bl_icon = 'SOUND'
    bl_color = (0, 0.7, 0, 1)           # XXX doesn't work?

    def init(self, context):
        # all inputs can be controled using a texture
        
        base_color = self.inputs.new('NodeSocketColor', 'Base color')
        base_color.default_value = (0.8, 0.8, 0.8, 1)
        
        edge_color = self.inputs.new('NodeSocketColor', 'Edge color')
        edge_color.default_value = (1, 1, 1, 1)

        metallic = self.inputs.new('OSPRaySocketFloat_0_1', 'Metallic')    
        metallic.default_value = 0

        diffuse = self.inputs.new('OSPRaySocketFloat_0_1', 'Diffuse')    
        diffuse.default_value = 1
        
        specular = self.inputs.new('OSPRaySocketFloat_0_1', 'Specular')    
        specular.default_value = 1

        ior = self.inputs.new('OSPRaySocketFloat_IOR', 'IOR')    
        ior.default_value = 1

        transmission = self.inputs.new('OSPRaySocketFloat_0_1', 'Transmission')    
        transmission.default_value = 0

        transmission_color = self.inputs.new('NodeSocketColor', 'Transmission color')
        transmission_color.default_value = (1, 1, 1, 1)

        transmission_depth = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Transmission depth')    
        transmission_depth.default_value = 1

        roughness = self.inputs.new('OSPRaySocketFloat_0_1', 'Roughness')    
        roughness.default_value = 0

        anisotropy = self.inputs.new('OSPRaySocketFloat_0_1', 'Anisotropy')    
        anisotropy.default_value = 0

        rotation = self.inputs.new('OSPRaySocketFloat_0_1', 'Rotation')    
        rotation.default_value = 0

        normal = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Normal')    
        normal.default_value = 1

        base_normal = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Base normal')    
        base_normal.default_value = 1

        thin = self.inputs.new('NodeSocketBool', 'Thin')    
        thin.default_value = 0
        
        thickness = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Thickness')    
        thickness.default_value = 1

        backlight = self.inputs.new('OSPRaySocketFloat_0_2', 'Backlight')    
        backlight.default_value = 1

        coat = self.inputs.new('OSPRaySocketFloat_0_1', 'Coat')    
        coat.default_value = 0

        coat_ior = self.inputs.new('OSPRaySocketFloat_IOR', 'Coat IOR')    
        coat_ior.default_value = 1.5

        coat_color = self.inputs.new('NodeSocketColor', 'Coat color')    
        coat_color.default_value = (1, 1, 1, 1)

        coat_thickness = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Coat thickness')    
        coat_thickness.default_value = 1

        coat_rougness = self.inputs.new('OSPRaySocketFloat_0_1', 'Coat roughness')    
        coat_rougness.default_value = 0

        coat_normal = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Coat normal')    
        coat_normal.default_value = 1

        sheen = self.inputs.new('OSPRaySocketFloat_0_1', 'Sheen')    
        sheen.default_value = 0

        sheen_color = self.inputs.new('NodeSocketColor', 'Sheen color')    
        sheen_color.default_value = (1, 1, 1, 1)

        sheen_tint = self.inputs.new('OSPRaySocketFloat_NonNegative', 'Sheen tint')    
        sheen_tint.default_value = 0

        sheen_roughness = self.inputs.new('OSPRaySocketFloat_0_1', 'Sheen roughness')    
        sheen_roughness.default_value = 0.2

        opacity = self.inputs.new('OSPRaySocketFloat_0_1', 'Opacity')    
        opacity.default_value = 1
        
        self.outputs.new('NodeSocketShader', 'Material')


node_classes = (
    # Sockets
    OSPRaySocketFloat_0_1,
    OSPRaySocketFloat_0_2,
    OSPRaySocketFloat_NonNegative,
    OSPRaySocketFloat_IOR,
    OSPRaySocketVolumeData,

    # General nodes
    OSPRayOutputNode,

    # Material nodes
    OSPRayCarPaint,
    OSPRayGlass,
    OSPRayThinGlass,
    OSPRayLuminous,
    OSPRayMetallicPaint,
    OSPRayOBJMaterial,
    OSPRayPrincipled,

    # Texture nodes
    OSPRayVolumeTexture,
)

node_categories = [

    OSPRayShaderNodeCategory("SHADEROUTPUT", "OSPRay", items=[
        NodeItem("OSPRayOutputNode"),
        
        NodeItem('OSPRayCarPaint'),
        NodeItem('OSPRayGlass'),
        NodeItem('OSPRayThinGlass'),
        NodeItem('OSPRayLuminous'),
        NodeItem('OSPRayMetallicPaint'),
        NodeItem('OSPRayOBJMaterial'),
        NodeItem('OSPRayPrincipled'),
        
        NodeItem('OSPRayVolumeTexture'),
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

