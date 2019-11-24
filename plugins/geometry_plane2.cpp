#include "plugin2.h"

static OSPGeometry
create_plane(float cx, float cy, float cz, float sx, float sy)
{
    uint32_t  num_vertices, num_triangles;
    float     *vertices, *colors;  
    uint32_t  *triangles;    
    
    num_vertices = 4;
    num_triangles = 2;
    
    vertices = new float[num_vertices*3];
    triangles = new uint32_t[num_triangles*3];
    colors = new float[num_vertices*4];
    
    vertices[0] = cx - 0.5f*sx;
    vertices[1] = cy - 0.5f*sy;
    vertices[2] = cz;

    vertices[3] = cx + 0.5f*sx;
    vertices[4] = cy - 0.5f*sy;
    vertices[5] = cz;

    vertices[6] = cx + 0.5f*sx;
    vertices[7] = cy + 0.5f*sy;
    vertices[8] = cz;

    vertices[9] = cx - 0.5f*sx;
    vertices[10] = cy + 0.5f*sy;
    vertices[11] = cz;
    
    triangles[0] = 0;
    triangles[1] = 1;
    triangles[2] = 2;

    triangles[3] = 0;
    triangles[4] = 2;
    triangles[5] = 3;

    for (int i = 0; i < num_vertices; i++)
    {
        colors[4*i+0] = 0.5f;
        colors[4*i+1] = 0.5f;
        colors[4*i+2] = 0.5f;
        colors[4*i+3] = 1.0f;
    }    
    
    OSPGeometry mesh = ospNewGeometry("triangles");
  
        OSPData data = ospNewCopiedData(num_vertices, OSP_VEC3F, vertices);   
        ospCommit(data);
        ospSetObject(mesh, "vertex.position", data);

        data = ospNewCopiedData(num_vertices, OSP_VEC4F, colors);
        ospCommit(data);
        ospSetObject(mesh, "vertex.color", data);

        data = ospNewCopiedData(num_triangles, OSP_VEC3UI, triangles);            
        ospCommit(data);
        ospSetObject(mesh, "index", data);

    ospCommit(mesh);
    
    delete [] vertices;
    delete [] colors;
    delete [] triangles;    
    
    return mesh;
}

class PlaneDefinition : public PluginDefinition
{
public:
    void initialize()
    {
        uses_renderer_type = false;
        
        add_parameter("size_x", PARAM_FLOAT, 1, FLAG_NONE, "Size in X");
        add_parameter("size_y", PARAM_FLOAT, 1, FLAG_NONE, "Size in Y");
    }
};

class PlanePlugin: public GeometryPlugin
{
public:
    bool create(const json& parameters) override;
    bool update(const json& parameters/*, changes*/) override;   // renderer type changed?
};

bool 
PlanePlugin::create(const json& parameters)
{
    const float size_x = parameters["size_x"];
    const float size_y = parameters["size_y"];
    
    OSPGeometry geometry = create_plane(0.0f, 0.0f, 0.0f, size_x, size_y); 
    
    BoundingMesh *bound = BoundingMesh::bbox(
        -0.5f*size_x, -0.5f*size_y, -1e-3f,
        0.5f*size_x, 0.5f*size_y, 1e-3f,
        true);
    
    set_geometry(geometry);
    set_bound(bound);
    
    return true;
}

bool 
PlanePlugin::update(const json& parameters)
{
    return false;
}

BLOSPRAY_REGISTER_PLUGIN(plane, PlaneDefinition, PlanePlugin)
