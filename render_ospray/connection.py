# - Use depsgraph fields to determine what to render
# - Make sockets non-blocking and use select() to handle errors on the server side

import bpy
#from bgl import *
from mathutils import Vector

import sys, array, json, os, socket, time
from math import tan, atan, degrees, radians
from struct import pack, unpack

import numpy

sys.path.insert(0, os.path.split(__file__)[0])

from .messages_pb2 import (
    CameraSettings, ImageSettings, 
    LightSettings, Light,
    RenderSettings, SceneElement, MeshInfo, 
    VolumeInfo, VolumeLoadResult,
    ClientMessage, RenderResult
)

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


class Connection:
    
    def __init__(self, engine, host, port):
        self.engine = engine
        
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        self.host = host
        self.port = port
        
    def close(self):
        # XXX send bye
        self.sock.close()

    def update(self, data, depsgraph):
        print(data)
        print(depsgraph)
        
        self.engine.update_stats('', 'Connecting')
        self.sock.connect((self.host, self.port))
        
        self.engine.update_stats('', 'Exporting')
        self.export_scene(data, depsgraph)    
    
    def render(self, depsgraph):
        
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
        # XXX perhaps send actual RENDER command?

        num_pixels = self.width * self.height    
        bytes_left = num_pixels * 4*4

        framebuffer = numpy.zeros(bytes_left, dtype=numpy.uint8)
        
        t0 = time.time()
        
        result = self.engine.begin_result(0, 0, self.width, self.height)
        # Only Combined and Depth seem to be available
        layer = result.layers[0].passes["Combined"] 
        
        FBFILE = '/dev/shm/blosprayfb.exr'
        
        sample = 1
        
        while True:
            
            self.engine.update_stats('', 'Rendering sample %d/%d' % (sample, self.render_samples))
            
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
                # of floats isn't possible, hence the OpenEXR file
                result.layers[0].load_from_file(FBFILE)
                
                self.engine.update_result(result)
                
                print('[%6.3f] update_result() done' % (time.time()-t0))

                self.engine.update_progress(sample/self.render_samples)
                
                sample += 1
                
            elif render_result.type == RenderResult.DONE:
                print('Rendering done!')
                break
            
        self.engine.end_result(result)      
        
    #
    # Scene export
    #
    
    def export_volume(self, obj, data, depsgraph):
    
        element = SceneElement()
        element.type = SceneElement.VOLUME
        element.name = obj.name
        
        send_protobuf(self.sock, element)        
        
        volume_info = VolumeInfo()
        volume_info.object2world[:] = matrix2list(obj.matrix_world)
        properties = customproperties2dict(obj)
        volume_info.properties = json.dumps(properties)
        volume_info.object_name = obj.name
        volume_info.mesh_name = obj.data.name
        
        print('Sending properties:')
        print(properties)
        
        send_protobuf(self.sock, volume_info)
        
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
        
        obj['loaded_id'] = id
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
        
        render_settings = RenderSettings()
        render_settings.renderer = scene.ospray.renderer
        # XXX doesn't specify the alpha value, only rgb
        render_settings.background_color[:] = world.ospray.background_color
        self.render_samples = render_settings.samples = scene.ospray.samples
        render_settings.ao_samples = scene.ospray.ao_samples
        render_settings.shadows_enabled = scene.ospray.shadows_enabled
        
        # Lights
        
        light_settings = LightSettings()
        
        light_settings.ambient_color[:] = world.ospray.ambient_color
        light_settings.ambient_intensity = world.ospray.ambient_intensity
        
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
                
            # XXX point lights don't cast hard shadows?
            if data.type == 'POINT':
                # XXX
                light.radius = 0.1
            
            lights.append(light)
                
        # Assigning to lights[:] doesn't work, need to use extend()
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
                # Answer from Brecht:
                # object.hide_render does not tell you if the object is hidden, 
                # it only returns the value of the property as set by the user. This is the same as in 2.7x.
                # To find out which objects are visible in the render, you can loop over depsgraph.object_instances.
                # See also 
                # https://en.blender.org/index.php/Dev:2.8/Source/Depsgraph
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
            mesh_info.object_name = obj.name
            mesh_info.mesh_name = mesh.name
            mesh_info.object2world[:] = matrix2list(obj.matrix_world)
            mesh_info.properties = json.dumps(customproperties2dict(mesh))
            
            flags = 0
        
            # Turn geometry into triangles
            # XXX handle modifiers
                        
            mesh.calc_loop_triangles()
            
            nv = mesh_info.num_vertices = len(mesh.vertices)
            nt = mesh_info.num_triangles = len(mesh.loop_triangles)
            
            print('Object %s - Mesh %s: %d v, %d t' % (obj.name, mesh.name, nv, nt))    
            print(mesh_info.object_name, mesh_info.mesh_name)

            # Check if any faces use smooth shading
            # XXX we currently don't handle meshes with both smooth
            # and non-smooth faces, but those are probably not very common anyway
            
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
            
    # Utility
    
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

