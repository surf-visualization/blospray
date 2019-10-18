# Sources based on https://docs.blender.org/api/blender2.8/bpy.types.RenderEngine.html
# blender -P blender_render_engine.trace.py  -p 900 1000 3000 1500 
import bpy
import bgl
from mathutils import Vector

import math, time, queue
import numpy
from PIL import Image, ImageFont, ImageDraw

t0 = time.time()


class CustomRenderEngine(bpy.types.RenderEngine):
    # These three members are used by blender to set up the
    # RenderEngine; define its internal name, visible name and capabilities.
    bl_idname = "VIEWPORT"
    bl_label = "Viewport"
    bl_use_preview = True
    bl_use_eevee_viewport = True
    bl_use_shading_nodes_custom = False     # Default: True
    
    MAIN_LOG_SIZE = 25

    # Init is called whenever a new render engine instance is created. Multiple
    # instances may exist at the same time, for example for a viewport and final
    # render.
    def __init__(self):
        print('-' * 40)
        print('RENDER ENGINE __init__()')
        print('-' * 40)
        print()
        
        self.scene_data = None
        self.draw_data = None
        
        self.render_count = 0
        self.draw_count = 0
        self.update_count = 0
        self.dimensions = None
        
        self.log_lines_main = queue.Queue()
        self.log_lines_viewupdate = []
        self.log_lines_viewdraw = []
        
        self.font = ImageFont.truetype('/usr/share/fonts/DejaVuSansMono-Bold.ttf', 14)
        

    # When the render engine instance is destroyed, this is called. Clean up any
    # render engine data here, for example stopping running render threads.
    def __del__(self):
        print('-' * 40)
        print('RENDER ENGINE __del__()')
        print('-' * 40)
        print()
        
    def clear_log_viewupdate(self):
        self.log_lines_viewupdate = []
        
    def clear_log_viewdraw(self):
        self.log_lines_viewdraw = []
        
    def log_main(self, *args):
        if len(args) > 0:
            s = ' ' .join(map(str, args))
            t = '%.6f' % (time.time() - t0)
            msg = '%s | %s' % (t, s)
        else:
            msg = ''
            
        if self.log_lines_main.qsize() == self.MAIN_LOG_SIZE:
            self.log_lines_main.get_nowait()
        self.log_lines_main.put(msg)            
        
    def log_viewupdate(self, *args):
        if len(args) > 0:
            s = ' ' .join(map(str, args))
            t = '%.6f' % (time.time() - t0)
            msg = '%s | %s' % (t, s)
        else:
            msg = ''
        self.log_lines_viewupdate.append(msg)            
        
    def log_viewdraw(self, *args):
        if len(args) > 0:
            s = ' ' .join(map(str, args))
            t = '%.6f' % (time.time() - t0)
            msg = '%s | %s' % (t, s)
        else:
            msg = ''
        self.log_lines_viewdraw.append(msg)

    def log_image(self, width, height):
        img = Image.new('RGBA', (width,height), (255,255,255,255))
        draw = ImageDraw.Draw(img)
        
        col = (255, 0, 0)
                
        Y = 200
        L = 4
        
        x = 300
        y = Y
        
        # XXX yuck, but can iterate over a queue
        t = queue.Queue()
        while True:
            try:
                line = self.log_lines_main.get_nowait()
                w, h = draw.textsize(line, font=self.font)
                draw.text((x, y), line, fill=col, font=self.font)
                y += h + L                                   
                t.put(line)
            except queue.Empty:
                break
            
        self.log_lines_main = t
        
        x += 300
        y = Y
        for line in self.log_lines_viewupdate:
            w, h = draw.textsize(line, font=self.font)
            draw.text((x, y), line, fill=col, font=self.font)
            y += h + L           

        x += 700
        y = Y
        for line in self.log_lines_viewdraw:
            w, h = draw.textsize(line, font=self.font)
            draw.text((x, y), line, fill=col, font=self.font)
            y += h + L           
        
        del draw
        
        return img

    # For viewport renders, this method gets called once at the start and
    # whenever the scene or 3D viewport changes. This method is where data
    # should be read from Blender in the same thread. Typically a render
    # thread will be started to do the work while keeping Blender responsive.
    def view_update(self, context, depsgraph):
        
        self.clear_log_viewupdate()
        
        self.update_count += 1
        
        self.log_main('view_update #%d' % self.update_count)
        
        self.log_viewupdate('-' * 40)
        self.log_viewupdate('view_update #%d' % self.update_count)
        self.log_viewupdate('-' * 40)
        
        # The first call to view_update will not list any object updates,
        # so the full scene has to be synced using depsgraph.object_instances
        
        self.log_viewupdate('object_instances: %d objects' % len(depsgraph.object_instances))
        
        region = context.region
        view3d = context.space_data
        scene = depsgraph.scene
        
        # Get viewport dimensions
        dimensions = region.width, region.height
                
        types = ['ACTION', 'ARMATURE', 'BRUSH', 'CAMERA', 'CACHEFILE', 'CURVE', 'FONT', 'GREASEPENCIL', 'COLLECTION', 'IMAGE', 'KEY', 'LIGHT', 'LIBRARY', 'LINESTYLE', 'LATTICE', 'MASK', 'MATERIAL', 'META', 'MESH', 'MOVIECLIP', 'NODETREE', 'OBJECT', 'PAINTCURVE', 'PALETTE', 'PARTICLE', 'LIGHT_PROBE', 'SCENE', 'SOUND', 'SPEAKER', 'TEXT', 'TEXTURE', 'WINDOWMANAGER', 'WORLD', 'WORKSPACE']
        for t in types:
            if depsgraph.id_type_updated(t):
                self.log_viewupdate("Type %s updated" % t)

        self.log_viewupdate()
        
        for update in depsgraph.updates:
            self.log_viewupdate('Datablock "%s" updated (%s)' % (update.id.name, type(update.id)))
            if update.is_updated_geometry:
                self.log_viewupdate('-- geometry was updated')
            if update.is_updated_transform:
                self.log_viewupdate('-- transform was updated')
                
        self.log_viewupdate()

    # For viewport renders, this method is called whenever Blender redraws
    # the 3D viewport. The renderer is expected to quickly draw the render
    # with OpenGL, and not perform other expensive work.
    # Blender will draw overlays for selection and editing on top of the
    # rendered image automatically.
    def view_draw(self, context, depsgraph):
        
        # See https://github.com/LuxCoreRender/BlendLuxCore/blob/master/draw/viewport.py
        
        self.clear_log_viewdraw()

        self.draw_count += 1
        
        self.log_main('view_draw #%d' % self.draw_count)
        
        self.log_viewdraw('-' * 40)
        self.log_viewdraw('view_draw #%d' % self.draw_count)
        self.log_viewdraw('-' * 40)
        
        region = context.region
        assert region.type == 'WINDOW'        
        assert context.space_data.type == 'VIEW_3D'
        
        region_data = context.region_data
        space_data = context.space_data
        
        scene = context.scene
        
        self.log_viewdraw('region_data:')
        
        # view clipping has no effect on these matrices
        self.log_viewdraw('window_matrix')
        for line in str(region_data.window_matrix).split('\n'):
            self.log_viewdraw(line)
            
        self.log_viewdraw('perspective_matrix')
        for line in str(region_data.perspective_matrix).split('\n'):    # = window * view
            self.log_viewdraw(line)
            
        self.log_viewdraw('view_matrix')
        for line in str(region_data.view_matrix).split('\n'):
            self.log_viewdraw(line)
        
        # view_matrix = identity when in top view.
        # view_matrix = model-view matrix,  i.e. world to camera
        
        self.log_viewdraw('... view_perspective', region_data.view_perspective) # PERSP (regular 3D view not tied to camera), CAMERA (view from camera) or ORTHO (one of top, etc)
        self.log_viewdraw('... view_location', region_data.view_location)       # View pivot point
        self.log_viewdraw('... view_distance', region_data.view_distance)       # Distance to view location
        self.log_viewdraw('... view_rotation', region_data.view_rotation)       # Quat
        self.log_viewdraw('... view_camera_offset', list(region_data.view_camera_offset))
        self.log_viewdraw('... view_camera_zoom', region_data.view_camera_zoom)
        self.log_viewdraw('... is_perspective %d' % region_data.is_perspective)
        
        cam_xform = region_data.view_matrix.inverted()
        location = cam_xform.translation
        position = list(location)
        view_dir = list(cam_xform @ Vector((0, 0, -1)) - location)
        up_dir = list(cam_xform @ Vector((0, 1, 0)) - location)
        self.log_viewdraw('derived camera:')
        self.log_viewdraw('... position', position)
        self.log_viewdraw('... view_dir', view_dir)
        self.log_viewdraw('... up_dir', up_dir)

        # view3d.perspective_matrix = window_matrix * view_matrix
        perspective_matrix = region_data.perspective_matrix
        
        self.log_viewdraw('space_data:')
        self.log_viewdraw('... camera', space_data.camera)
        camobj = space_data.camera
        camdata = camobj.data
        self.log_viewdraw('... shift', camdata.shift_x, camdata.shift_y)
        self.log_viewdraw('... use_local_camera', space_data.use_local_camera)  # No relation to camera view yes/no
        self.log_viewdraw('... lens', space_data.lens)                          # Always set to viewport lens setting, even when in camera view
        self.log_viewdraw('... clip_start', space_data.clip_start)
                      
        scene = depsgraph.scene

        # Get viewport dimensions
        dimensions = region.width, region.height
        
        if dimensions != self.dimensions:
            self.log_viewdraw('Dimensions changed to %d x %d' % dimensions)
            self.dimensions = dimensions        
            
        self.log_viewdraw('Aspect (dims) %.3f' % (self.dimensions[0]/self.dimensions[1]))
        self.log_viewdraw('Aspect (window matrix) %.3f' % (region_data.window_matrix[1][1] / region_data.window_matrix[0][0]))
        
        # Turn log into image :)
        img = self.log_image(region.width, region.height)
            
        # Bind shader that converts from scene linear to display space,
        bgl.glEnable(bgl.GL_BLEND)
        bgl.glBlendFunc(bgl.GL_ONE, bgl.GL_ONE_MINUS_SRC_ALPHA);
        self.bind_display_space_shader(scene)

        if not self.draw_data or self.draw_data.dimensions != dimensions:
            self.draw_data = CustomDrawData(img)            
        else:
            self.draw_data.update_image(img)

        self.draw_data.draw()

        self.unbind_display_space_shader()
        bgl.glDisable(bgl.GL_BLEND)
                
        


class CustomDrawData:
    def __init__(self, img):
        print('CustomDrawData.__init__()')
        
        self.img = img
                
        width, height = self.dimensions = self.img.size
        
        imgbytes = img.transpose(Image.FLIP_TOP_BOTTOM).tobytes()                
        nimg = numpy.frombuffer(imgbytes, dtype=numpy.uint8)        
        nimg = nimg.astype(numpy.float32) / 255.0        
        
        pixels = bgl.Buffer(bgl.GL_FLOAT, width * height * 4, nimg)

        # Generate texture
        self.texture = bgl.Buffer(bgl.GL_INT, 1)
        bgl.glGenTextures(1, self.texture)
        bgl.glActiveTexture(bgl.GL_TEXTURE0)
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, self.texture[0])
        bgl.glTexImage2D(bgl.GL_TEXTURE_2D, 0, bgl.GL_RGBA16F, width, height, 0, bgl.GL_RGBA, bgl.GL_FLOAT, pixels)
        bgl.glTexParameteri(bgl.GL_TEXTURE_2D, bgl.GL_TEXTURE_MIN_FILTER, bgl.GL_NEAREST)
        bgl.glTexParameteri(bgl.GL_TEXTURE_2D, bgl.GL_TEXTURE_MAG_FILTER, bgl.GL_NEAREST)
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

    def update_image(self, img):
        print('CustomDrawData.update_image()')
        assert img.size == self.dimensions
        
        image_width, image_height = self.dimensions
        imgbytes = img.transpose(Image.FLIP_TOP_BOTTOM).tobytes()                
        nimg = numpy.frombuffer(imgbytes, dtype=numpy.uint8)        
        nimg = nimg.astype(numpy.float32) / 255.0        
        
        pixels = bgl.Buffer(bgl.GL_FLOAT, image_width * image_height * 4, nimg)        
                
        bgl.glActiveTexture(bgl.GL_TEXTURE0)        
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, self.texture[0])
        # XXX glTexSubImage2D
        bgl.glTexImage2D(bgl.GL_TEXTURE_2D, 0, bgl.GL_RGBA16F, image_width, image_height, 0, bgl.GL_RGBA, bgl.GL_FLOAT, pixels)
        bgl.glTexParameteri(bgl.GL_TEXTURE_2D, bgl.GL_TEXTURE_MIN_FILTER, bgl.GL_LINEAR)
        bgl.glTexParameteri(bgl.GL_TEXTURE_2D, bgl.GL_TEXTURE_MAG_FILTER, bgl.GL_LINEAR)        
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, 0)


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


# RenderEngines also need to tell UI Panels that they are compatible with.
# We recommend to enable all panels marked as BLENDER_RENDER, and then
# exclude any panels that are replaced by custom panels registered by the
# render engine, or that are not supported.
def get_panels():
    exclude_panels = {
        'VIEWLAYER_PT_filter',
        'VIEWLAYER_PT_layer_passes',
        'WORLD_PT_context_world',
    }

    panels = []
    for panel in bpy.types.Panel.__subclasses__():
        if hasattr(panel, 'COMPAT_ENGINES') and 'BLENDER_RENDER' in panel.COMPAT_ENGINES:
            if panel.__name__ not in exclude_panels:
                panels.append(panel)

    return panels

def register():
    # Register the RenderEngine
    bpy.utils.register_class(CustomRenderEngine)

    for panel in get_panels():
        panel.COMPAT_ENGINES.add('VIEWPORT')

def unregister():
    bpy.utils.unregister_class(CustomRenderEngine)

    for panel in get_panels():
        if 'VIEWPORT' in panel.COMPAT_ENGINES:
            panel.COMPAT_ENGINES.remove('VIEWPORT')


if __name__ == "__main__":
    register()
