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
    #imp.reload(render)
    #imp.reload(update_files)
    
import bpy
    
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



classes = (
    OsprayRenderEngine,
)

def register():
    from bpy.utils import register_class
    
    from . import properties
    from . import ui
    #from . import render
    #from . import update_files
    
    properties.register()
    ui.register()
    
    for cls in classes:
        register_class(cls)
    
    
def unregister():
    from bpy.utils import unregister_class
    
    from . import properties
    from . import ui
    #from . import render
    #from . import update_files
    
    properties.unregister()
    ui.unregister()
    
    for cls in classes:
        unregister_class(cls)
        
    
if __name__ == "__main__":
    register()        
    