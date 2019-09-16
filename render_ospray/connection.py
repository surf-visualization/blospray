# ======================================================================== #
# BLOSPRAY - OSPRay as a Blender render engine                             #
# Paul Melis, SURFsara <paul.melis@surfsara.nl>                            #
# Connection to render server, scene export, result handling               #
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

# - Make sockets non-blocking and use select() to handle errors on the server side

import bpy, bmesh
#from bgl import *
from mathutils import Vector, Matrix

import sys, array, json, os, select, socket, time
from math import tan, atan, degrees, radians
from struct import pack, unpack

import numpy

sys.path.insert(0, os.path.split(__file__)[0])

from .common import PROTOCOL_VERSION, send_protobuf, receive_protobuf
from .messages_pb2 import (
    HelloResult,
    CameraSettings, FramebufferSettings,
    LightSettings, RenderSettings,
    MeshData,
    ClientMessage, GenerateFunctionResult,
    RenderResult,
    UpdateObject, UpdatePluginInstance,
    Volume, Slices, Slice,
    MaterialUpdate, 
    CarPaintSettings, GlassSettings, LuminousSettings, MetallicPaintSettings, OBJMaterialSettings, PrincipledSettings
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

def customproperties2dict(obj, filepath_keys=['file']):
    user_keys = [k for k in obj.keys() if k not in ['_RNA_UI', 'ospray']]
    properties = {}
    for k in user_keys:
        v = obj[k]
        if hasattr(v, 'to_dict'):
            properties[k] = v.to_dict()
        elif hasattr(v, 'to_list'):
            properties[k] = v.to_list()
        elif isinstance(v, str):
            if k == 'file' or k.endswith('_file'):
                # Convert blendfile-relative paths to full paths, e.g.
                # //.../file.name -> /.../.../file.name
                v = bpy.path.abspath(v)
            properties[k] = v
        else:
            # XXX assumes simple type that can be serialized to json
            properties[k] = v

    return properties


class Connection:

    def __init__(self, engine, host, port):
        self.engine = engine

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
        self.host = host
        self.port = port

        self.framebuffer_width = self.framebuffer_height = None

    def connect(self):
        try:
            self.sock.connect((self.host, self.port))
        except:
            return False

        # Handshake
        client_message = ClientMessage()
        client_message.type = ClientMessage.HELLO
        client_message.uint_value = PROTOCOL_VERSION
        send_protobuf(self.sock, client_message)

        result = HelloResult()
        receive_protobuf(self.sock, result)

        if not result.success:
            print('ERROR: Handshake with server:')
            print(result.message)
            return False

        return True

    def close(self):
        client_message = ClientMessage()
        client_message.type = ClientMessage.BYE
        send_protobuf(self.sock, client_message)

        self.sock.close()

    def update(self, data, depsgraph):
        #print(data)
        #print(depsgraph)

        self.engine.update_stats('', 'Connecting')

        # Export scene    
        self.export_scene(data, depsgraph)

        # Connection will be closed by render(), which is always
        # called after update()

    #
    # Scene export
    #

    def _process_properties(self, obj, properties):
        """
        Get Blender custom properties set on obj.
        Custom properties starting with an underscore become
        element properties (key without the underscore), all the others
        become plugin parameters, set in the key "plugin_parameters",
        *but only if a property key 'plugin_type' is set*.
        """

        plugin_parameters = None
        if 'plugin_type' in properties:
            if 'plugin_parameters' in properties:
                plugin_parameters = properties['plugin_parameters']
            else:
                plugin_parameters = properties['plugin_parameters'] = {}

        custom_properties = customproperties2dict(obj)
        for k, v in custom_properties.items():
            #print('k', k, 'v', v)
            if k[0] == '_':
                # This might overwrite one of the already defined properties above
                #print(properties, k, v)
                properties[k[1:]] = v
            elif plugin_parameters is not None:
                plugin_parameters[k] = v
                
    def _process_properties2(self, obj, extract_plugin_parameters=False):
        """
        Get Blender custom properties set on obj.
        Custom properties starting with an underscore become
        element properties (with a key without the underscore), 
        all the others become plugin parameters, but only if
        extract_plugin_parameters is True.
        """

        properties = {}
        plugin_parameters = {}

        for k, v in customproperties2dict(obj).items():
            #print('k', k, 'v', v)
            if k[0] == '_':
                #print(properties, k, v)
                properties[k[1:]] = v
            elif extract_plugin_parameters:
                plugin_parameters[k] = v       
            else:
                properties[k] = v
                
        return properties, plugin_parameters
        

    def send_updated_camera(self, sock, cam_obj, border=None):
        
        # Camera

        cam_xform = cam_obj.matrix_world
        cam_data = cam_obj.data

        camera_settings = CameraSettings()
        camera_settings.object_name = cam_obj.name
        camera_settings.camera_name = cam_data.name

        camera_settings.aspect = self.framebuffer_aspect
        camera_settings.clip_start = cam_data.clip_start
        # XXX no far clip in ospray :)

        if cam_data.type == 'PERSP':
            camera_settings.type = CameraSettings.PERSPECTIVE

            hfov = vfov = None

            if cam_data.sensor_fit == 'AUTO':
                if self.framebuffer_aspect >= 1:
                    # Horizontal
                    hfov = cam_data.angle
                else:
                    # Vertical
                    vfov = cam_data.angle
            elif cam_data.sensor_fit == 'HORIZONTAL':
                hfov = cam_data.angle
            else:
                vfov = cam_data.angle

            # Blender provides FOV in radians
            # OSPRay needs vertical FOV in degrees
            if vfov is None:
                image_plane_width = 2 * tan(hfov/2)
                image_plane_height = image_plane_width / self.framebuffer_aspect
                vfov = 2*atan(image_plane_height/2)
                
            camera_settings.fov_y = degrees(vfov)

        elif cam_data.type == 'ORTHO':
            camera_settings.type = CameraSettings.ORTHOGRAPHIC
            camera_settings.height = cam_data.ortho_scale / self.framebuffer_aspect

        elif cam_data.type == 'PANO':
            camera_settings.type = CameraSettings.PANORAMIC

        else:
            raise ValueError('Unknown camera type "%s"' % cam_data.type)

        camera_settings.position[:] = list(cam_obj.location)
        camera_settings.view_dir[:] = list(cam_xform @ Vector((0, 0, -1)) - cam_obj.location)
        camera_settings.up_dir[:] = list(cam_xform @ Vector((0, 1, 0)) - cam_obj.location)

        # Depth of field
        camera_settings.dof_focus_distance = 0
        camera_settings.dof_aperture = 0.0

        if cam_data.dof.use_dof:
            dof_settings = cam_data.dof
            if dof_settings.focus_object is not None:
                focus_world = dof_settings.focus_object.matrix_world.translation
                cam_world = cam_obj.matrix_world.translation
                camera_settings.dof_focus_distance = (focus_world - cam_world).length
            else:
                camera_settings.dof_focus_distance = dof_settings.focus_distance

            # Camera focal length in mm + f-stop -> aperture in m
            camera_settings.dof_aperture = (0.5 * cam_data.lens / dof_settings.aperture_fstop) / 1000

        if border is not None:
            camera_settings.border[:] = border
            
        client_message = ClientMessage()
        client_message.type = ClientMessage.UPDATE_CAMERA

        send_protobuf(self.sock, client_message)
        send_protobuf(self.sock, camera_settings)
        
    def export_scene(self, data, depsgraph):

        msg = 'Exporting scene'
        self.engine.update_stats('', msg)
        print(msg)    

        scene = depsgraph.scene
        render = scene.render
        world = scene.world

        # Make sure this is the first thing we send
        client_message = ClientMessage()
        client_message.type = ClientMessage.UPDATE_RENDERER_TYPE
        client_message.string_value = scene.ospray.renderer
        send_protobuf(self.sock, client_message)

        client_message = ClientMessage()
        client_message.type = ClientMessage.UPDATE_SCENE
        send_protobuf(self.sock, client_message)

        scale = render.resolution_percentage / 100.0
        self.framebuffer_width = int(render.resolution_x * scale)
        self.framebuffer_height = int(render.resolution_y * scale)
        self.framebuffer_aspect = self.framebuffer_width / self.framebuffer_height

        print("%d x %d (scale %d%%) -> %d x %d (aspect %.3f)" % \
            (render.resolution_x, render.resolution_y, render.resolution_percentage,
            self.framebuffer_width, self.framebuffer_height, self.framebuffer_aspect))

        # Image

        border = None
        framebuffer_settings = FramebufferSettings()

        if render.use_border:
            # XXX nice, in ospray the render region is set on the *camera*,
            # while in blender it is a *render* setting, but we pass it as
            # an *image* setting :)

            # Blender: X to the right, Y up, i.e. (0,0) is lower-left, same
            # as ospray. BUT: ospray always fills up the complete framebuffer
            # with the specified image region, so we don't have a direct
            # equivalent of only rendering a sub-region of the full
            # framebuffer as in blender :-/
            # We need to set the framebuffer resolution to the cropped region
            # as well.

            min_x = render.border_min_x
            min_y = render.border_min_y
            max_x = render.border_max_x
            max_y = render.border_max_y
            print('Border render enabled: %.3f, %.3f -> %.3f, %.3f' % (min_x, min_y, max_x, max_y))

            left = int(min_x*self.framebuffer_width)
            right = int(max_x*self.framebuffer_width)
            bottom = int(min_y*self.framebuffer_height)
            top = int(max_y*self.framebuffer_height)

            # Crop region in ospray is set in normalized screen-space coordinates,
            # i.e. bottom-left of pixel (i,j) is (i,j), but top-right is (i+1,j+1)
            border = [
                left/self.framebuffer_width, bottom/self.framebuffer_height,
                (right+1)/self.framebuffer_width, (top+1)/self.framebuffer_height
            ]

            self.framebuffer_width = right - left + 1
            self.framebuffer_height = top - bottom + 1
            # XXX we don't update the fb aspect when border render is active as the camera
            # settings need to full fb aspect
            #self.framebuffer_aspect = self.framebuffer_width / self.framebuffer_height
            print('Framebuffer for border render: %d x %d' % (self.framebuffer_width, self.framebuffer_height))

        framebuffer_settings.width = self.framebuffer_width
        framebuffer_settings.height = self.framebuffer_height

        # Render settings

        render_settings = RenderSettings()
        render_settings.renderer = scene.ospray.renderer
        render_settings.type = RenderSettings.FINAL
        render_settings.background_color[:] = world.ospray.background_color
        self.render_samples = render_settings.samples = scene.ospray.samples
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
        #render_settings.shadows_enabled = scene.ospray.shadows_enabled     # XXX removed in 2.0?

        #
        # Send scene
        #

        # Image settings
        send_protobuf(self.sock, framebuffer_settings)

        # Render settings
        send_protobuf(self.sock, render_settings)

        # Camera settings
        self.send_updated_camera(self.sock, scene.camera, border)        

        # Scene objects

        # XXX turn into render setting
        self.send_updated_ambient_light(world.ospray.ambient_color, world.ospray.ambient_intensity)

        self.mesh_data_exported = set()
        self.materials_exported = set()

        print('DEPSGRAPH STATS:', depsgraph.debug_stats())

        for instance in depsgraph.object_instances:

            obj = instance.object

            #print('DEPSGRAPH object instance "%s", type=%s, is_instance=%d, random_id=%d' % \
            #    (obj.name, obj.type, instance.is_instance, instance.random_id))

            if obj.type == 'LIGHT':
                self.send_updated_light(data, depsgraph, obj)
                continue
            elif obj.type != 'MESH':
                continue
        
            # Send object linking to the mesh data
            self.send_updated_mesh_object(data, depsgraph, obj, obj.data, instance.matrix_world, instance.is_instance, instance.random_id)        

    def send_updated_ambient_light(self, color, intensity):

        light_settings = LightSettings()
        light_settings.type = LightSettings.AMBIENT
        light_settings.object_name = '<ambient>'

        light_settings.color[:] = color
        light_settings.intensity = intensity

        client_message = ClientMessage()
        client_message.type = ClientMessage.UPDATE_OBJECT  

        update = UpdateObject()
        update.type = UpdateObject.LIGHT
        update.name = '<ambient>'
        
        # XXX using three messages :-/
        send_protobuf(self.sock, client_message)
        send_protobuf(self.sock, update)
        send_protobuf(self.sock, light_settings)


    def send_updated_light(self, data, depsgraph, obj):

        self.engine.update_stats('', 'Light %s' % obj.name)

        TYPE2ENUM = dict(POINT=LightSettings.POINT, SUN=LightSettings.SUN, SPOT=LightSettings.SPOT, AREA=LightSettings.AREA)

        data = obj.data
        xform = obj.matrix_world

        ospray_data = data.ospray

        light_settings = LightSettings()
        light_settings.type = TYPE2ENUM[data.type]
        light_settings.object2world[:] = matrix2list(xform)      # XXX get from updateobject
        light_settings.object_name = obj.name
        light_settings.light_name = data.name

        light_settings.color[:] = data.color
        light_settings.intensity = ospray_data.intensity
        light_settings.visible = ospray_data.visible

        if data.type == 'SUN':
            light_settings.angular_diameter = ospray_data.angular_diameter
        elif data.type != 'AREA':
            light_settings.position[:] = (xform[0][3], xform[1][3], xform[2][3])

        if data.type in ['SUN', 'SPOT']:
            light_settings.direction[:] = obj.matrix_world @ Vector((0, 0, -1)) - obj.location

        if data.type == 'SPOT':
            # Blender:
            # .spot_size = full angle where light shines, in degrees
            # .spot_blend = factor in [0,1], 0 = no penumbra, 1 = penumbra is full angle
            light_settings.opening_angle = degrees(data.spot_size)
            light_settings.penumbra_angle = 0.5*data.spot_blend*degrees(data.spot_size)
            # assert light.penumbra_angle < 0.5*light.opening_angle

        if data.type in ['POINT', 'SPOT']:
            light_settings.radius = data.shadow_soft_size        # XXX what is this called in ospray?

        if data.type == 'AREA':
            size_x = data.size
            size_y = data.size_y

            # Local
            position = Vector((-0.5*size_x, -0.5*size_y, 0))
            edge1 = position + Vector((0, size_y, 0))
            edge2 = position + Vector((size_x, 0, 0))

            # World
            position = obj.matrix_world @ position
            edge1 = obj.matrix_world @ edge1 - position
            edge2 = obj.matrix_world @ edge2 - position

            light_settings.position[:] = position
            light_settings.edge1[:] = edge1
            light_settings.edge2[:] = edge2

        client_message = ClientMessage()
        client_message.type = ClientMessage.UPDATE_OBJECT  

        update = UpdateObject()
        update.type = UpdateObject.LIGHT
        update.name = obj.name
        update.object2world[:] = matrix2list(xform)
        update.data_link = data.name
        
        custom_properties, plugin_parameters = self._process_properties2(obj, False)
        assert len(plugin_parameters.keys()) == 0
        
        update.custom_properties = json.dumps(custom_properties)  

        # XXX using three messages :-/
        send_protobuf(self.sock, client_message)
        send_protobuf(self.sock, update)
        send_protobuf(self.sock, light_settings)

    def send_updated_material(self, data, depsgraph, material):

        name = material.name        
        
        if name in self.materials_exported:
            print('Not sending MATERIAL "%s" again' % name)
            return

        print('Updating MATERIAL "%s"' % name)

        if not material.use_nodes:
            print('... WARNING: material does not use nodes, not sending!')
            return

        client_message = ClientMessage()
        client_message.type = ClientMessage.UPDATE_MATERIAL  

        update = MaterialUpdate()        
        update.name = name

        tree = material.node_tree
        assert tree.type == 'SHADER'

        nodes = tree.nodes
        print('... %d shader nodes in tree' % len(nodes))

        # Get output node
        # XXX tree.get_output_node() doesn't return anything?
        output = nodes.get('Output')

        # Find node attached to the output
        shadernode = None
        for link in tree.links:
            if link.to_node == output:
                shadernode = link.from_node
                break
        
        if shadernode is None:
            print('... WARNING: no shader node attached to output!')
            return
        
        idname = shadernode.bl_idname
        inputs = shadernode.inputs

        if idname == 'OSPRayCarPaint':
            update.type = MaterialUpdate.CAR_PAINT
            settings = CarPaintSettings()
            settings.base_color[:] = inputs['Base color'].default_value[:3]  
            settings.roughness = inputs['Roughness'].default_value
            settings.normal = inputs['Normal'].default_value
            settings.flake_density = inputs['Flake density'].default_value
            settings.flake_scale = inputs['Flake scale'].default_value            
            settings.flake_spread = inputs['Flake spread'].default_value
            settings.flake_jitter = inputs['Flake jitter'].default_value
            settings.flake_roughness = inputs['Flake roughness'].default_value
            settings.coat = inputs['Coat'].default_value
            settings.coat_ior = inputs['Coat IOR'].default_value
            settings.coat_color[:] = inputs['Coat color'].default_value[:3]
            settings.coat_thickness = inputs['Coat thickness'].default_value
            settings.coat_roughness = inputs['Coat roughness'].default_value
            settings.coat_normal = inputs['Coat normal'].default_value
            settings.flipflop_color[:] = inputs['Flipflop color'].default_value[:3]
            settings.flipflop_falloff = inputs['Flipflop falloff'].default_value

        elif idname == 'OSPRayGlass':
            update.type = MaterialUpdate.GLASS
            settings = GlassSettings()
            settings.eta = inputs['Eta'].default_value   
            settings.attenuation_color[:] = list(inputs['Attenuation color'].default_value)[:3]
            settings.attenuation_distance = inputs['Attenuation distance'].default_value

        elif idname == 'OSPRayLuminous':
            update.type = MaterialUpdate.LUMINOUS
            settings = LuminousSettings()
            settings.color[:] = inputs['Color'].default_value[:3]  
            settings.intensity = inputs['Intensity'].default_value
            settings.transparency = inputs['Transparency'].default_value

        elif idname == 'OSPRayMetallicPaint':
            update.type = MaterialUpdate.METALLIC_PAINT
            settings = MetallicPaintSettings()
            settings.base_color[:] = inputs['Base color'].default_value[:3]  
            settings.flake_color[:] = inputs['Flake color'].default_value[:3]  
            settings.flake_amount = inputs['Flake amount'].default_value
            settings.flake_spread = inputs['Flake spread'].default_value
            settings.eta = inputs['Eta'].default_value

        elif idname == 'OSPRayOBJMaterial':
            update.type = MaterialUpdate.OBJMATERIAL
            settings = OBJMaterialSettings()
            settings.kd[:] = list(inputs['Diffuse'].default_value)[:3]
            settings.ks[:] = list(inputs['Specular'].default_value)[:3]
            settings.ns = inputs['Shininess'].default_value
            settings.d = inputs['Opacity'].default_value

        elif idname == 'OSPRayPrincipled':
            update.type = MaterialUpdate.PRINCIPLED
            settings = PrincipledSettings()
            settings.base_color[:] = inputs['Base color'].default_value[:3]  
            settings.edge_color[:] = inputs['Edge color'].default_value[:3]  
            settings.metallic = inputs['Metallic'].default_value
            settings.diffuse = inputs['Diffuse'].default_value
            settings.specular = inputs['Specular'].default_value
            settings.ior = inputs['IOR'].default_value
            settings.transmission = inputs['Transmission'].default_value
            settings.transmission_color[:] = inputs['Transmission color'].default_value[:3]
            settings.transmission_depth = inputs['Transmission depth'].default_value
            settings.roughness = inputs['Roughness'].default_value
            settings.anisotropy = inputs['Anisotropy'].default_value
            settings.rotation = inputs['Rotation'].default_value
            settings.normal = inputs['Normal'].default_value
            settings.base_normal = inputs['Base normal'].default_value
            settings.thin = inputs['Thin'].default_value
            settings.thickness = inputs['Thickness'].default_value
            settings.backlight = inputs['Backlight'].default_value
            settings.coat = inputs['Coat'].default_value
            settings.coat_ior = inputs['Coat IOR'].default_value
            settings.coat_color[:] = inputs['Coat color'].default_value[:3]
            settings.coat_thickness = inputs['Coat thickness'].default_value
            settings.coat_roughness = inputs['Coat roughness'].default_value
            settings.coat_normal = inputs['Coat normal'].default_value
            settings.sheen = inputs['Sheen'].default_value
            settings.sheen_color[:] = inputs['Sheen color'].default_value[:3]
            settings.sheen_tint = inputs['Sheen tint'].default_value
            settings.sheen_roughness = inputs['Sheen roughness'].default_value
            settings.opacity = inputs['Opacity'].default_value

        else:
            print('... WARNING: shader of type "%s" not handled!' % shadernode.bl_idname)
            return

        # XXX three messages
        send_protobuf(self.sock, client_message)
        send_protobuf(self.sock, update)
        send_protobuf(self.sock, settings)

        self.materials_exported.add(name)


    def send_updated_mesh_object(self, data, depsgraph, obj, mesh, matrix_world, is_instance, random_id):

        # We do a bit of the logic here in determining what a certain
        # mesh object -> mesh data combination (including their properties)
        # means, so the server isn't bothered with this.

        # XXX for now we assume materials are only linked to object data in the blender
        # scene (and not to objects, even though that is possible).
        # The exported OSPRay scene *does* have materials linked to objects, though,
        # as that is the only way to do it. That's also the reason we handle materials
        # in the object export and not the mesh export. Here we have the detail needed.

        mesh = obj.data

        # Send mesh data first, so the object can refer to it
        self.send_updated_mesh_data(data, depsgraph, mesh)
                    
        name = obj.name
        
        s = 'Updating MESH OBJECT "%s"' % name
        if is_instance:
            s += ' (instance %d)' % random_id
            name = '%s [%d]' % (name, random_id)
        print(s)

        client_message = ClientMessage()
        client_message.type = ClientMessage.UPDATE_OBJECT    
        
        update = UpdateObject()
        update.name = name
        update.object2world[:] = matrix2list(matrix_world)
        update.data_link = mesh.name
        
        custom_properties, plugin_parameters = self._process_properties2(obj, False)
        assert len(plugin_parameters.keys()) == 0
        
        update.custom_properties = json.dumps(custom_properties)    

        if not obj.ospray.ospray_override or not mesh.ospray.plugin_enabled:
            # Not OSPRay enabled or plugin disabled on the linked mesh data.
            # Treat as regular blender Mesh object            
            update.type = UpdateObject.MESH
                
            # Check that this object isn't a child used for slicing, in which case it 
            # should not be sent as normal geometry
            parent = obj.parent 
            if (parent is not None) and parent.ospray.ospray_override:
                assert parent.type == 'MESH'
                parent_mesh = parent.data
                if (parent_mesh.ospray.plugin_type == 'volume') and (parent.ospray.volume_usage == 'slices'):
                    print('Object "%s" is child of slice-enabled parent "%s", not sending' % (obj.name, parent.name))
                    return

            # Update material first (if set)

            if len(obj.material_slots) > 0:
                if len(obj.material_slots) > 1:
                    print('WARNING: only exporting a single material slot!')

                mslot = obj.material_slots[0]

                if mslot.link == 'DATA':
                    material = obj.data.materials[0]
                else:
                    # Material linked to object
                    material = mslot.material

                self.send_updated_material(data, depsgraph, material)

                update.material_link = material.name

            # Send object itself            
            send_protobuf(self.sock, client_message)
            send_protobuf(self.sock, update)                    

        else:        

            extra = []
        
            plugin_type = mesh.ospray.plugin_type
            print("Plugin type = %s" % plugin_type)
            
            if plugin_type == 'geometry':
                update.type = UpdateObject.GEOMETRY

                # XXX yuck, duplicated from above
                if len(obj.material_slots) > 0:
                    if len(obj.material_slots) > 1:
                        print('WARNING: only exporting a single material slot!')
                    mslot = obj.material_slots[0]
                    assert mslot.link == 'DATA'
                    material = obj.data.materials[0]

                    self.send_updated_material(data, depsgraph, material)

                    update.material_link = material.name
                
            elif plugin_type == 'scene':
                update.type = UpdateObject.SCENE   
                
            elif plugin_type == 'volume':
                
                # XXX properties: single shade, gradient shading, ...
                
                volume_usage = obj.ospray.volume_usage
                print('Volume usage is %s' % volume_usage)

                if volume_usage == 'volume':
                    update.type = UpdateObject.VOLUME
                    volume = Volume()
                    volume.sampling_rate = obj.ospray.sampling_rate
                    extra.append(volume)

                elif volume_usage == 'slices':
                    update.type = UpdateObject.SLICES
                    #  Process child objects for slices
                    ss = []
                    # Apparently the depsgraph leaves out the parenting? So get
                    # that information from the original object
                    # XXX need to ignore slice object itself in export, but not its mesh data
                    children = depsgraph.scene.objects[obj.name].children
                    print('Object "%s" has %d CHILDREN' % (obj.name, len(children)))

                    # XXX volumetric texture and geometric model share the same coordinate space.
                    # child.matrix_local should provide the transform of child in the parent's
                    # coordinate system. unfortunately, this means we need to use this transform
                    # to actually update the geometry on the server to use for slicing. we can
                    # then transform it with the parent's transform to get it in the right position.                        

                    for childobj in children:

                        if not childobj.type == 'MESH':
                            print('Ignoring slicing child object "%s", it\'s not a mesh' % childobj.name)
                            continue

                        if childobj.hide_render:
                            print('Ignoring slicing child object "%s", it\'s hidden for rendering' % childobj.name)
                            continue

                        print('Sending slicing child mesh "%s"' % childobj.data.name)

                        self.update_blender_mesh(data, depsgraph, childobj.data, childobj.matrix_local)

                        slice = Slice()
                        slice.linked_mesh = childobj.data.name
                        # Note: this is the parent's object-to-world transform
                        slice.object2world[:] = matrix2list(obj.matrix_world)
                        ss.append(slice)                            

                    slices = Slices()
                    slices.slices.extend(ss)
                    extra.append(slices)

                elif volume_usage == 'isosurfaces':
                    # Isosurface values are read from the custom property 'isovalue'
                    update.type = UpdateObject.ISOSURFACES
                                    
            send_protobuf(self.sock, client_message)
            send_protobuf(self.sock, update)
            for msg in extra:
                send_protobuf(self.sock, msg)
    

    def send_updated_mesh_data(self, data, depsgraph, mesh):
        
        """
        Send an update on a Mesh Data block
        """
                
        if mesh.name in self.mesh_data_exported:
            print('Not sending MESH DATA "%s" again' % mesh.name)
            return
                
        if mesh.ospray.plugin_enabled:
            # Plugin instance
            
            print('Updating plugin-enabled mesh "%s"' % mesh.name)
            
            ospray = mesh.ospray            
            plugin_enabled = ospray.plugin_enabled
            plugin_name = ospray.plugin_name
            plugin_type = ospray.plugin_type
            
            custom_properties, plugin_parameters = self._process_properties2(mesh, True)
            
            self.update_plugin_instance(mesh.name, plugin_type, plugin_name, plugin_parameters, custom_properties)

        else:
            # Treat as regular blender mesh
            # XXX any need to send custom properties?
            self.update_blender_mesh(data, depsgraph, mesh) 

        # Remember that we exported this mesh
        self.mesh_data_exported.add(mesh.name)        


    def update_plugin_instance(self, name, plugin_type, plugin_name, plugin_parameters, custom_properties):
        
        self.engine.update_stats('', 'Updating plugin instance %s (plugin type %s)' % (name, plugin_type))
        
        type2enum = dict(
            geometry = UpdatePluginInstance.GEOMETRY,
            volume = UpdatePluginInstance.VOLUME,
            scene = UpdatePluginInstance.SCENE
        )
        
        client_message = ClientMessage()
        client_message.type = ClientMessage.UPDATE_PLUGIN_INSTANCE            
        
        update = UpdatePluginInstance()
        update.type = type2enum[plugin_type]
        update.name = name
        
        update.plugin_name = plugin_name
        update.plugin_parameters = json.dumps(plugin_parameters)
        update.custom_properties = json.dumps(custom_properties)
        
        send_protobuf(self.sock, client_message)
        send_protobuf(self.sock, update)
        
        generate_function_result = GenerateFunctionResult()
        
        receive_protobuf(self.sock, generate_function_result)
        
        if not generate_function_result.success:
            print('ERROR: plugin generation failed:')
            print(generate_function_result.message)
            return
        

    def update_blender_mesh(self, data, depsgraph, mesh, xform=None):

        if mesh.name in self.mesh_data_exported:
            print('Not updating MESH DATA "%s", already sent' % mesh.name)
            return
    
        # XXX we should export meshes separately, keeping a local
        # list which ones we already exported (by name).
        # Then for MESH objects use the name of the mesh to instantiate
        # it using the given xform. This gives us real instancing.
        # But a user can change a mesh's name. However, we can
        # sort of handle this by using the local name list and deleting
        # (also on the server) whichever name's we don't see when exporting.
        # Could also set a custom property on meshes with a unique ID
        # we choose ourselves. But props get copied when duplicating
        # See https://devtalk.blender.org/t/universal-unique-id-per-object/363/3

        self.engine.update_stats('', 'Updating Blender MESH DATA "%s"' % mesh.name)
        
        client_message = ClientMessage()
        client_message.type = ClientMessage.UPDATE_BLENDER_MESH       
        client_message.string_value = mesh.name
        
        send_protobuf(self.sock, client_message)
        
        # Send the actual mesh geometry
        
        mesh_data = MeshData()
        flags = 0

        # Send (triangulated) geometry

        mesh.calc_loop_triangles()

        nv = mesh_data.num_vertices = len(mesh.vertices)
        nt = mesh_data.num_triangles = len(mesh.loop_triangles)

        print('MESH DATA "%s": %d vertices, %d triangles' % (mesh.name, nv, nt))

        # Check if any faces use smooth shading
        # XXX we currently don't handle meshes with both smooth
        # and non-smooth faces, but those are probably not very common anyway

        use_smooth = False
        for tri in mesh.loop_triangles:
            if tri.use_smooth:
                print('... mesh uses smooth shading')
                use_smooth = True
                flags |= MeshData.NORMALS
                break

        # Vertex colors
        #https://blender.stackexchange.com/a/8561
        if mesh.vertex_colors:
            flags |= MeshData.VERTEX_COLORS

        # Send mesh data

        mesh_data.flags = flags

        send_protobuf(self.sock, mesh_data)

        # Send vertices

        vertices = numpy.empty(nv*3, dtype=numpy.float32)

        if xform is None:
            xform = Matrix()    # Identity

        for idx, v in enumerate(mesh.vertices):
                p = xform @ v.co
                vertices[3*idx+0] = p.x
                vertices[3*idx+1] = p.y
                vertices[3*idx+2] = p.z
            
        #print(vertices)

        self.sock.send(vertices.tobytes())

        # Send vertex normals (if set)

        if use_smooth:
            normals = numpy.empty(nv*3, dtype=numpy.float32)

            for idx, v in enumerate(mesh.vertices):
                # XXX use .index?
                n = v.normal
                normals[3*idx+0] = n.x
                normals[3*idx+1] = n.y
                normals[3*idx+2] = n.z

            self.sock.send(normals.tobytes())

        # Send vertex colors (if set)

        if mesh.vertex_colors:
            vcol_layer = mesh.vertex_colors.active
            vcol_data = vcol_layer.data

            vertex_colors = numpy.empty(nv*4, dtype=numpy.float32)

            for poly in mesh.polygons:
                for loop_index in poly.loop_indices:
                    loop_vert_index = mesh.loops[loop_index].vertex_index
                    color = vcol_data[loop_index].color
                    # RGBA vertex colors in Blender 2.8x
                    vertex_colors[4*loop_vert_index+0] = color[0]
                    vertex_colors[4*loop_vert_index+1] = color[1]
                    vertex_colors[4*loop_vert_index+2] = color[2]
                    vertex_colors[4*loop_vert_index+3] = 1.0

            self.sock.send(vertex_colors.tobytes())

        # Send triangles

        triangles = numpy.empty(nt*3, dtype=numpy.uint32)   # XXX opt possible with <64k vertices ;-)

        for idx, tri in enumerate(mesh.loop_triangles):
            triangles[3*idx+0] = tri.vertices[0]
            triangles[3*idx+1] = tri.vertices[1]
            triangles[3*idx+2] = tri.vertices[2]
            
        #print(triangles)

        self.sock.send(triangles.tobytes())

        self.mesh_data_exported.add(mesh.name)

                
                
    #
    # Rendering
    #
    
    def render(self, depsgraph):

        """
        if self.is_preview:
            self.render_preview(depsgraph)
        else:
            self.render_scene(depsgraph)
        """

        # Signal server to start rendering

        client_message = ClientMessage()
        client_message.type = ClientMessage.START_RENDERING
        send_protobuf(self.sock, client_message)

        # Read back successive framebuffer samples

        num_pixels = self.framebuffer_width * self.framebuffer_height
        # RGBA floats
        framebuffer = numpy.zeros(num_pixels*4*4, dtype=numpy.uint8)

        t0 = time.time()

        result = self.engine.begin_result(0, 0, self.framebuffer_width, self.framebuffer_height)
        # Only Combined and Depth seem to be available
        layer = result.layers[0].passes["Combined"]

        FBFILE = '/dev/shm/blosprayfb.exr'

        sample = 1
        cancel_sent = False

        self.engine.update_stats('', 'Rendering sample %d/%d' % (sample, self.render_samples))

        # XXX this loop blocks too often, might need to move it to a separate thread,
        # but OTOH we're already using select() to detect when to read
        while True:

            # Check for incoming render results

            r, w, e = select.select([self.sock], [], [], 0)

            if len(r) == 1:

                render_result = RenderResult()
                # XXX handle receive error
                receive_protobuf(self.sock, render_result)

                if render_result.type == RenderResult.FRAME:

                    # New framebuffer (for a single pixel sample) is available

                    """
                    # XXX Slow: get as raw block of floats
                    print('[%6.3f] _read_framebuffer start' % (time.time()-t0))
                    self._read_framebuffer(framebuffer, self.framebuffer_width, self.framebuffer_height)
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

                    # We read the framebuffer file content from the server
                    # and locally write it to FBFILE, which then gets loaded by Blender

                    # XXX both receiving into a file and loading from file 
                    # block the blender UI for a short time

                    #print('[%6.3f] _read_framebuffer_to_file start' % (time.time()-t0))
                    self._read_framebuffer_to_file(FBFILE, render_result.file_size)
                    #print('[%6.3f] _read_framebuffer_to_file end' % (time.time()-t0))

                    # Sigh, this needs an image file format. I.e. reading in a raw framebuffer
                    # of floats isn't possible, hence the OpenEXR file
                    # XXX result.load_from_file(...), instead of result.layers[0].load_from_file(...), would work as well?
                    result.layers[0].load_from_file(FBFILE)

                    # Remove file
                    os.unlink(FBFILE)

                    self.engine.update_result(result)

                    #print('[%6.3f] update_result() done' % (time.time()-t0))

                    self.engine.update_progress(sample/self.render_samples)

                    sample += 1

                    # XXX perhaps use update_memory_stats()

                    self.engine.update_stats(
                        'Server %.1f MB' % render_result.memory_usage,
                        'Variance %.3f | Rendering sample %d/%d' % (render_result.variance, sample, self.render_samples))

                elif render_result.type == RenderResult.CANCELED:
                    print('Rendering CANCELED!')
                    self.engine.update_stats('', 'Rendering canceled')
                    cancel_sent = True
                    break

                elif render_result.type == RenderResult.DONE:
                    # XXX this message is never really shown, the final timing stats get shown instead
                    self.engine.update_stats('', 'Variance %.3f | Rendering done' % render_result.variance)
                    print('Rendering done!')
                    break

            # Check if render was canceled

            if self.engine.test_break() and not cancel_sent:
                client_message = ClientMessage()
                client_message.type = ClientMessage.CANCEL_RENDERING
                send_protobuf(self.sock, client_message)
                cancel_sent = True

            time.sleep(0.001)

        self.engine.end_result(result)

        print('Done with render loop')

    # Utility

    """
    def _read_framebuffer(self, framebuffer, width, height):

        # XXX use select() in a loop, to allow UI updates more frequently

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
    """
    
    def _read_framebuffer_to_file(self, fname, size):

        #print('_read_framebuffer_to_file(%s, %d)' % (fname, size))

        # XXX use select() in a loop, to allow UI updates more frequently

        with open(fname, 'wb') as f:
            bytes_left = size
            while bytes_left > 0:
                d = self.sock.recv(bytes_left)
                # XXX check d
                f.write(d)
                bytes_left -= len(d)
