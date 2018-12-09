# Can test with blender -P <thisscript.py> -E custom_renderer
#
# https://docs.blender.org/api/2.79/bpy.types.RenderEngine.html
#
# https://github.com/LuxCoreRender/BlendLuxCore/blob/master/engine/__init__.py
#
# Luxrender discussion: http://www.luxrender.net/forum/viewtopic.php?f=11&t=11370&start=10
# https://bitbucket.org/luxrender/luxblend25/commits/branch/LuxCore_RealtimePreview

import time
import bpy
#from bgl import *

class CustomRenderEngine(bpy.types.RenderEngine):
    # These three members are used by blender to set up the
    # RenderEngine; define its internal name, visible name and capabilities.
    bl_idname = "custom_renderer"
    bl_label = "Flat Color Renderer"
    
    # Enable the availability of material preview renders
    bl_use_preview = False
    
    bl_use_shading_nodes = True
    bl_use_shading_nodes_custom = False     # If True will hide cycles shading nodes
    
    def log(self, s):
        print('[%.3f] (%s) %s' % (time.time(), id(self), s))

    
    def __init__(self):
        self.log('>>> CustomRenderEngine.__init__()')
        super(CustomRenderEngine, self).__init__()
        
        #self.texture = Buffer(GL_INT, 1)
        #glGenTextures(1, self.texture)
        #self.texture_id = self.texture[0]
        
        #self.texture_format = GL_RGBA
        
    def __del__(self):
        self.log('>>> CustomRenderEngine.__del__()')
        
    def update(self, data, depsgraph):
        """
        Export scene data for (final or material preview) render
        
        Note that this method is always called, even when re-rendering
        exactly the same scene or moving just the camera.
        """
        self.log('>>> CustomRenderEngine.update()')
        print('data', data)
        print('depsgraph', depsgraph)
        print('depsgraph mode =', depsgraph.mode)
        print('%d object instances' % len(depsgraph.object_instances))
        
        #depsgraph.debug_relations_graphviz('depsgraph.dot')
        #depsgraph.debug_stats_gnuplot('depsgraph.dat', 'script')   #XXX
        
        scene = depsgraph.scene
        camera = scene.camera
        
    # This is the only method called by blender, in this example
    # we use it to detect preview rendering and call the implementation
    # in another method.
    def render(self, depsgraph):
        """Render scene into an image"""
        self.log('>>> CustomRenderEngine.render()')
        
        scene = depsgraph.scene
        scale = scene.render.resolution_percentage / 100.0
        self.size_x = int(scene.render.resolution_x * scale)
        self.size_y = int(scene.render.resolution_y * scale)
        print("%d x %d (scale %d%%) -> %d x %d" % \
            (scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage,
            self.size_x, self.size_y))
        
        #self.size_x = 960
        #self.size_y = 540

        if self.is_preview:
            self.render_preview(depsgraph)
        else:
            self.render_scene(depsgraph)
            
    # If the two view_... methods are defined the interactive rendered
    # mode becomes available
    
    def view_update(self, context):
        """Update on data changes for viewport render"""
        self.log('>>> CustomRenderEngine.view_update()')
        
        depsgraph = context.depsgraph
        print('%d object instances' % len(depsgraph.object_instances))
        print('%d updates:' % len(depsgraph.updates))
        for update in depsgraph.updates:
            print(('ID %s, geom %s, xform %s' % (update.id, update.is_dirty_geometry, update.is_dirty_transform)))
        
        region = context.region
        view_camera_offset = list(context.region_data.view_camera_offset)
        view_camera_zoom = context.region_data.view_camera_zoom
        print(region.width, region.height)
        print(view_camera_offset, view_camera_zoom)
        
        width = region.width
        height = region.height
        channels_per_pixel = 4
        
        #self.buffer = Buffer(GL_UNSIGNED_BYTE, [width * height * channels_per_pixel])

    def view_draw(self, context):
        """Draw viewport render"""
        # Note: some changes in blender do not cause a view_update(),
        # but only a view_draw()
        self.log('>>> CustomRenderEngine.view_draw()')
        # XXX need to draw ourselves with OpenGL bgl module :-/
        region = context.region
        view_camera_offset = list(context.region_data.view_camera_offset)
        view_camera_zoom = context.region_data.view_camera_zoom
        print(region.width, region.height)
        print(view_camera_offset, view_camera_zoom)
        
        
        
        
    # Nodes
    
    def update_script_node(self, node):
        """Compile shader script node"""
        self.log('>>> CustomRenderEngine.update_script_node()')
        
    # Implementation of the actual rendering

    # In this example, we fill the preview renders with a flat green color.
    def render_preview(self, depsgraph):
        self.log('>>> CustomRenderEngine.render_preview()')
        
        pixel_count = self.size_x * self.size_y

        # The framebuffer is defined as a list of pixels, each pixel
        # itself being a list of R,G,B,A values
        green_rect = [[0.0, 1.0, 0.0, 1.0]] * pixel_count

        # Here we write the pixel values to the RenderResult
        result = self.begin_result(0, 0, self.size_x, self.size_y)
        layer = result.layers[0].passes["Combined"]
        layer.rect = green_rect
        self.end_result(result)
        
    # In this example, we fill the full renders with a flat blue color.
    def render_scene(self, depsgraph):
        self.log('>>> CustomRenderEngine.render_scene()')
        
        pixel_count = self.size_x * self.size_y

        # The framebuffer is defined as a list of pixels, each pixel
        # itself being a list of R,G,B,A values
        blue_rect = [[0.0, 0.0, 1.0, 1.0]] * pixel_count

        # Here we write the pixel values to the RenderResult
        result = self.begin_result(0, 0, self.size_x, self.size_y)
        layer = result.layers[0].passes["Combined"]
        layer.rect = blue_rect
        self.end_result(result)


def register():
    # Register the RenderEngine
    bpy.utils.register_class(CustomRenderEngine)

    # RenderEngines also need to tell UI Panels that they are compatible
    # Otherwise most of the UI will be empty when the engine is selected.
    # In this example, we need to see the main render image button and
    # the material preview panel.
    from bl_ui import (
            properties_render,
            properties_material,
            )
    
    #properties_render.RENDER_PT_evee_render.COMPAT_ENGINES.add(CustomRenderEngine.bl_idname)
    #properties_material.MATERIAL_PT_preview.COMPAT_ENGINES.add(CustomRenderEngine.bl_idname)


def unregister():
    bpy.utils.unregister_class(CustomRenderEngine)

    from bl_ui import (
            properties_render,
            properties_material,
            )
    
    #properties_render.RENDER_PT_render.COMPAT_ENGINES.remove(CustomRenderEngine.bl_idname)
    #properties_material.MATERIAL_PT_preview.COMPAT_ENGINES.remove(CustomRenderEngine.bl_idname)


if __name__ == "__main__":
    register()
