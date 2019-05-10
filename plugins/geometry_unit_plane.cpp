// A plane of 1x1 units in X and Y, centered at the origin (for testing)
#include <cstdio>
#include <stdint.h>
#include <ospray/ospray.h>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "json.hpp"
#include "util.h"
#include "plugin.h"

using json = nlohmann::json;

OSPGeometry
add_plane(float cx, float cy, float cz, float sx, float sy)
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
  
      OSPData data = ospNewData(num_vertices, OSP_FLOAT3, vertices);   
      ospCommit(data);
      ospSetData(mesh, "vertex", data);

      data = ospNewData(num_vertices, OSP_FLOAT4, colors);
      ospCommit(data);
      ospSetData(mesh, "vertex.color", data);

      data = ospNewData(num_triangles, OSP_INT3, triangles);            
      ospCommit(data);
      ospSetData(mesh, "index", data);

    ospCommit(mesh);
    
    delete [] vertices;
    delete [] colors;
    delete [] triangles;    
    
    return mesh;
}

extern "C" 
void
load(ModelInstances& model_instances, float *bbox, LoadFunctionResult &result, const json &parameters, const glm::mat4& object2world)
{    
    OSPGeometry plane_geom = add_plane(0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    
    OSPModel model = ospNewModel();
        ospAddGeometry(model, plane_geom);
    ospCommit(model);
    
    // Add instance
    model_instances.push_back(std::make_pair(model, glm::mat4(1.0f)));
    
    printf("Data loaded...\n");

    // XXX too large
    bbox[0] = -1.0f;
    bbox[1] = -1.0f;
    bbox[2] = -1.0f;
    
    bbox[3] = 1.0f;
    bbox[4] = 1.0f;
    bbox[5] = 1.0f;
}

PluginFunctions    
functions = {

    // Volume
    NULL,
    NULL,

    // Geometry
    load
};

