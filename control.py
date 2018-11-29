# Connect to ospray render server, send scene data, let it render and get back image
# PM for CompBioMed
import bpy
from mathutils import Vector
from struct import pack
import socket, numpy
from math import tan, atan, degrees

#HOST = 'elvis.surfsara.nl'
HOST = 'localhost'
PORT = 5909

scene = bpy.context.scene

# XXX make sure the image has the correct dimensions, otherwise
# can segfault
IMAGE = 'Untitled'

"""
Send to renderer:
- framebuffer width, height
- cam type ("perspective")
- cam aspect
- cam pos, viewdir, updir
- cam fovy
- light direction
(- cam nearclip)
(- number of samples)

Response from renderer:
- pixels!

At some point:
- Light info
"""

# Camera

#CAMERA = 'Camera'
#CAMERA = 'Close-up'
#camobj = bpy.data.objects[CAMERA]


actobj = bpy.context.active_object
if actobj.type == 'CAMERA':
    camobj = actobj
else:
    camobj = scene.camera

print(camobj.name)

camdata = camobj.data
cam_xform = camobj.matrix_world

cam_pos = camobj.location
cam_viewdir = cam_xform * Vector((0, 0, -1)) - camobj.location
cam_updir = cam_xform * Vector((0, 1, 0)) - camobj.location

# Image

#width = 1920
#height = 1080

perc = scene.render.resolution_percentage
perc = perc / 100

width = int(scene.render.resolution_x * perc)
height = int(scene.render.resolution_y * perc)

print(perc)

aspect = width / height

# Get camera FOV

# radians!
hfov = camdata.angle   
image_plane_width = 2 * tan(hfov/2)
image_plane_height = image_plane_width / aspect
vfov = 2*atan(image_plane_height/2)

fovy = degrees(vfov)

# Lights

sun_obj = bpy.data.objects['Sun']
sun_data = sun_obj.data

sun_dir = sun_obj.matrix_world * Vector((0, 0, -1)) - sun_obj.location
sun_intensity = sun_data.node_tree.nodes["Emission"].inputs[1].default_value

ambient_obj = bpy.data.objects['Ambient']
ambient_data = ambient_obj.data

ambient_intensity = ambient_data.node_tree.nodes["Emission"].inputs[1].default_value

# Send params

parameters = pack('<HHfffffffffffffff',
    width, height, 
    cam_pos.x, cam_pos.y, cam_pos.z,
    cam_viewdir.x, cam_viewdir.y, cam_viewdir.z,
    cam_updir.x, cam_updir.y, cam_updir.z,
    fovy,
    sun_dir.x, sun_dir.y, sun_dir.z, sun_intensity, ambient_intensity)
    
# Connect 
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0)
sock.connect((HOST, PORT))
# Send parameters
sock.sendall(parameters)

# Get back framebuffer

num_pixels = width * height
bytes_left = num_pixels * 4

pixels = numpy.zeros(num_pixels*4, dtype=numpy.uint8)
view = memoryview(pixels)

while bytes_left > 0:
    n = sock.recv_into(view, bytes_left)
    view = view[n:]
    bytes_left -= n

sock.close()

# Update image

image = bpy.data.images[IMAGE]
image.pixels = pixels.astype(numpy.float) / 255.0