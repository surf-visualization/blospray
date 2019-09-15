#include "bounding_mesh.h"

BoundingMesh*
BoundingMesh::bbox_edges(float xmin, float ymin, float zmin, float xmax, float ymax, float zmax)
{        
    BoundingMesh *bm = new BoundingMesh;
    
    std::vector<float> &vertices = bm->vertices;
    std::vector<uint32_t> &edges = bm->edges;
    
    vertices.push_back(xmin);    vertices.push_back(ymin);    vertices.push_back(zmin);
    vertices.push_back(xmax);    vertices.push_back(ymin);    vertices.push_back(zmin);
    vertices.push_back(xmax);    vertices.push_back(ymax);    vertices.push_back(zmin);
    vertices.push_back(xmin);    vertices.push_back(ymax);    vertices.push_back(zmin);
    
    vertices.push_back(xmin);    vertices.push_back(ymin);    vertices.push_back(zmax);
    vertices.push_back(xmax);    vertices.push_back(ymin);    vertices.push_back(zmax);
    vertices.push_back(xmax);    vertices.push_back(ymax);    vertices.push_back(zmax);
    vertices.push_back(xmin);    vertices.push_back(ymax);    vertices.push_back(zmax);
    
    edges.push_back(0);     edges.push_back(1);
    edges.push_back(1);     edges.push_back(2);
    edges.push_back(2);     edges.push_back(3);
    edges.push_back(3);     edges.push_back(0);

    edges.push_back(4);     edges.push_back(5);
    edges.push_back(5);     edges.push_back(6);
    edges.push_back(6);     edges.push_back(7);
    edges.push_back(7);     edges.push_back(4);
    
    edges.push_back(0);     edges.push_back(4);
    edges.push_back(1);     edges.push_back(5);
    edges.push_back(2);     edges.push_back(6);
    edges.push_back(3);     edges.push_back(7);
    
    return bm;
}    

BoundingMesh*
BoundingMesh::bbox_mesh(float xmin, float ymin, float zmin, float xmax, float ymax, float zmax)
{
    BoundingMesh *bm = new BoundingMesh;
    
    std::vector<float>      &vertices = bm->vertices;
    std::vector<uint32_t>   &faces = bm->faces;
    std::vector<uint32_t>   &loop_start = bm->loop_start;
    std::vector<uint32_t>   &loop_total = bm->loop_total;
    
    vertices.push_back(xmin);    vertices.push_back(ymin);    vertices.push_back(zmin);
    vertices.push_back(xmax);    vertices.push_back(ymin);    vertices.push_back(zmin);
    vertices.push_back(xmax);    vertices.push_back(ymax);    vertices.push_back(zmin);
    vertices.push_back(xmin);    vertices.push_back(ymax);    vertices.push_back(zmin);
    
    vertices.push_back(xmin);    vertices.push_back(ymin);    vertices.push_back(zmax);
    vertices.push_back(xmax);    vertices.push_back(ymin);    vertices.push_back(zmax);
    vertices.push_back(xmax);    vertices.push_back(ymax);    vertices.push_back(zmax);
    vertices.push_back(xmin);    vertices.push_back(ymax);    vertices.push_back(zmax);
    
    faces.push_back(0); faces.push_back(1); faces.push_back(5); faces.push_back(4);
    faces.push_back(1); faces.push_back(2); faces.push_back(6); faces.push_back(5);
    faces.push_back(5); faces.push_back(6); faces.push_back(7); faces.push_back(4);
    faces.push_back(2); faces.push_back(6); faces.push_back(7); faces.push_back(3);
    faces.push_back(3); faces.push_back(7); faces.push_back(4); faces.push_back(0);
    faces.push_back(0); faces.push_back(1); faces.push_back(2); faces.push_back(3);
    
    for (int i = 0; i < 6; i++)
    {
        loop_start.push_back(i*4);
        loop_total.push_back(4);
    }
    
    return bm;
}

BoundingMesh::BoundingMesh() 
{
}

BoundingMesh::~BoundingMesh() 
{
}

uint8_t*
BoundingMesh::serialize(uint32_t &size) const
{
    size = 
        4*sizeof(uint32_t)
        + vertices.size()*sizeof(float)    
        + edges.size()*sizeof(uint32_t)  
        + faces.size()*sizeof(uint32_t)
        + loop_start.size()*sizeof(uint32_t)
        + loop_total.size()*sizeof(uint32_t)
        ;
    
    uint8_t *buffer = new uint8_t[size];
    
    uint32_t *i;
    float *f;
    
    i = (uint32_t*)buffer;    
    
    *i++ = vertices.size();
    *i++ = edges.size();
    *i++ = faces.size();
    *i++ = loop_start.size();   // loop_total has same length
    
    f = (float*)i;
    
    for (const float &v : vertices)
        *f++ = v;
        
    i = (uint32_t*)f;
    
    for (const uint32_t &vi : edges)
        *i++ = vi;
    
    for (const uint32_t &vi : faces)
        *i++ = vi;
    
    for (const uint32_t &vi : loop_start)
        *i++ = vi;
    
    for (const uint32_t &vi : loop_total)
        *i++ = vi;
    
    return buffer;    
}

BoundingMesh*
BoundingMesh::deserialize(const uint8_t *buffer, uint32_t size)
{
    // XXX actually check against the size value
    
    BoundingMesh *bm = new BoundingMesh;
    
    uint32_t *i, n;
    float *f;

    i = (uint32_t*)buffer;
    
    uint32_t    vertices_len = *i++;
    uint32_t    edges_len = *i++;
    uint32_t    faces_len = *i++;
    uint32_t    loop_len = *i++;
    
    f = (float*)i;
    
    // Vertices
    n = vertices_len;
    std::vector<float> &vertices = bm->vertices;
    while (n > 0)
    {
        vertices.push_back(*f++);
        n--;
    }

    // Edges
    n = edges_len;
    std::vector<uint32_t> &edges = bm->edges;
    while (n > 0)
    {
        edges.push_back(*i++);
        n--;
    }

    // Faces
    n = faces_len;
    std::vector<uint32_t> &faces = bm->faces;
    while (n > 0)
    {
        faces.push_back(*i++);
        n--;
    }
    
    // Loop start
    n = loop_len;
    std::vector<uint32_t> &loop_start = bm->loop_start;
    while (n > 0)
    {
        loop_start.push_back(*i++);
        n--;
    }    

    // Loop total
    n = loop_len;
    std::vector<uint32_t> &loop_total = bm->loop_total;
    while (n > 0)
    {
        loop_total.push_back(*i++);
        n--;
    }    
    
    return bm;
}

