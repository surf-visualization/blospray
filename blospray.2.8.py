# Can test with 
# $ blender -P <thisscript.py> -E OSPRAY

import bpy
#from bgl import *
from mathutils import Vector

import array
from math import tan, atan, degrees
import socket
from struct import pack

import numpy

HOST = 'localhost'
PORT = 5909

class OsprayRenderEngine(bpy.types.RenderEngine):
    # These three members are used by blender to set up the
    # RenderEngine; define its internal name, visible name and capabilities.
    bl_idname = "OSPRAY"
    bl_label = "Ospray"
    
    # Enable the availability of material preview renders
    bl_use_preview = False
    
    bl_use_shading_nodes = True
    bl_use_shading_nodes_custom = False     # If True will hide cycles shading nodes
    
    def __init__(self):
        print('>>> CustomRenderEngine.__init__()')
        super(OsprayRenderEngine, self).__init__()
        
        # XXX connect here?
        
        #self.texture = Buffer(GL_INT, 1)
        #glGenTextures(1, self.texture)
        #self.texture_id = self.texture[0]
        
        #self.texture_format = GL_RGBA
            
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        
    def __del__(self):
        print('>>> CustomRenderEngine.__del__()')
        
    def update(self, data, depsgraph):
        """
        Export scene data for (final or material preview) render
        
        Note that this method is always called, even when re-rendering
        exactly the same scene or moving just the camera.
        """
        print('>>> CustomRenderEngine.update()')
        print(data)
        print(depsgraph)
        
        self.update_stats('', 'Connecting')
        self.sock.connect((HOST, PORT))
        
        self.export_scene(data, depsgraph)
        self.update_stats('', 'Exporting')
        
    # This is the only method called by blender, in this example
    # we use it to detect preview rendering and call the implementation
    # in another method.
    def render(self, depsgraph):
        """Render scene into an image"""
        print('>>> CustomRenderEngine.render()')
        
        scene = depsgraph.scene
        scale = scene.render.resolution_percentage / 100.0
        self.size_x = int(scene.render.resolution_x * scale)
        self.size_y = int(scene.render.resolution_y * scale)
        print("%d x %d (scale %d%%) -> %d x %d" % \
            (scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage,
            self.size_x, self.size_y))

        """
        #self.size_x = 960
        #self.size_y = 540

        if self.is_preview:
            self.render_preview(depsgraph)
        else:
            self.render_scene(depsgraph)
        """
        
        # Rendering already started when we sent the scene data in update()
        # Read back framebuffer (might block)
        
        self.update_stats('', 'Rendering & reading back framebuffer')

        num_pixels = self.width * self.height
        bytes_left = num_pixels * 4*4

        framebuffer = numpy.zeros(bytes_left, dtype=numpy.uint8)
        view = memoryview(framebuffer)

        while bytes_left > 0:
            n = self.sock.recv_into(view, bytes_left)
            view = view[n:]
            bytes_left -= n

        self.sock.close()
        
        pixels = framebuffer.view(numpy.float32).reshape((num_pixels, 4))
        print(pixels.shape)
        
        # Here we write the pixel values to the RenderResult
        result = self.begin_result(0, 0, self.width, self.height)
        layer = result.layers[0].passes["Combined"]
        lst = pixels.tolist()
        print(len(lst))
        layer.rect = lst
        self.end_result(result)

        
    #
    # Scene export
    #

    def export_scene(self, data, depsgraph):
        
        scene = depsgraph.scene
            
        # Camera
        
        camobj = scene.camera
        camdata = camobj.data
        cam_xform = camobj.matrix_world

        cam_pos = camobj.location
        cam_viewdir = cam_xform @ Vector((0, 0, -1)) - camobj.location
        cam_updir = cam_xform @ Vector((0, 1, 0)) - camobj.location

        # Image

        #width = 1920
        #height = 1080

        perc = scene.render.resolution_percentage
        perc = perc / 100

        self.width = int(scene.render.resolution_x * perc)
        self.height = int(scene.render.resolution_y * perc)

        print(perc)

        aspect = self.width / self.height

        # Get camera FOV

        # radians!
        hfov = camdata.angle   
        image_plane_width = 2 * tan(hfov/2)
        image_plane_height = image_plane_width / aspect
        vfov = 2*atan(image_plane_height/2)

        fovy = degrees(vfov)

        # Lights

        """
        sun_obj = bpy.data.objects['Sun']
        sun_data = sun_obj.data

        sun_dir = sun_obj.matrix_world @ Vector((0, 0, -1)) - sun_obj.location
        sun_intensity = sun_data.node_tree.nodes["Emission"].inputs[1].default_value

        ambient_obj = bpy.data.objects['Ambient']
        ambient_data = ambient_obj.data

        ambient_intensity = ambient_data.node_tree.nodes["Emission"].inputs[1].default_value
        """
        
        sun_dir = Vector((-1, -1, -1))
        sun_intensity = 1.0

        ambient_intensity = 0.4

        # Send params

        parameters = pack('<HHfffffffffffffff',
            self.width, self.height, 
            cam_pos.x, cam_pos.y, cam_pos.z,
            cam_viewdir.x, cam_viewdir.y, cam_viewdir.z,
            cam_updir.x, cam_updir.y, cam_updir.z,
            fovy,
            sun_dir.x, sun_dir.y, sun_dir.z, sun_intensity, ambient_intensity)
            
        # Send scene parameters
        
        self.sock.sendall(parameters)


            
    # If the two view_... methods are defined the interactive rendered
    # mode becomes available
    
    def view_update(self, context):
        """Update on data changes for viewport render"""
        print('>>> CustomRenderEngine.view_update()')
        
        region = context.region
        view_camera_offset = list(context.region_data.view_camera_offset)
        view_camera_zoom = context.region_data.view_camera_zoom
        print(region.width, region.height)
        print(view_camera_offset, view_camera_zoom)
        
        width = region.width
        height = region.height
        channels_per_pixel = 4
        
        self.buffer = Buffer(GL_UNSIGNED_BYTE, [width * height * channels_per_pixel])

    def view_draw(self, context):
        """Draw viewport render"""
        # Note: some changes in blender do not cause a view_update(),
        # but only a view_draw()
        print('>>> CustomRenderEngine.view_draw()')
        # XXX need to draw ourselves with OpenGL bgl module :-/
        region = context.region
        view_camera_offset = list(context.region_data.view_camera_offset)
        view_camera_zoom = context.region_data.view_camera_zoom
        print(region.width, region.height)
        print(view_camera_offset, view_camera_zoom)
        
    # Nodes
    
    def update_script_node(self, node):
        """Compile shader script node"""
        print('>>> CustomRenderEngine.update_script_node()')
        
    # Implementation of the actual rendering

    # In this example, we fill the preview renders with a flat green color.
    def render_preview(self, depsgraph):
        print('>>> CustomRenderEngine.render_preview()')
        
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
        print('>>> CustomRenderEngine.render_scene()')
        
        pixel_count = self.size_x * self.size_y

        # The framebuffer is defined as a list of pixels, each pixel
        # itself being a list of R,G,B,A values
        blue_rect = [[0.0, 0.0, 1.0, 1.0]] * pixel_count

        # Here we write the pixel values to the RenderResult
        result = self.begin_result(0, 0, self.size_x, self.size_y)
        layer = result.layers[0].passes["Combined"]
        layer.rect = blue_rect
        self.end_result(result)




#
# Class registration
#

def register():
    # Register the RenderEngine
    bpy.utils.register_class(OsprayRenderEngine)

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
    bpy.utils.unregister_class(OsprayRenderEngine)

    from bl_ui import (
            properties_render,
            properties_material,
            )
    
    #properties_render.RENDER_PT_render.COMPAT_ENGINES.remove(CustomRenderEngine.bl_idname)
    #properties_material.MATERIAL_PT_preview.COMPAT_ENGINES.remove(CustomRenderEngine.bl_idname)


if __name__ == "__main__":
    register()
