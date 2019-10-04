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
    "version": (0, 0, 3),
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
import bpy, bgl
import numpy

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
    
    # Init is called whenever a new render engine instance is created. Multiple
    # instances may exist at the same time, for example for a viewport and final
    # render.    
    def __init__(self):
        print('>>> OsprayRenderEngine.__init__()')
        super(OsprayRenderEngine, self).__init__()

        self.connection = None
        self.first_view_update = True
    
        self.draw_data = None        
    
    # When the render engine instance is destroy, this is called. Clean up any
    # render engine data here, for example stopping running render threads.    
    def __del__(self):
        print('>>> OsprayRenderEngine.__del__()')
        # XXX doesn't work, apparently self.connection is no longer available here?
        #if self.connection is not None:
        #    self.connection.close()

    def connect(self, depsgraph):
        assert self.connection is None
        ospray = depsgraph.scene.ospray        
        self.connection = Connection(self, ospray.host, ospray.port)        
        return self.connection.connect()
        
    def update(self, data, depsgraph):
        """
        Export scene data for final or material preview render
        
        Note that this method is always called, even when re-rendering
        exactly the same scene or moving just the camera.
        """
        print('>>> OsprayRenderEngine.update()')

        assert not self.is_preview

        self.update_succeeded = False
        
        if not self.connect(depsgraph):        
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
        # signal to blender that it should *not* subsequently call render().
        # Seems update() and render() are always called as a pair. 
        # Strange, why have two separate methods then?
        # So we use self.update_succeeded to handle it ourselves.

        self.update_succeeded = True
        
    # This is the method called by Blender for both final renders (F12) and
    # small preview for materials, world and lights.        
    def render(self, depsgraph):
        """Render scene into an image"""
        print('>>> OsprayRenderEngine.render()')

        if not self.update_succeeded:
            return
        
        try:
            self.connection.render(depsgraph)
            self.connection.close()        
            self.connection = None
        except:
            exc_type, exc_value, exc_traceback = sys.exc_info()            
            lines = traceback.format_exception(exc_type, exc_value, exc_traceback)
            print('ERROR: Exception while rendering scene on server:')
            print(''.join(lines))
            self.report({'ERROR'}, 'Exception while rendering scene on server: %s' % sys.exc_info()[0])

    # If the two view_... methods are defined the interactive rendered
    # mode becomes available

    def _print_depsgraph_updates(self, depsgraph):
        print('--- DEPSGRAPH UPDATES '+'-'*50)

        types = ['ACTION', 'ARMATURE', 'BRUSH', 'CAMERA', 'CACHEFILE', 'CURVE', 'FONT', 'GREASEPENCIL', 'COLLECTION', 'IMAGE', 'KEY', 'LIGHT', 'LIBRARY', 'LINESTYLE', 'LATTICE', 'MASK', 'MATERIAL', 'META', 'MESH', 'MOVIECLIP', 'NODETREE', 'OBJECT', 'PAINTCURVE', 'PALETTE', 'PARTICLE', 'LIGHT_PROBE', 'SCENE', 'SOUND', 'SPEAKER', 'TEXT', 'TEXTURE', 'WINDOWMANAGER', 'WORLD', 'WORKSPACE']
        for t in types:
            if depsgraph.id_type_updated(t):
                print("Type %s updated" % t)

        print()
        
        for update in depsgraph.updates:
            print('Datablock "%s" updated (%s)' % (update.id.name, type(update.id)))
            if update.is_updated_geometry:
                print('-- geometry was updated')
            if update.is_updated_transform:
                print('-- transform was updated')
                
        print('-'*50)
        
    # For viewport renders, this method gets called once at the start and
    # whenever the scene or 3D viewport changes. This method is where data
    # should be read from Blender in the same thread. Typically a render
    # thread will be started to do the work while keeping Blender responsive.        
    def view_update(self, context, depsgraph):
        """Update on data changes for viewport render"""
        print('>>> OsprayRenderEngine.view_update()')

        if self.first_view_update:

            # Open connection
            if not self.connect(depsgraph):  
                print('ERROR(view_update): Failed to connect to server')                
                return 

            scene = depsgraph.scene
            render = scene.render
            world = scene.world        

            # Renderer type
            self.send_updated_renderer_type(scene.ospray.renderer)

            # Render settings
            render_settings = RenderSettings()
            render_settings.renderer = scene.ospray.renderer
            render_settings.type = RenderSettings.INTERACTIVE        
            render_settings.samples = scene.ospray.samples
            render_settings.max_depth = scene.ospray.max_depth
            render_settings.min_contribution = scene.ospray.min_contribution
            render_settings.variance_threshold = scene.ospray.variance_threshold
            if scene.ospray.renderer == 'scivis':
                render_settings.ao_samples = scene.ospray.ao_samples
                render_settings.ao_radius = scene.ospray.ao_radius
                render_settings.ao_intensity = scene.ospray.ao_intensity
            elif scene.ospray.renderer == 'pathtracer':
                render_settings.roulette_depth = scene.ospray.roulette_depth
                render_settings.max_contribution = scene.ospray.max_contribution
                render_settings.geometry_lights = scene.ospray.geometry_lights

            self.send_updated_render_settings(render_settings)  

            # Send complete (visible) scene
            try:
                print('Sending initial scene')
                self.connection.update(None, depsgraph)
            except:
                exc_type, exc_value, exc_traceback = sys.exc_info()            
                lines = traceback.format_exception(exc_type, exc_value, exc_traceback)
                print('ERROR(view_update): Exception while updating scene on server:')
                print(''.join(lines))
                self.report({'ERROR'}, 'Exception sending initial scene to server: %s' % sys.exc_info()[0])
                return 

            self.first_view_update = False

        else:
            #  Update scene on server
            self._print_depsgraph_updates(depsgraph)
            # XXX

    # For viewport renders, this method is called whenever Blender redraws
    # the 3D viewport. The renderer is expected to quickly draw the render
    # with OpenGL, and not perform other expensive work.
    # Blender will draw overlays for selection and editing on top of the
    # rendered image automatically.    
    def view_draw(self, context, depsgraph):
        """
        Draw viewport render

        Note: some changes in blender do not cause a view_update(),
        but only a view_draw():
        - Resizing the 3D editor 
        """
        print('>>> OsprayRenderEngine.view_draw()')

        assert len(depsgraph.updates) == 0

        scene = depsgraph.scene

        # We only know the viewport resolution for certain here

        region = context.region
        view3d = context.space_data

        # Get viewport dimensions
        dimensions = region.width, region.height

        self.connection.send_updated_framebuffer_settings(region.width, region.height, OSP_FB_RGBA32F)

        # Get camera, HOW?

        # Start rendering

        client_message = ClientMessage()
        client_message.type = ClientMessage.START_RENDERING
        client_message.string_value = "interactive"
        self.render_samples = client_message.uint_value = scene.ospray.samples
        send_protobuf(self.sock, client_message)

        # Check for incoming render results

        if False:

            # Bind shader that converts from scene linear to display space,
            bgl.glEnable(bgl.GL_BLEND)
            bgl.glBlendFunc(bgl.GL_ONE, bgl.GL_ONE_MINUS_SRC_ALPHA);
            self.bind_display_space_shader(scene)

            if not self.draw_data or self.draw_data.dimensions != dimensions:
                self.draw_data = CustomDrawData(dimensions)

            self.draw_data.draw()

            self.unbind_display_space_shader()
            bgl.glDisable(bgl.GL_BLEND)
        
    # Nodes
    
    def NO_update_script_node(self, node):
        """Compile shader script node"""
        print('>>> OsprayRenderEngine.update_script_node()')
                


# From https://docs.blender.org/api/current/bpy.types.RenderEngine.html
class CustomDrawData:
    def __init__(self, dimensions):
        print('CustomDrawData.__init__()')
        
        # Generate dummy float image buffer
        self.dimensions = dimensions
        width, height = dimensions
        
        pixels = numpy.full(width*height*4, 0.3, dtype=numpy.float32)
        #pixels = [0.1, 0.2, 0.1, 1.0] * width * height
        pixels = bgl.Buffer(bgl.GL_FLOAT, width * height * 4, pixels)

        # Generate texture
        self.texture = bgl.Buffer(bgl.GL_INT, 1)
        bgl.glGenTextures(1, self.texture)
        bgl.glActiveTexture(bgl.GL_TEXTURE0)
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, self.texture[0])
        bgl.glTexImage2D(bgl.GL_TEXTURE_2D, 0, bgl.GL_RGBA16F, width, height, 0, bgl.GL_RGBA, bgl.GL_FLOAT, pixels)
        bgl.glTexParameteri(bgl.GL_TEXTURE_2D, bgl.GL_TEXTURE_MIN_FILTER, bgl.GL_LINEAR)
        bgl.glTexParameteri(bgl.GL_TEXTURE_2D, bgl.GL_TEXTURE_MAG_FILTER, bgl.GL_LINEAR)
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, 0)

        # Bind shader that converts from scene linear to display space,
        # use the scene's color management settings.
        shader_program = bgl.Buffer(bgl.GL_INT, 1)
        bgl.glGetIntegerv(bgl.GL_CURRENT_PROGRAM, shader_program);

        # Generate vertex array
        self.vertex_array = bgl.Buffer(bgl.GL_INT, 1)
        bgl.glGenVertexArrays(1, self.vertex_array)
        bgl.glBindVertexArray(self.vertex_array[0])

        texturecoord_location = bgl.glGetAttribLocation(shader_program[0], "texCoord");
        position_location = bgl.glGetAttribLocation(shader_program[0], "pos");

        bgl.glEnableVertexAttribArray(texturecoord_location);
        bgl.glEnableVertexAttribArray(position_location);

        # Generate geometry buffers for drawing textured quad
        position = [0.0, 0.0, width, 0.0, width, height, 0.0, height]
        position = bgl.Buffer(bgl.GL_FLOAT, len(position), position)
        texcoord = [0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0]
        texcoord = bgl.Buffer(bgl.GL_FLOAT, len(texcoord), texcoord)

        self.vertex_buffer = bgl.Buffer(bgl.GL_INT, 2)

        bgl.glGenBuffers(2, self.vertex_buffer)
        bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, self.vertex_buffer[0])
        bgl.glBufferData(bgl.GL_ARRAY_BUFFER, 32, position, bgl.GL_STATIC_DRAW)
        bgl.glVertexAttribPointer(position_location, 2, bgl.GL_FLOAT, bgl.GL_FALSE, 0, None)

        bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, self.vertex_buffer[1])
        bgl.glBufferData(bgl.GL_ARRAY_BUFFER, 32, texcoord, bgl.GL_STATIC_DRAW)
        bgl.glVertexAttribPointer(texturecoord_location, 2, bgl.GL_FLOAT, bgl.GL_FALSE, 0, None)

        bgl.glBindBuffer(bgl.GL_ARRAY_BUFFER, 0)
        bgl.glBindVertexArray(0)

    def __del__(self):
        print('CustomDrawData.__del__()')
        
        bgl.glDeleteBuffers(2, self.vertex_buffer)
        bgl.glDeleteVertexArrays(1, self.vertex_array)
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, 0)
        bgl.glDeleteTextures(1, self.texture)

    def draw(self):
        print('CustomDrawData.draw()')
        
        bgl.glActiveTexture(bgl.GL_TEXTURE0)
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, self.texture[0])
        bgl.glBindVertexArray(self.vertex_array[0])
        bgl.glDrawArrays(bgl.GL_TRIANGLE_FAN, 0, 4);
        bgl.glBindVertexArray(0)
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, 0)





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
    