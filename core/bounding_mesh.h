#ifndef BOUNDING_MESH_H
#define BOUNDING_MESH_H

#include <stdint.h>
#include <vector>
#include <ospray/ospray.h>

// A plugin can return a bounding mesh, to be used as proxy object
// in the blender scene. The mesh geometry is defined in the same way
// as in Blender: vertices, edges and faces.

struct BoundingMesh
{
    // XXX edges_only=false -> include_faces=true

    // Convenience methods for constructing an axis-aligned bounding box
    static BoundingMesh *bbox(float xmin, float ymin, float zmin, float xmax, float ymax, float zmax, bool edges_only=false);
    static BoundingMesh *bbox_from_group(OSPGroup group, bool edges_only=false);
    static BoundingMesh *bbox_from_instance(OSPInstance instance, bool edges_only=false);

    // Generates a simplified version of the given geometry using quadratic clustering (requires VTK)
    // If VTK support is not enabled it returns add bbox (edges) based on vertex positions.
    static BoundingMesh *simplify_qc(const float *vertices, int nv, const uint32_t *triangles, int nt, int divisions);
    
    // Deserialize
    static BoundingMesh *deserialize(const uint8_t *buffer, uint32_t size);
    
    BoundingMesh();
    ~BoundingMesh();
    
    uint8_t *serialize(uint32_t &size) const;    
    
    std::vector<float>      vertices;       // x, y, z, ...
    std::vector<uint32_t>   edges;          // v0, v1, ...
    std::vector<uint32_t>   faces;          // i, j, k, l, ...
    std::vector<uint32_t>   loop_start;     
    std::vector<uint32_t>   loop_total;     
};

#endif
