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
    
import sys, logging, socket, threading, time, traceback, weakref
from math import tan, atan, degrees
from queue import Queue
from select import select
from struct import unpack

import bpy, bgl
import numpy

from .common import send_protobuf, receive_protobuf, OSP_FB_RGBA32F
from .sync import BlenderCamera, sync_view
from .connection import Connection
from .messages_pb2 import (
    ClientMessage,
    RenderResult,
    WorldSettings, CameraSettings, LightSettings, RenderSettings,
)

# bpy.app.background

HOST = 'localhost'
PORT = 5909

def setup_logging(logger_name, logfile, console=True):

    # Format
    formatter = logging.Formatter('%(asctime)s - %(name)15s - %(levelname)-5s [%(thread)08x] %(message)s')    
    
    logger = logging.getLogger(logger_name)
    logger.propagate = False
    logger.setLevel(logging.DEBUG)

    # Log all to file, truncates existing file
    file_handler = logging.FileHandler(logfile, mode='w')
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(formatter)
    logger.addHandler(file_handler)

    # Log info and higher to console
    if console:
        console_handler = logging.StreamHandler()
        console_handler.setLevel(logging.DEBUG)
        console_handler.setFormatter(formatter)
        logger.addHandler(console_handler)

    logger.info('------- Logging started ----------')

    return logger

setup_logging('blospray', 'blospray.log')


class ReceiveRenderResultThread(threading.Thread):

    """
    Thread to handle receiving of interactive render results 
    (i.e. framebuffer)
    """

    def __init__(self, engine, connection, result_queue, log, num_samples, initial_reduction_factor):
        threading.Thread.__init__(self)
        self.engine_ref = weakref.ref(engine)
        self.connection = connection
        self.result_queue = result_queue
        self.log = log

        self.num_samples = num_samples
        self.initial_reduction_factor = initial_reduction_factor

        self._cancel = threading.Event()

    def cancel(self):
        """Cancel rendering, to be sent by outside thread"""
        self._cancel.set()

    def run(self):

        # Start rendering on the server
        self.log.debug('(RRR thread) Sending START_RENDERING to server')
        client_message = ClientMessage()
        client_message.type = ClientMessage.START_RENDERING
        client_message.string_value = "interactive"
        client_message.uint_value = self.num_samples        
        client_message.uint_value2 = self.initial_reduction_factor
        self.connection.send_protobuf(client_message)

        # Loop to get results until the render is either done or canceled

        sock = self.connection.sock         # XXX        
        rsocks = [sock]
        incoming_data = []

        framebuffer = None
        fbview = None

        # h = receive protobuf length header
        # r = receive RenderResult protobuf
        # f = receive framebuffer data
        mode = 'h'                          
        bytes_left = 4        

        self.log.debug('(RRR thread) Entering receive loop')
        while True:

            if self._cancel.is_set():                
                self.log.error('(RRR thread) Got request to cancel thread, sending CANCEL_RENDERING to server')
                client_message = ClientMessage()
                client_message.type = ClientMessage.CANCEL_RENDERING
                self.connection.send_protobuf(client_message)                    
                self._cancel.clear()
                break

            # Check for new incoming data
            r, w, e = select(rsocks, [], [], 0.001)

            if len(r) == 0:
                continue

            # There's new data available, read some

            if mode == 'f':
                #print('bufsize', len(fbview), 'bytes_left', bytes_left)
                n = sock.recv_into(fbview, bytes_left)
                if n == 0:
                    # XXX
                    self.log.error('(RRR thread) Connection reset by peer, exiting')
                    break
                bytes_left -= n
                assert bytes_left >= 0
                fbview = fbview[n:]

            else:
                d = sock.recv(bytes_left)
                if d == '':
                    # XXX
                    self.log.error('(RRR thread) Connection reset by peer, exiting')
                    break                
                bytes_left -= len(d)
                assert bytes_left >= 0

            # Next step?

            if mode == 'h': 

                assert bytes_left == 0              # Assume we got the 4 byte length header in one recv()
                bytes_left = unpack('<I', d)[0]
                assert bytes_left > 0
                mode = 'r'

            elif mode == 'r':

                incoming_data.append(d)

                if bytes_left > 0:
                    continue
                    
                message = b''.join(incoming_data)
                incoming_data = []

                render_result = RenderResult()
                render_result.ParseFromString(message)

                self.log.debug('(RRR thread): Render result of type %s' % render_result.type)
                self.log.debug('(RRR thread): %s' % render_result)

                if render_result.type == RenderResult.FRAME:                    
                    # XXX can keep buffer if res didn't change
                    print('allocating empty %d x %d' % (render_result.width, render_result.height))
                    
                    mode = 'f'                    
                    bytes_left = render_result.file_size

                    framebuffer = numpy.empty(bytes_left, dtype=numpy.uint8)
                    fbview = memoryview(framebuffer)
                    
                else:
                    # DONE, CANCELED
                    self.result_queue.put((render_result, None))

                    #mode = 'h'
                    #bytes_left = 4

                    # XXX why not break on DONE as well?
                    #if render_result.type == RenderResult.CANCELED:     
                    #    break         

                    break

            elif mode == 'f':

                if bytes_left > 0:
                    continue

                mode = 'h'
                bytes_left = 4

                # Got complete frame buffer, let engine know
                self.result_queue.put((render_result, framebuffer))   

                engine = self.engine_ref()
                if engine is not None:
                    try:                
                        self.log.debug('(RRR thread) Tagging for view_draw()')
                        engine.tag_redraw()
                    except ReferenceError:
                        #  StructRNA of type OsprayRenderEngine has been removed
                        break
                    engine = None
                else:
                    # Engine has gone away
                    break
                
        self.log.error('(RRR thread) Done')

    
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
        self.log = logging.getLogger('blospray')

        self.log.info('OsprayRenderEngine.__init__() [%s]' % self)
        super(OsprayRenderEngine, self).__init__()

        self.connection = None
        self.render_output_connection = None

        self.first_view_update = True
        self.rendering_active = False        
        self.receive_render_result_thread = None

        self.viewport_width = self.viewport_height = None
        
        self.last_view_matrix = None
        self.last_ortho_view_height = None
        self.last_view_camera_zoom = None
        self.last_view_camera_offset = None

        self.draw_data = None        
    
    # When the render engine instance is destroyed, this is called. Clean up any
    # render engine data here, for example stopping running render threads.    
    def __del__(self):
        print('OsprayRenderEngine.__del__()')
        logging.getLogger('blospray').info('[%s] OsprayRenderEngine.__del__() [%s]' % (time.asctime(), self))

        if hasattr(self, 'receive_render_result_thread') and self.receive_render_result_thread is not None:
            t0 = time.time()
            self.cancel_render_thread()
            t1 = time.time()
            print('************************************* %f' % (t1-t0))
            
        # XXX doesn't work, apparently self.connection is no longer available here?
        if hasattr(self, 'connection') and self.connection is not None:        
            self.connection.close()

    def connect(self, depsgraph):
        assert self.connection is None
        ospray = depsgraph.scene.ospray        
        self.connection = Connection(self, ospray.host, ospray.port)        
        return self.connection.connect()

    def connect_render_output(self, depsgraph):
        assert self.render_output_connection is None
        ospray = depsgraph.scene.ospray        
        self.render_output_connection = Connection(self, ospray.host, ospray.port)     
        assert self.render_output_connection.connect()
        self.render_output_connection.request_render_output()
        
    def update(self, data, depsgraph):
        """
        Export scene data for final or material preview render
        
        Note that this method is always called, even when re-rendering
        exactly the same scene or moving just the camera.
        """
        self.log.info('OsprayRenderEngine.update() [%s]' % self)        

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
            self.log.exception('Exception while updating scene on server')
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
        self.log.info('OsprayRenderEngine.render() [%s]' % self)

        if not self.update_succeeded:
            return
        
        try:
            self.connection.render(depsgraph)
            self.connection.close()        
            self.connection = None
        except:
            exc_type, exc_value, exc_traceback = sys.exc_info()            
            lines = traceback.format_exception(exc_type, exc_value, exc_traceback)
            self.log.exception('Exception while rendering scene on server')
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

    def start_render_thread(self):
        assert self.receive_render_result_thread is None
        self.log.debug('Starting render thread')
        self.render_result_queue = Queue()
        self.receive_render_result_thread = ReceiveRenderResultThread(self, self.connection, self.render_result_queue, self.log, 
            self.num_samples, self.initial_reduction_factor)
        self.receive_render_result_thread.start()

    def cancel_render_thread(self):
        assert self.receive_render_result_thread is not None
        self.log.debug('cancel_render_thread(): Waiting for render thread to cancel')
        t0= time.time()
        self.receive_render_result_thread.cancel()
        self.receive_render_result_thread.join()
        t1 = time.time()
        print('***************** CANCEL AND JOIN: %f' % (t1-t0))
        self.receive_render_result_thread = None
        self.log.debug('cancel_render_thread(): Render thread canceled')         
    
    # For viewport renders, this method gets called once at the start and
    # whenever the scene or 3D viewport changes. This method is where data
    # should be read from Blender in the same thread. Typically a render
    # thread will be started to do the work while keeping Blender responsive.   
    def view_update(self, context, depsgraph):
        """Update on data changes for viewport render"""
        self.log.info('OsprayRenderEngine.view_update() [%s]' % self)       

        scene = depsgraph.scene
        ospray = scene.ospray
        render = scene.render
        #world = scene.world
        region = context.region  

        restart_rendering = False   

        if self.first_view_update:        

            self.log.debug('view_update(): FIRST')

            assert self.receive_render_result_thread is None

            # Open connection
            if not self.connect(depsgraph):  
                self.log.info('ERROR(view_update): Failed to connect to BLOSPRAY server')                
                return

            # Renderer type
            self.connection.send_updated_renderer_type(scene.ospray.renderer)

            # Render settings
            render_settings = RenderSettings()
            render_settings.renderer = scene.ospray.renderer                        
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

            self.connection.send_updated_render_settings(render_settings)          

            # Send complete (visible) scene
            # XXX put exception handler around whole block above
            try:
                self.log.info('Sending initial scene')
                self.connection.update(None, depsgraph)
            except:
                exc_type, exc_value, exc_traceback = sys.exc_info()            
                lines = traceback.format_exception(exc_type, exc_value, exc_traceback)
                self.log.exception('Exception while updating scene on server')
                self.report({'ERROR'}, 'Exception sending initial scene to server: %s' % sys.exc_info()[0])
                return 

            self.first_view_update = False

            restart_rendering = True

        else:
            # Cancel render thread and wait for it to finish
            self.log.debug('view_update(): canceling render thread')

            if self.receive_render_result_thread is not None:
                self.log.debug('view_update(): canceling render thread')
                self.cancel_render_thread()

            #  Update scene on server
            self.log.debug('view_update(): SUBSEQUENT')
            self._print_depsgraph_updates(depsgraph)
            # XXX
            # restart_rendering = True

        if restart_rendering:
            self.log.info('view_update(): restarting rendering')
            assert self.receive_render_result_thread is None
            # Start thread to handle results
            self.num_samples = ospray.samples
            self.initial_reduction_factor = ospray.reduction_factor
            self.start_render_thread()

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
        - Resizing the 3D editor that's in interactive rendering mode
        """
        self.log.info('OsprayRenderEngine.view_draw() [%s]' % self)   

        restart_rendering = False
        update_camera = False 

        assert len(depsgraph.updates) == 0

        scene = depsgraph.scene
        ospray = scene.ospray
        region = context.region
        assert region.type == 'WINDOW'        
        assert context.space_data.type == 'VIEW_3D'
        
        region_data = context.region_data
        space_data = context.space_data        

        # Get viewport dimensions
        viewport_width, viewport_height = viewport_dimensions = region.width, region.height        
               
        if viewport_width != self.viewport_width or viewport_height != self.viewport_height:
            self.log.info('view_draw(): viewport size changed to %d x %d' % (viewport_width,viewport_height))        

            if self.receive_render_result_thread is not None:
                self.log.debug('view_draw(): canceling render thread')
                t0 = time.time()
                self.cancel_render_thread()
                t1 = time.time()
                print('************************************* %f' % (t1-t0))

            self.viewport_width = viewport_width
            self.viewport_height = viewport_height
            # Reduction factor is passed with START_RENDERING
            self.connection.send_updated_framebuffer_settings(viewport_width, viewport_height, OSP_FB_RGBA32F)        
            restart_rendering = True
            update_camera = True

        # Camera view    
        # XXX clipping and focal length change should trigger camera update

        view_matrix = region_data.view_matrix
        if update_camera or view_matrix != self.last_view_matrix or \
            (region_data.view_perspective == 'ORTHO' and region_data.view_distance != self.last_ortho_view_height) or \
            (region_data.view_perspective == 'CAMERA' and (region_data.view_camera_zoom != self.last_view_camera_zoom or list(region_data.view_camera_offset) != self.last_view_camera_offset)):
                
            self.log.info('view_draw(): view matrix changed, or camera updated')

            if self.receive_render_result_thread is not None:
                self.log.debug('view_draw(): canceling render thread')
                t0 = time.time()
                self.cancel_render_thread()
                t1 = time.time()
                print('************************************* %f' % (t1-t0))

            self.connection.send_updated_camera_for_interactive_view(scene.render, region_data, space_data, self.viewport_width, self.viewport_height)
            
            self.last_view_matrix = view_matrix.copy()
            self.last_ortho_view_height = region_data.view_distance
            self.last_view_camera_zoom = region_data.view_camera_zoom
            self.last_view_camera_offset = list(region_data.view_camera_offset)
            
            restart_rendering = True

        # Restart rendering if needed

        if restart_rendering:
            self.log.info('view_draw(): restarting rendering')
            self.num_samples = ospray.samples
            self.initial_reduction_factor = ospray.reduction_factor
            self.start_render_thread()

        # Bind shader that converts from scene linear to display space
        bgl.glEnable(bgl.GL_BLEND)
        bgl.glBlendFunc(bgl.GL_ONE, bgl.GL_ONE_MINUS_SRC_ALPHA);
        self.bind_display_space_shader(scene)  

        # Check for incoming render results

        while self.render_result_queue.qsize() > 0:

            render_result, framebuffer = self.render_result_queue.get()            

            if render_result.type == RenderResult.FRAME:
                self.log.info('FRAME')                

                rf = render_result.reduction_factor
                if rf > 1:
                    self.update_stats('', 'Rendering sample %d/%d (reduced %dx)' % (render_result.sample, self.num_samples, rf))
                else:
                    self.update_stats('', 'Rendering sample %d/%d' % (render_result.sample, self.num_samples))
                
                image_dimensions = render_result.width, render_result.height
                fbpixels = framebuffer.view(numpy.float32)

                if not self.draw_data or self.draw_data.image_dimensions != image_dimensions or self.draw_data.viewport_dimensions != viewport_dimensions:
                    self.log.info('Creating new CustomDrawData(viewport = %s, image = %s)' % (viewport_dimensions, image_dimensions))
                    self.draw_data = CustomDrawData(viewport_dimensions, image_dimensions, fbpixels)
                else:
                    self.log.info('Updating pixels of existing CustomDraw')
                    self.draw_data.update_pixels(fbpixels)

            elif render_result.type == RenderResult.DONE:
                self.log.info('DONE')
                self.rendering_active = False
                self.update_stats('', 'Rendering Done')

            elif render_result.type == RenderResult.CANCELED:
                self.log.info('CANCELED')
                # Thread will have exited by itself already
                self.receive_render_result_thread = None

        if self.draw_data is not None: 
            self.draw_data.draw()

        self.unbind_display_space_shader()
        bgl.glDisable(bgl.GL_BLEND)     
        
    # Nodes
    
    def NO_update_script_node(self, node):
        """Compile shader script node"""
        self.log.debug('OsprayRenderEngine.update_script_node() [%s]' % self)
                


# Based on https://docs.blender.org/api/current/bpy.types.RenderEngine.html
class CustomDrawData:

    def __init__(self, viewport_dimensions, image_dimensions, pixels):
        self.log = logging.getLogger('blospray')

        self.log.info('CustomDrawData.__init__(viewport_dimensions=%s, image_dimensions=%s, fbpixels=%s) [%s]' % \
            (viewport_dimensions, image_dimensions, pixels.shape, self))    
        
        viewport_width, viewport_height = self.viewport_dimensions = viewport_dimensions
        image_width, image_height = self.image_dimensions = image_dimensions
        
        assert pixels is not None
        pixels = bgl.Buffer(bgl.GL_FLOAT, image_width * image_height * 4, pixels)

        # Generate texture
        self.texture = bgl.Buffer(bgl.GL_INT, 1)
        bgl.glGenTextures(1, self.texture)

        bgl.glActiveTexture(bgl.GL_TEXTURE0)
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, self.texture[0])
        bgl.glTexImage2D(bgl.GL_TEXTURE_2D, 0, bgl.GL_RGBA16F, image_width, image_height, 0, bgl.GL_RGBA, bgl.GL_FLOAT, pixels)
        bgl.glTexParameteri(bgl.GL_TEXTURE_2D, bgl.GL_TEXTURE_MIN_FILTER, bgl.GL_NEAREST)
        bgl.glTexParameteri(bgl.GL_TEXTURE_2D, bgl.GL_TEXTURE_MAG_FILTER, bgl.GL_NEAREST)
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, 0)

        # Bind shader that converts from scene linear to display space,
        # use the scene's color management settings.
        shader_program = bgl.Buffer(bgl.GL_INT, 1)
        bgl.glGetIntegerv(bgl.GL_CURRENT_PROGRAM, shader_program)

        # Generate vertex array
        self.vertex_array = bgl.Buffer(bgl.GL_INT, 1)
        bgl.glGenVertexArrays(1, self.vertex_array)
        bgl.glBindVertexArray(self.vertex_array[0])

        texturecoord_location = bgl.glGetAttribLocation(shader_program[0], "texCoord")
        position_location = bgl.glGetAttribLocation(shader_program[0], "pos")

        bgl.glEnableVertexAttribArray(texturecoord_location)
        bgl.glEnableVertexAttribArray(position_location)

        # Generate geometry buffers for drawing textured quad
        position = [0.0, 0.0, viewport_width, 0.0, viewport_width, viewport_height, 0.0, viewport_height]
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
        print('[%s] CustomDrawData.__del__() [%s]' % (time.asctime(), self))        
        bgl.glDeleteBuffers(2, self.vertex_buffer)
        bgl.glDeleteVertexArrays(1, self.vertex_array)
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, 0)
        bgl.glDeleteTextures(1, self.texture)

    def update_pixels(self, pixels):
        self.log.info('CustomDrawData.update_pixels(%d x %d, %d) [%s]' % (self.image_dimensions[0], self.image_dimensions[1], pixels.shape[0], self))        
        image_width, image_height = self.image_dimensions
        assert pixels.shape[0] == image_width*image_height*4
        pixels = bgl.Buffer(bgl.GL_FLOAT, image_width * image_height * 4, pixels)
        bgl.glActiveTexture(bgl.GL_TEXTURE0)        
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, self.texture[0])
        # XXX glTexSubImage2D
        bgl.glTexImage2D(bgl.GL_TEXTURE_2D, 0, bgl.GL_RGBA16F, image_width, image_height, 0, bgl.GL_RGBA, bgl.GL_FLOAT, pixels)
        bgl.glTexParameteri(bgl.GL_TEXTURE_2D, bgl.GL_TEXTURE_MIN_FILTER, bgl.GL_LINEAR)
        bgl.glTexParameteri(bgl.GL_TEXTURE_2D, bgl.GL_TEXTURE_MAG_FILTER, bgl.GL_LINEAR)        
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, 0)

    def draw(self):
        self.log.info('CustomDrawData.draw() [%s]' % self)        
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
    