# ======================================================================== #
# BLOSPRAY - OSPRay as a Blender render engine                             #
# Paul Melis, SURFsara <paul.melis@surfsara.nl>                            #
# UI definition                                                            #
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

from bpy.types import (Panel,
                       Operator,
                       PropertyGroup,
                       )                       
 
class OSPRAY_RENDER_PT_connection(Panel):
    #bl_idname = 'OSPRAY_RENDER_PT_connection'
    bl_label = 'Connection'
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
        col.prop(ospray, 'host') 
        col.prop(ospray, 'port') 
        col.separator()
        col.operator('ospray.get_server_state')
                
        
class OSPRAY_RENDER_PT_rendering(Panel):
    #bl_idname = 'RENDER_PT_OSPRAY'
    bl_label = 'Rendering'
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
        col.prop(ospray, 'renderer') 
        col.prop(ospray, 'samples') 
        col.prop(ospray, 'ao_samples') 
        #col.prop(ospray, 'shadows_enabled')    # XXX Removed in 2.0?


class OSPRAY_OBJECT_PT_settings(Panel):
    #bl_idname = 'OSPRAY_OBJECT_PT_settings'
    bl_label = 'OSPRay'
    bl_space_type = 'PROPERTIES'   
    bl_region_type = 'WINDOW'    
    bl_context = 'object'  
    bl_options = {'DEFAULT_CLOSED'}
    
    COMPAT_ENGINES = {'OSPRAY'}
    
    @classmethod
    def poll(cls, context):
        obj = context.object
        return ((context.engine in cls.COMPAT_ENGINES) and 
                obj and (obj.type in {'MESH'}))
                
    def draw_header(self, context):
        self.layout.prop(context.object.ospray, 'ospray_override', text="")                
    
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        #layout.use_property_decorate = False
        
        obj = context.object
        ospray = obj.ospray
        
        # XXX align labels to the left
        col = layout.column(align=True)
        #col.prop(ospray, 'ospray_override')


class OSPRAY_OBJECT_PT_volume(Panel):
    #bl_idname = 'OSPRAY_OBJECT_PT_volume'
    bl_label = 'Volume'
    bl_space_type = 'PROPERTIES'   
    bl_region_type = 'WINDOW'    
    bl_context = 'object'  
    bl_parent_id = 'OSPRAY_OBJECT_PT_settings'
    
    COMPAT_ENGINES = {'OSPRAY'}
    
    @classmethod
    def poll(cls, context):
        if context.engine not in cls.COMPAT_ENGINES:
            return False
            
        obj = context.object
        if obj is None or (obj.type not in {'MESH'}):
            return False
            
        mesh = obj.data
        return mesh.ospray.plugin_type == 'volume'
    
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        #layout.use_property_decorate = False
        
        obj = context.object
        ospray = obj.ospray
        
        # XXX align labels to the left
        col = layout.column(align=True)
        col.prop(ospray, 'volume_usage', expand=True)
        col.separator()
        # XXX only show when volume is attached
        #col.prop(ospray, 'gradient_shading')
        #col.prop(ospray, 'pre_integration')
        #col.prop(ospray, 'single_shade')
        col.prop(ospray, 'sampling_rate')
    
    
class OSPRAY_MESH_PT_data(Panel):
    #bl_idname = 'OSPRAY_MESH_PT_data'
    bl_label = 'OSPRay'
    bl_space_type = 'PROPERTIES'   
    bl_region_type = 'WINDOW'    
    bl_context = 'data'  
    bl_options = {'DEFAULT_CLOSED'}
    
    COMPAT_ENGINES = {'OSPRAY'}
    
    @classmethod
    def poll(cls, context):
        mesh = context.mesh
        return ((context.engine in cls.COMPAT_ENGINES) and mesh)
            
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        
        mesh = context.mesh
        ospray = mesh.ospray
        
        # XXX plugin panel?
        col = layout.column(align=True)
        col.prop(ospray, 'plugin_enabled')
        col.prop(ospray, 'plugin_type')
        col.prop(ospray, 'plugin_name')

        col.separator()
        # XXX only show this mesh from "ospray-enabled" meshes
        # XXX only enable after sync with server once?
        col.operator('ospray.update_mesh_bound')

    
"""
class OSPRAY_MESH_PT_volume(Panel):
    #bl_idname = 'OSPRAY_MESH_PT_volume'
    bl_label = 'OSPRay Volume'
    bl_space_type = 'PROPERTIES'   
    bl_region_type = 'WINDOW'    
    bl_context = 'data'  
    
    COMPAT_ENGINES = {'OSPRAY'}
    
    @classmethod
    def poll(cls, context):
        obj = context.object
        mesh = context.mesh
        return ((context.engine in cls.COMPAT_ENGINES) and  mesh and (mesh.ospray.plugin_type in {'volume'}))
            
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        
        mesh = context.mesh
        ospray = mesh.ospray
"""        
    
    
class OSPRAY_LIGHT_PT_data(Panel):
    #bl_idname = 'OSPRAY_LIGHT_PT_data'
    bl_label = 'Light'
    bl_space_type = 'PROPERTIES'   
    bl_region_type = 'WINDOW'    
    bl_context = 'data'  
    
    COMPAT_ENGINES = {'OSPRAY'}
    
    @classmethod
    def poll(cls, context):
        light = context.light
        engine = context.engine
        return light and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        blender_light = context.light
        ospray_light = blender_light.ospray

        col = layout.column()            

        col.prop(blender_light, "color")
        col.prop(ospray_light, "intensity", slider=False)
        col.prop(ospray_light, "visible")
        
        if blender_light.type in {'SUN'}:
            col.prop(ospray_light, "angular_diameter")

        if blender_light.type in {'POINT', 'SPOT'}:
            col.prop(blender_light, "shadow_soft_size")

        if blender_light.type in {'SPOT'}:
            col.prop(blender_light, "spot_size")
            col.prop(blender_light, "spot_blend")
            col.prop(blender_light, "show_cone")
            
        if blender_light.type in {'AREA'}:
            col.prop(blender_light, "size")     # XXX why not size_x?
            col.prop(blender_light, "size_y")
  
  
class OSPRAY_WORLD_PT_lighting(Panel):
    #bl_idname = 'OSPRAY_WORLD_PT_lighting'
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
        col.prop(ospray, 'background_color') 
        col.prop(ospray, 'ambient_color') 
        col.prop(ospray, 'ambient_intensity') 
        

classes = (
    OSPRAY_RENDER_PT_connection,
    OSPRAY_RENDER_PT_rendering,
    OSPRAY_OBJECT_PT_settings,
    OSPRAY_OBJECT_PT_volume,
    OSPRAY_MESH_PT_data,
    #OSPRAY_MESH_PT_volume,
    OSPRAY_LIGHT_PT_data,
    OSPRAY_WORLD_PT_lighting,
)

from bl_ui import (
        properties_data_camera,
        properties_data_empty,
        properties_data_light,
        properties_data_mesh,
        properties_data_modifier,
        properties_material,
        properties_object,
        properties_output,
        properties_render,
        properties_scene,
        properties_texture,
        properties_view_layer,
        properties_workspace,
        properties_world
        )

enabled_panels = {

    properties_data_camera : [
        #'CAMERA_PT_presets',
        #'DATA_PT_camera',
        #'DATA_PT_camera_background_image',
        'DATA_PT_camera_display',
        'DATA_PT_camera_dof',
        'DATA_PT_camera_dof_aperture',     # This is a GPU-specific panel, the regular aperture settings are cycles specific
        #'DATA_PT_camera_safe_areas',
        #'DATA_PT_camera_stereoscopy',
        'DATA_PT_context_camera',
        'DATA_PT_custom_props_camera',
        'DATA_PT_lens',
        #'SAFE_AREAS_PT_presets'
    ],
    
    properties_data_empty : [
        'DATA_PT_empty'
    ],
    
    properties_data_light : [
        # Enable this one, as it is the parent of a number of panels, such DATA_PT_spot.
        # If not enabled those panels won't show up. However, this shows a number of
        # light settings that are not appropriate for OSPRay, such as point.specular_factor
        #'DATA_PT_EEVEE_light',      
        #'DATA_PT_EEVEE_shadow',
        #'DATA_PT_EEVEE_shadow_cascaded_shadow_map',
        #'DATA_PT_EEVEE_shadow_contact',
        # Disabled, as we don't want the shape options (OSPRay only has square area lights)
        #'DATA_PT_area',
        'DATA_PT_context_light',
        'DATA_PT_custom_props_light',
        'DATA_PT_falloff_curve',
        # Shows the same light type buttons as DATA_PT_EEVEE_light, but without the specific controls
        'DATA_PT_light',
        #'DATA_PT_preview',
        #'DATA_PT_spot'           # XXX for some reason this one doesn't show up
    ],
    
    properties_data_mesh : [
        'DATA_PT_context_mesh',
        'DATA_PT_custom_props_mesh',
        #'DATA_PT_customdata',
        #'DATA_PT_face_maps',
        'DATA_PT_normals',
        #'DATA_PT_shape_keys',
        #'DATA_PT_texture_space',
        #'DATA_PT_uv_texture',
        'DATA_PT_vertex_colors',
        'DATA_PT_vertex_groups',
    ],
    
    properties_output: [
        'RENDER_PT_dimensions',
        #'RENDER_PT_encoding',
        #'RENDER_PT_encoding_audio',
        #'RENDER_PT_encoding_video',
        #'RENDER_PT_ffmpeg_presets',
        #'RENDER_PT_frame_remapping',
        'RENDER_PT_output',
        #'RENDER_PT_output_views',
        #'RENDER_PT_post_processing',
        #'RENDER_PT_presets',
        #'RENDER_PT_stamp',
        #'RENDER_PT_stamp_burn',
        #'RENDER_PT_stamp_note',
        #'RENDER_PT_stereoscopy
    ],
    
    properties_render: [
        #'RENDER_PT_context',   # XXX unclear which panel this is
    ],
    
    properties_texture: [
        'TEXTURE_PT_blend',
        'TEXTURE_PT_clouds',
        'TEXTURE_PT_colors',
        'TEXTURE_PT_colors_ramp',
        'TEXTURE_PT_context',
        'TEXTURE_PT_custom_props',
        'TEXTURE_PT_distortednoise',
        'TEXTURE_PT_image',
        'TEXTURE_PT_image_alpha',
        'TEXTURE_PT_image_mapping',
        'TEXTURE_PT_image_mapping_crop',
        'TEXTURE_PT_image_sampling',
        'TEXTURE_PT_image_settings',
        'TEXTURE_PT_influence',
        'TEXTURE_PT_magic',
        'TEXTURE_PT_mapping',
        'TEXTURE_PT_marble',
        'TEXTURE_PT_musgrave',
        'TEXTURE_PT_node',
        #'TEXTURE_PT_node_mapping',
        'TEXTURE_PT_preview',
        'TEXTURE_PT_stucci',
        'TEXTURE_PT_voronoi',
        'TEXTURE_PT_voronoi_feature_weights',
        'TEXTURE_PT_wood',
    ],
    
    properties_world : [
        #'EEVEE_WORLD_PT_mist',
        #'EEVEE_WORLD_PT_surface',
        'WORLD_PT_context_world',
        'WORLD_PT_custom_props',
        #'WORLD_PT_viewport_display',
    ]
}


def register():
    from bpy.utils import register_class
    
    for cls in classes:
        register_class(cls)
        
    # RenderEngines need to tell UI Panels that they are compatible with them.
    # Otherwise most of the UI will be empty when the engine is selected.
    
    for module, panels in enabled_panels.items():
        for panelname in panels:
            panel = getattr(module, panelname)
            if hasattr(panel, 'COMPAT_ENGINES'):
                panel.COMPAT_ENGINES.add('OSPRAY')
    
    
def unregister():
    from bpy.utils import unregister_class
    
    for cls in classes:
        unregister_class(cls)
        
    for module, panels in enabled_panels.items():
        for panelname in panels:
            panel = getattr(module, panelname)
            if hasattr(panel, 'COMPAT_ENGINES'):
                panel.COMPAT_ENGINES.remove('OSPRAY')

