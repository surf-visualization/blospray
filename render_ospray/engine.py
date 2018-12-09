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




if __name__ == "__main__":
    register()
