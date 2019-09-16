# ======================================================================== #
# BLOSPRAY - OSPRay as a Blender render engine                             #
# Paul Melis, SURFsara <paul.melis@surfsara.nl>                            #
# Render engine definition                                                 #
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

bl_info = {
    "name": "OSPRay",
    "author": "Paul Melis",
    "version": (0, 0, 2),
    "blender": (2, 80, 0),
    "location": "Render > Engine > OSPRay",
    "description": "OSPRay integration for blender",
    "warning": "Alpha quality",
    "category": "Render"}
    
if "bpy" in locals():
    import importlib
    if 'ui' in locals():
        importlib.reload(ui)
    if 'properties' in locals():
        importlib.reload(properties)
    if 'connection' in locals():
        importlib.reload(connection)
    #imp.reload(render)
    #imp.reload(update_files)
    
import sys, traceback
import bpy
from .connection import Connection

HOST = 'localhost'
PORT = 5909
    
class OsprayRenderEngine(bpy.types.RenderEngine):
    bl_idname = "OSPRAY"
    bl_label = "OSPRay"
    # See ./source/blender/makesrna/intern/rna_render.c
    # scripts/startup/nodeitems_builtins.py defines the builtin nodes AND
    # for which renderers they are shown
    bl_use_preview = False                  # Availability of material preview renders
    #bl_use_shading_nodes = True            # No longer available in 2.8, see 095df1ac217f3e43667f94ab189a67175bcd7af5
    bl_use_shading_nodes_custom = False     # If True will hide cycles shading nodes
    #bl_use_eevee_viewport = True
    
    def __init__(self):
        print('>>> OsprayRenderEngine.__init__()')
        super(OsprayRenderEngine, self).__init__()
        
    def __del__(self):
        print('>>> OsprayRenderEngine.__del__()')
        
    def update(self, data, depsgraph):
        """
        Export scene data for (final or material preview) render
        
        Note that this method is always called, even when re-rendering
        exactly the same scene or moving just the camera.
        """
        print('>>> CustomRenderEngine.update()')

        self.update_succeeded = False
        
        ospray = depsgraph.scene.ospray
        
        self.connection = Connection(self, ospray.host, ospray.port)

        if not self.connection.connect():        
            self.report({'ERROR'}, 'Failed to connect to server')
            return 

        try:
            self.connection.update(data, depsgraph)
        except:
            exc_type, exc_value, exc_traceback = sys.exc_info()            
            lines = traceback.format_exception(exc_type, exc_value, exc_traceback)
            print('ERROR: Exception while updating scene on server:')
            print(''.join(lines))
            self.report({'ERROR'}, 'Exception while updating scene on server: %s' % sys.exc_info()[0])
            return 

        # XXX if we fail connecting here there's no way to 
        # signal to blender that it should not subsequently call render()        

        self.update_succeeded = True
        
    # This is the only method called by blender, in this example
    # we use it to detect preview rendering and call the implementation
    # in another method.
    def render(self, depsgraph):
        """Render scene into an image"""
        print('>>> OsprayRenderEngine.render()')

        if not self.update_succeeded:
            return
        
        try:
            self.connection.render(depsgraph)
            self.connection.close()
        except:
            self.report({'ERROR'}, 'Exception while rendering scene on server: %s' % sys.exc_info()[0])

    # If the two view_... methods are defined the interactive rendered
    # mode becomes available
    
    if False:
    
        def view_update(self, context, depsgraph):
            """Update on data changes for viewport render"""
            print('>>> OsprayRenderEngine.view_update()')
            
            region = context.region
            view_camera_offset = list(context.region_data.view_camera_offset)
            view_camera_zoom = context.region_data.view_camera_zoom
            print(region.width, region.height)
            print(view_camera_offset, view_camera_zoom)
            
            width = region.width
            height = region.height
            channels_per_pixel = 4
            
            self.buffer = Buffer(GL_UNSIGNED_BYTE, [width * height * channels_per_pixel])

        def view_draw(self, context, depsgraph):
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
    
    def NO_update_script_node(self, node):
        """Compile shader script node"""
        print('>>> OsprayRenderEngine.update_script_node()')
        
    # Implementation of the actual rendering

    # In this example, we fill the preview renders with a flat green color.
    def render_preview(self, depsgraph):
        print('>>> OsprayRenderEngine.render_preview()')
        
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
        print('>>> OsprayRenderEngine.render_scene()')
        
        pixel_count = self.size_x * self.size_y

        # The framebuffer is defined as a list of pixels, each pixel
        # itself being a list of R,G,B,A values
        blue_rect = [[0.0, 0.0, 1.0, 1.0]] * pixel_count

        # Here we write the pixel values to the RenderResult
        result = self.begin_result(0, 0, self.size_x, self.size_y)
        layer = result.layers[0].passes["Combined"]
        layer.rect = blue_rect
        self.end_result(result)



classes = (
    OsprayRenderEngine,
)

def register():
    from bpy.utils import register_class
    
    from . import operators
    from . import properties
    from . import ui
    from . import nodes
    
    properties.register()
    operators.register()
    ui.register()
    nodes.register()
    
    for cls in classes:
        register_class(cls)
    
    
def unregister():
    from bpy.utils import unregister_class
    
    from . import properties
    from . import operators
    from . import ui
    from . import nodes
    
    properties.unregister()
    operators.unregister()
    ui.unregister()
    nodes.unregister()
    
    for cls in classes:
        unregister_class(cls)
        
    
if __name__ == "__main__":
    register()        
    