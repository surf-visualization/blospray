import bpy

from bpy.types import (Panel,
                       Operator,
                       PropertyGroup,
                       )                       

   
class RENDER_PT_OSPRAY_CONNECTION(Panel):
    bl_idname = 'RENDER_PT_OSPRAY_CONNECTION'
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
        col.prop(ospray, 'host', text='Host') 
        col.prop(ospray, 'port', text='Port') 
        
class RENDER_PT_OSPRAY_RENDERING(Panel):
    bl_idname = 'RENDER_PT_OSPRAY'
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
        col.prop(ospray, 'renderer', text='Type') 
        col.prop(ospray, 'samples', text='Samples') 
        col.prop(ospray, 'ao_samples', text='AO Samples') 
        col.prop(ospray, 'shadows_enabled', text='Shadows') 


  
class WORLD_PT_OSPRAY(Panel):
    bl_idname = 'WORLD_PT_OSPRAY'
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
        col.prop(ospray, 'background_color', text='Background') 
        col.prop(ospray, 'ambient_color', text='Ambient color') 
        col.prop(ospray, 'ambient_intensity', text='Ambient intensity') 
        

classes = (
    RENDER_PT_OSPRAY_CONNECTION,
    RENDER_PT_OSPRAY_RENDERING,
    WORLD_PT_OSPRAY,
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
        #'DATA_PT_camera_dof_aperture',     # This is a GPU-specific panel, the regular aperture settings are cycles specific
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
        #'DATA_PT_EEVEE_light',
        #'DATA_PT_EEVEE_shadow',
        #'DATA_PT_EEVEE_shadow_cascaded_shadow_map',
        #'DATA_PT_EEVEE_shadow_contact',
        'DATA_PT_area',
        'DATA_PT_context_light',
        'DATA_PT_custom_props_light',
        'DATA_PT_falloff_curve',
        'DATA_PT_light',
        #'DATA_PT_preview',
        'DATA_PT_spot'
    ],
    
    properties_data_mesh : [
        #'DATA_PT_context_mesh',
        'DATA_PT_custom_props_mesh',
        #'DATA_PT_customdata',
        #'DATA_PT_face_maps',
        'DATA_PT_normals',
        #'DATA_PT_shape_keys',
        #'DATA_PT_texture_space',
        #'DATA_PT_uv_texture',
        'DATA_PT_vertex_colors',
        #'DATA_PT_vertex_groups',
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
    
    properties_world : [
        #'EEVEE_WORLD_PT_mist',
        #'EEVEE_WORLD_PT_surface',
        'WORLD_PT_context_world',
        'WORLD_PT_custom_props',
        'WORLD_PT_viewport_display',
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

