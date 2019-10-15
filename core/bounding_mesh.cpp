#include "bounding_mesh.h"
#include "config.h"

#ifdef VTK_QC_BOUND
#include <vtkPolyData.h>
#include <vtkQuadricClustering.h>
#include <vtkSmartPointer.h>
#include <vtkTriangleFilter.h>
#endif

BoundingMesh*
BoundingMesh::bbox(float xmin, float ymin, float zmin, float xmax, float ymax, float zmax, bool edges_only)
{        
    BoundingMesh *bm = new BoundingMesh;
    
    std::vector<float>      &vertices = bm->vertices;
    std::vector<uint32_t>   &edges = bm->edges;
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
    
    if (edges_only)
    {    
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
    }
    else
    {
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
    }
    
    return bm;
}    

BoundingMesh*
BoundingMesh::bbox_from_group(OSPGroup group, bool edges_only)
{
    OSPBounds bbox = ospGetBounds(group);
   
    return BoundingMesh::bbox(
        bbox.lower[0], bbox.lower[1], bbox.lower[2],
        bbox.upper[0], bbox.upper[1], bbox.upper[2],
        edges_only
    );
}

BoundingMesh*
BoundingMesh::bbox_from_instance(OSPInstance instance, bool edges_only)
{
    OSPBounds bbox = ospGetBounds(instance);
   
    return BoundingMesh::bbox(
        bbox.lower[0], bbox.lower[1], bbox.lower[2],
        bbox.upper[0], bbox.upper[1], bbox.upper[2],
        edges_only
    );
}

// https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification
// https://lorensen.github.io/VTKExamples/site/Cxx/Meshes/QuadricDecimation/
// https://vtk.org/doc/nightly/html/classvtkQuadricClustering.html

BoundingMesh*
BoundingMesh::simplify_qc(const float *vertices, int num_vertices, const uint32_t *triangles, int num_triangles, int divisions)
{
#ifdef VTK_QC_BOUND
    vtkSmartPointer<vtkPolyData> polydata = vtkSmartPointer<vtkPolyData>::New();
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkCellArray> polys = vtkSmartPointer<vtkCellArray>::New();
    vtkIdType triangle[3];

    for (int i = 0; i < num_vertices; i++)  
        points->InsertNextPoint(vertices[3*i], vertices[3*i+1], vertices[3*i+2]);

    for (int i = 0; i < num_triangles; i++)
    {
        triangle[0] = triangles[3*i+0];
        triangle[1] = triangles[3*i+1];
        triangle[2] = triangles[3*i+2];
        polys->InsertNextCell(3, triangle);
    }

    polydata->SetPoints(points);
    polydata->SetPolys(polys);

    vtkSmartPointer<vtkQuadricClustering> decimate = vtkSmartPointer<vtkQuadricClustering>::New();
    decimate->SetInputData(polydata);
    decimate->SetNumberOfDivisions(divisions, divisions, divisions);
    decimate->AutoAdjustNumberOfDivisionsOn();
    //decimate->UseFeatureEdgesOn();

    vtkSmartPointer<vtkTriangleFilter> trifilter = vtkSmartPointer<vtkTriangleFilter>::New();
    trifilter->SetInputConnection(decimate->GetOutputPort());
    trifilter->Update();

    vtkSmartPointer<vtkPolyData> decimated = vtkSmartPointer<vtkPolyData>::New();
    decimated->ShallowCopy(trifilter->GetOutput());
    printf("... Decimated (QC): %d vertices, %d polygons\n", decimated->GetNumberOfPoints(), decimated->GetNumberOfPolys());

    points = decimated->GetPoints();
    polys = decimated->GetPolys();

    BoundingMesh *bm = new BoundingMesh;
    
    std::vector<float>      &bm_vertices = bm->vertices;
    //std::vector<uint32_t>   &edges = bm->edges;
    std::vector<uint32_t>   &faces = bm->faces;
    std::vector<uint32_t>   &loop_start = bm->loop_start;
    std::vector<uint32_t>   &loop_total = bm->loop_total;

    for (int i = 0; i < points->GetNumberOfPoints(); i++)
    {
        const double *p = points->GetPoint(i);
        bm_vertices.push_back(p[0]);
        bm_vertices.push_back(p[1]);
        bm_vertices.push_back(p[2]);
    }

    vtkIdType npts, *pts; 
    int i = 0;
    polys->InitTraversal();
    while (polys->GetNextCell(npts, pts))
    {
        assert(npts == 3);

        faces.push_back(pts[0]);
        faces.push_back(pts[1]);
        faces.push_back(pts[2]);

        loop_start.push_back(i);
        loop_total.push_back(3);

        i += 3;
    }
 
    return bm;
#else
    // VTK not available, return regular AABB
    float   min[3] = {1e6, 1e6, 1e6}, max[3] = {-1e6, -1e6, -1e6};
    float   x, y, z;

    for (int i = 0; i < num_vertices; i++)
    {
        x = vertices[3*i+0];
        y = vertices[3*i+1];
        z = vertices[3*i+2];

        min[0] = std::min(min[0], x);
        min[1] = std::min(min[1], y);
        min[2] = std::min(min[2], z);

        max[0] = std::max(max[0], x);
        max[1] = std::max(max[1], y);
        max[2] = std::max(max[2], z);
    }

    return BoundingMesh::bbox(min[0], min[1], min[2], max[0], max[1], max[2], true);
#endif
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

