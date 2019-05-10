bl_info = {
    "name": "OSPRay",
    "author": "Paul Melis",
    "version": (0, 0, 1),
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
    
import bpy
from .connection import Connection

HOST = 'localhost'
PORT = 5909
    
class OsprayRenderEngine(bpy.types.RenderEngine):
    bl_idname = "OSPRAY"
    bl_label = "OSPRay"
    bl_use_preview = False                  # Availability of material preview renders
    bl_use_shading_nodes = True             # XXX?
    bl_use_shading_nodes_custom = False     # If True will hide cycles shading nodes
    
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
        
        ospray = depsgraph.scene.ospray
        
        self.connection = Connection(self, ospray.host, ospray.port)

        self.connection.update(data, depsgraph)
        
    # This is the only method called by blender, in this example
    # we use it to detect preview rendering and call the implementation
    # in another method.
    def render(self, depsgraph):
        """Render scene into an image"""
        print('>>> OsprayRenderEngine.render()')
        
        self.connection.render(depsgraph)
        
        self.connection.close()

    # If the two view_... methods are defined the interactive rendered
    # mode becomes available
    
    def NO_view_update(self, context):
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

    def NO_view_draw(self, context):
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
    
    from . import properties
    from . import ui
    
    properties.register()
    ui.register()
    
    for cls in classes:
        register_class(cls)
    
    
def unregister():
    from bpy.utils import unregister_class
    
    from . import properties
    from . import ui
    
    properties.unregister()
    ui.unregister()
    
    for cls in classes:
        unregister_class(cls)
        
    
if __name__ == "__main__":
    register()        
    