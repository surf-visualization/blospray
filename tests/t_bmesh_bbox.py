import bpy, bmesh
from mathutils import Vector

obj = bpy.context.active_object
mesh = obj.data

id = '12345'
print(id)

mesh['loaded_id'] = id

bbox = [0.0, 0.0, 0.0, 2000.0, 1000.0, 1000.0]

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

bm = bmesh.new()

bm_verts = []
for vi, v in enumerate(verts):
    bm_verts.append(bm.verts.new(v))
    
for i, j in edges:
    bm.edges.new((bm_verts[i], bm_verts[j]))

bm.to_mesh(mesh)
bm.free()

mesh.validate(verbose=True)

print([v.co for v in mesh.vertices])
