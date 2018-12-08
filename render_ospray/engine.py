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
