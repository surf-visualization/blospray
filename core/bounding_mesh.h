#ifndef BOUNDING_MESH_H
#define BOUNDING_MESH_H

#include <stdint.h>
#include <vector>

// A plugin can return a bounding mesh, to be used as proxy object
// in the blender scene. The mesh geometry is defined in the same way
// as in Blender: vertices, edges and polygons.

struct BoundingMesh
{
    // Convenience methods for constructing an axis-aligned bounding box
    static BoundingMesh *bbox_edges(float xmin, float ymin, float zmin, float xmax, float ymax, float zmax);
    static BoundingMesh *bbox_mesh(float xmin, float ymin, float zmin, float xmax, float ymax, float zmax);
    
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
