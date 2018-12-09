# Can test with 
# $ blender -P <thisscript.py> -E OSPRAY

# - Need to check which collections to render ourselves
# - Make sockets non-blocking and use select() to handle errors on the server side

import bpy
#from bgl import *
from mathutils import Vector

import sys, array, json, os, socket, time
from math import tan, atan, degrees, radians
from struct import pack, unpack

import numpy

sys.path.insert(0, os.path.split(__file__)[0])

from messages_pb2 import (
    CameraSettings, ImageSettings, 
    LightSettings, Light,
    RenderSettings, SceneElement, MeshInfo, 
    VolumeInfo, VolumeLoadResult,
    ClientMessage, RenderResult
)

HOST = 'localhost'
PORT = 5909

# Object to world matrix
#
# Matrix(((0.013929054141044617, 0.0, 0.0, -0.8794544339179993),
#         (0.0, 0.013929054141044617, 0.0, -0.8227154612541199),
#         (0.0, 0.0, 0.013929054141044617, 0.0),
#         (0.0, 0.0, 0.0, 1.0)))
#
# Translation part is in right-most column

def matrix2list(m):
    """Convert to list of 16 floats"""
    values = []
    for row in m:
        values.extend(list(row))
    return values
    
def customproperties2dict(obj):
    user_keys = [k for k in obj.keys() if k[0] != '_']
    properties = {}
    for k in user_keys:
        v = obj[k]
        if hasattr(v, 'to_dict'):
            properties[k] = v.to_dict()
        elif hasattr(v, 'to_list'):
            properties[k] = v.to_list()
        else:
            # XXX assumes simple type that can be serialized to json
            properties[k] = v
            
    if 'file' in properties:
        # XXX might not always be called 'file'
        # //... -> full path
        properties['file'] = bpy.path.abspath(properties['file'])
        
    return properties
    
def send_protobuf(sock, pb, sendall=False):
    """Serialize a protobuf object and send it on the socket"""
    s = pb.SerializeToString()
    sock.send(pack('<I', len(s)))
    if sendall:
        sock.sendall(s) 
    else:
        sock.send(s)
        
def receive_protobuf(sock, protobuf):
    d = sock.recv(4)
    bufsize = unpack('<I', d)[0]
    
    parts = []
    bytes_left = bufsize
    while bytes_left > 0:
        d = sock.recv(bytes_left)
        parts.append(d)
        bytes_left -= len(d)

    message = b''.join(parts)
    
    protobuf.ParseFromString(message)

class OsprayRenderEngine(bpy.types.RenderEngine):
    # These three members are used by blender to set up the
    # RenderEngine; define its internal name, visible name and capabilities.
    bl_idname = "OSPRAY"
    bl_label = "OSPRay"
    
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
        
        self.update_stats('', 'Exporting')
        self.export_scene(data, depsgraph)
        
    def _read_framebuffer(self, framebuffer, width, height):
        
        # XXX use select() in a loop, to handle UI updates better
        
        num_pixels = width * height
        bytes_left = num_pixels * 4*4
        
        #self.update_stats('%d bytes left' % bytes_left, 'Reading back framebuffer')        
        
        view = memoryview(framebuffer)

        while bytes_left > 0:
            n = self.sock.recv_into(view, bytes_left)
            view = view[n:]
            bytes_left -= n
            sys.stdout.write('.')
            sys.stdout.flush()
            #self.update_stats('%d bytes left' % bytes_left, 'Reading back framebuffer')
        
    def _read_framebuffer_to_file(self, fname, size):
        
        with open(fname, 'wb') as f:
            bytes_left = size
            while bytes_left > 0:
                d = self.sock.recv(bytes_left)
                # XXX check d
                f.write(d)
                bytes_left -= len(d)
        
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
        if self.is_preview:
            self.render_preview(depsgraph)
        else:
            self.render_scene(depsgraph)
        """
        
        # Rendering already started when we sent the scene data in update()
        # Read back framebuffer (might block)  

        num_pixels = self.width * self.height    
        bytes_left = num_pixels * 4*4

        framebuffer = numpy.zeros(bytes_left, dtype=numpy.uint8)
        
        t0 = time.time()
        
        result = self.begin_result(0, 0, self.width, self.height)
        # Only Combined and Depth seem to be available
        layer = result.layers[0].passes["Combined"] 
        
        FBFILE = '/dev/shm/blosprayfb.exr'
        
        sample = 1
        
        while True:
            
            self.update_stats('', 'Rendering sample %d/%d' % (sample, self.render_samples))
            
            render_result = RenderResult()
            receive_protobuf(self.sock, render_result)
            
            if render_result.type == RenderResult.FRAME:
                
                """
                # XXX Slow: get as raw block of floats
                print('[%6.3f] _read_framebuffer start' % (time.time()-t0))
                self._read_framebuffer(framebuffer, self.width, self.height)            
                print('[%6.3f] _read_framebuffer end' % (time.time()-t0))
                
                pixels = framebuffer.view(numpy.float32).reshape((num_pixels, 4))
                print(pixels.shape)
                print('[%6.3f] view() end' % (time.time()-t0))
                
                # Here we write the pixel values to the RenderResult   
                # XXX This is the slow part
                print(type(layer.rect))
                layer.rect = pixels
                self.update_result(result)
                """
                
                print('[%6.3f] _read_framebuffer_to_file start' % (time.time()-t0))
                self._read_framebuffer_to_file(FBFILE, render_result.file_size)
                print('[%6.3f] _read_framebuffer_to_file end' % (time.time()-t0))
                
                # Sigh, this needs an image file format. I.e. reading in a raw framebuffer
                # of floats isn't possible
                result.layers[0].load_from_file(FBFILE)
                self.update_result(result)
                
                print('[%6.3f] update_result() done' % (time.time()-t0))

                self.update_progress(sample/self.render_samples)
                
                sample += 1
                
            elif render_result.type == RenderResult.DONE:
                print('Rendering done!')
                break
            
        self.end_result(result)      
        
        self.sock.close()

        
    #
    # Scene export
    #
    
    def export_volume(self, obj, data, depsgraph):
    
        element = SceneElement()
        element.type = SceneElement.VOLUME
        element.name = obj.name
        
        send_protobuf(self.sock, element)        
        
        volume = VolumeInfo()
        volume.object2world[:] = matrix2list(obj.matrix_world)
        properties = customproperties2dict(obj)
        volume.properties = json.dumps(properties)
        
        print('Sending properties:')
        print(properties)
        
        send_protobuf(self.sock, volume)
        
        # Wait for volume to be loaded on the server, signaled
        # by the return of a result value
        
        volume_load_result = VolumeLoadResult()
        
        receive_protobuf(self.sock, volume_load_result)
        
        if not volume_load_result.success:
            print('ERROR: volume loading failed:')
            print(volume_load_result.message)
            return
            
        id = volume_load_result.hash
        print(id)
        
        obj['loaded_id'] = id.decode('utf8')
        print('Exported volume received id %s' % id)
        
        # Get volume bbox 
        
        bbox = list(volume_load_result.bbox)
        print('Bbox', bbox)
        
        # Update mesh to match bbox
        
        verts = [
            Vector((bbox[0], bbox[1], bbox[2])),
            Vector((bbox[3], bbox[1], bbox[2])),
            Vector((bbox[3], bbox[4], bbox[2])),
            Vector((bbox[0], bbox[4], bbox[2])),
            Vector((bbox[0], bbox[1], bbox[5])),
            Vector((bbox[3], bbox[1], bbox[5])),
            Vector((bbox[3], bbox[4], bbox[5])),
            Vector((bbox[0], bbox[4], bbox[5]))
        ]        
        
        edges = [
            (0, 1), (1, 2), (2, 3), (3, 0),
            (4, 5), (5, 6), (6, 7), (7, 4),
            (0, 4), (1, 5), (2, 6), (3, 7)
        ]
        
        faces = []
        
        mesh = bpy.data.meshes.new(name="Volume extent")
        mesh.from_pydata(verts, edges, faces)
        mesh.validate(verbose=True)
        obj.data = mesh
        

    def export_scene(self, data, depsgraph):
        
        scene = depsgraph.scene
        world = scene.world
        
        # Image

        perc = scene.render.resolution_percentage
        perc = perc / 100

        # XXX should pass full resolution in image_settings below
        self.width = int(scene.render.resolution_x * perc)
        self.height = int(scene.render.resolution_y * perc)
        
        aspect = self.width / self.height
        
        image_settings = ImageSettings()
        image_settings.width = self.width
        image_settings.height = self.height
        image_settings.percentage = scene.render.resolution_percentage

        # Camera
        
        cam_obj = scene.camera        
        cam_xform = cam_obj.matrix_world
        cam_data = cam_obj.data

        camera_settings = CameraSettings()
        camera_settings.position[:] = list(cam_obj.location)
        camera_settings.view_dir[:] = list(cam_xform @ Vector((0, 0, -1)) - cam_obj.location)
        camera_settings.up_dir[:] = list(cam_xform @ Vector((0, 1, 0)) - cam_obj.location)

        # Get camera FOV (in radians)
        hfov = cam_data.angle   
        image_plane_width = 2 * tan(hfov/2)
        image_plane_height = image_plane_width / aspect
        vfov = 2*atan(image_plane_height/2)
        camera_settings.fov_y = degrees(vfov)
        
        # DoF
        if cam_data.dof_object is not None:
            camera_settings.dof_focus_distance = (cam_data.dof_object.location - cam_obj.location).length
        else:
            camera_settings.dof_focus_distance = cam_data.dof_distance
            
        camera_settings.dof_aperture = 0.0
        if 'aperture' in cam_data:
            camera_settings.dof_aperture = cam_data['aperture']
            
        # Render settings
        # XXX For now, use some properties of the world
        
        render_settings = RenderSettings()
        render_settings.renderer = world['renderer'] if 'renderer' in world else 'scivis'
        # XXX this is a hack, as it doesn't specify the alpha value, only rgb
        # World -> Viewport Display -> Color
        render_settings.background_color[:] = world.color
        self.render_samples = render_settings.samples = world['samples'] if 'samples' in world else 4
        render_settings.ao_samples = world['ao_samples'] if 'ao_samples' in world else 1
        render_settings.shadows_enabled = world['shadows_enabled'] if 'shadows_enabled' in world else False
        
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
        
        light_settings = LightSettings()
        
        light_settings.ambient_color[:] = world['ambient_color'] if 'ambient_color' in world else (1,1,1)
        light_settings.ambient_intensity = world['ambient_intensity'] if 'ambient_intensity' in world else 1
        
        type2enum = dict(POINT=Light.POINT, SUN=Light.SUN, SPOT=Light.SPOT, AREA=Light.AREA)
        
        lights = []
        for obj in scene.objects:
            
            if obj.type != 'LIGHT':
                continue
                
            data = obj.data
            xform = obj.matrix_world
            properties = customproperties2dict(data)
            
            light = Light()
            light.type = type2enum[data.type]
            light.object2world[:] = matrix2list(xform)
            
            # XXX
            light.color[:] = properties['color'] if 'color' in properties else (1.0, 1.0, 1.0)            
            light.intensity = properties['intensity'] if 'intensity' in properties else 1.0   
            light.visible = True
            
            if data.type != 'SUN':
                light.position[:] = (xform[0][3], xform[1][3], xform[2][3])
                
            if data.type in ['SUN', 'SPOT']:
                light.direction[:] = obj.matrix_world @ Vector((0, 0, -1)) - obj.location
                
            if data.type == 'SPOT':
                # Blender:
                # .spot_size = full angle where light shines, in degrees
                # .spot_blend = factor in [0,1], 0 = no penumbra, 1 = penumbra is full angle
                light.opening_angle = degrees(data.spot_size)
                light.penumbra_angle = 0.5*data.spot_blend*degrees(data.spot_size)
                # assert light.penumbra_angle < 0.5*light.opening_angle                
                # XXX
                light.radius = 0.0
                
            if data.type == 'POINT':
                # XXX
                light.radius = 0.0
            
            lights.append(light)
                
        # XXX assigning to lights[:] doesn't work
        light_settings.lights.extend(lights)

        #
        # Send scene
        #
        
        # Image settings
        send_protobuf(self.sock, image_settings)
        
        # Render settings        
        send_protobuf(self.sock, render_settings)
        
        # Camera settings
        send_protobuf(self.sock, camera_settings)
            
        # Light settings
        send_protobuf(self.sock, light_settings)
        
        # Objects (meshes and volumes)
         
        for obj in scene.objects:
            
            if obj.type != 'MESH':
                continue
                
            if obj.hide_render:
                # XXX This doesn't seem to work for hiding the collection in which
                # an object is located.
                # See https://developer.blender.org/T58823 for more info
                continue
                
            if 'voltype' in obj:
                self.export_volume(obj, data, depsgraph)
                continue
                
            # Object with mesh data
                
            element = SceneElement()
            element.type = SceneElement.MESH
            element.name = obj.name
            
            send_protobuf(self.sock, element)      

            mesh = obj.data            
            
            mesh_info = MeshInfo()            
            mesh_info.object2world[:] = matrix2list(obj.matrix_world)
            mesh_info.properties = json.dumps(customproperties2dict(mesh))
            
            flags = 0
        
            # Turn geometry into triangles
            # XXX handle modifiers
                        
            mesh.calc_loop_triangles()
            
            nv = mesh_info.num_vertices = len(mesh.vertices)
            nt = mesh_info.num_triangles = len(mesh.loop_triangles)
            
            print('Object %s - Mesh %s: %d v, %d t' % (obj.name, mesh.name, nv, nt))        

            # Check if any faces use smooth shading
            # XXX we currently don't handle meshes with both smooth
            # and non-smooth faces, but those are not very common anyway
            
            use_smooth = False
            for tri in mesh.loop_triangles:
                if tri.use_smooth:
                    use_smooth = True
                    flags |= MeshInfo.NORMALS
                    break
                    
            if use_smooth:
                print('Mesh uses smooth shading')

            # Send mesh info
            
            mesh_info.flags = flags
            
            send_protobuf(self.sock, mesh_info)
            
            # Send vertices
            
            vertices = numpy.empty(nv*3, dtype=numpy.float32)

            for idx, v in enumerate(mesh.vertices):
                p = v.co
                vertices[3*idx+0] = p.x
                vertices[3*idx+1] = p.y
                vertices[3*idx+2] = p.z
                
            self.sock.send(vertices.tobytes())
                
            if use_smooth:
                normals = numpy.empty(nv*3, dtype=numpy.float32)
                
                for idx, v in enumerate(mesh.vertices):
                    n = v.normal
                    normals[3*idx+0] = n.x
                    normals[3*idx+1] = n.y
                    normals[3*idx+2] = n.z
                    
                self.sock.send(normals.tobytes())
                
            # Send triangles
            
            triangles = numpy.empty(nt*3, dtype=numpy.uint32)   # XXX opt possible when less than 64k vertices ;-)
            
            for idx, tri in enumerate(mesh.loop_triangles):
                triangles[3*idx+0] = tri.vertices[0]
                triangles[3*idx+1] = tri.vertices[1]
                triangles[3*idx+2] = tri.vertices[2]
                
            self.sock.send(triangles.tobytes())
                        
        # Signal last data was sent!
        
        element = SceneElement()
        element.type = SceneElement.NONE
        send_protobuf(self.sock, element)
            
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
    bpy.utils.register_class(OsprayRenderEngine)

    # RenderEngines need to tell UI Panels that they are compatible with them.
    # Otherwise most of the UI will be empty when the engine is selected.
    
    for module, panels in enabled_panels.items():
        for panelname in panels:
            panel = getattr(module, panelname)
            if hasattr(panel, 'COMPAT_ENGINES'):
                panel.COMPAT_ENGINES.add(OsprayRenderEngine.bl_idname)
                
def unregister():
    bpy.utils.unregister_class(OsprayRenderEngine)
    
    for module, panels in enabled_panels.items():
        for panelname in panels:
            panel = getattr(module, panelname)
            if hasattr(panel, 'COMPAT_ENGINES'):
                panel.COMPAT_ENGINES.remove(OsprayRenderEngine.bl_idname)
    

if __name__ == "__main__":
    register()