// Need to set RBC_DATA_PATH for the server process before the plugin is used
// XXX turn into plugin parameter
#include <cstdio>
#include <stdint.h>
#include <ospray/ospray.h>
#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "util.h"       // XXX for ...?
#include "plugin.h"

using json = nlohmann::json;

const char      *rbc_data_path;
OSPModel        mesh_model_rbc, mesh_model_plt;

bool
load_cell_models()
{
    char fname[1024];
    
    uint32_t  num_vertices, num_triangles;
    float     *vertices, *colors;  
    uint32_t  *triangles;
    
    // Read RBC geometry
    
    sprintf(fname, "%s/rbc_normal_translated.bin", rbc_data_path);
    FILE *f = fopen(fname, "rb");
    if (!f)
        return false;

    fread(&num_vertices, 4, 1, f);
    fread(&num_triangles, 4, 1, f);
    
    printf("RBC: %d vertices, %d triangles\n", num_vertices, num_triangles);

    vertices = new float[3*num_vertices];    
    triangles = new uint32_t[3*num_triangles];

    fread(vertices, num_vertices*3*sizeof(float), 1, f);
    fread(triangles, num_triangles*3*sizeof(uint32_t), 1, f);

    fclose(f);    
    
    // Set vertex colors
    
    colors = new float[4*num_vertices];
    
    for (int i = 0; i < num_vertices; i++)
    {
        colors[4*i+0] = 1.0f;   //187
        colors[4*i+1] = 0.0f;   //96
        colors[4*i+2] = 0.0f;   //96
        colors[4*i+3] = 1.0f;
    }
        
    // Create mesh
    
    OSPGeometry mesh = ospNewGeometry("triangles");
    
      // XXX use ospRelease() here?
  
      OSPData data = ospNewData(num_vertices, OSP_FLOAT3, vertices);    // OSP_FLOAT3A format is also supported for vertex positions
      ospCommit(data);
      ospSetData(mesh, "vertex", data);

      data = ospNewData(num_vertices, OSP_FLOAT4, colors);
      ospCommit(data);
      ospSetData(mesh, "vertex.color", data);

      // XXX are aligned indices, i.e. OSP_INT4, faster to render?
      // YYY they are not really aligned it seems, merely 4 elements
      // stored for a 3-element vector 
      data = ospNewData(num_triangles, OSP_INT3, triangles);            // OSP_INT4 format is also supported for triangle indices
      ospCommit(data);
      ospSetData(mesh, "index", data);
            
      OSPMaterial material = ospNewMaterial2("scivis", "OBJMaterial");
    
      ospSet3f(material, "Kd", 0.8f, 0, 0);
      ospCommit(material);
      ospSetMaterial(mesh, material);
      ospRelease(material);

    ospCommit(mesh);
  
    // Create model (for instancing)
    
    mesh_model_rbc = ospNewModel();
        ospAddGeometry(mesh_model_rbc, mesh);
    ospCommit(mesh_model_rbc);
    
    delete [] vertices;
    delete [] triangles;
    delete [] colors;
    
    // Read PLT geometry

    sprintf(fname, "%s/plt_normal_translated.bin", rbc_data_path);
    f = fopen(fname, "rb");
    if (!f)
        return false;

    fread(&num_vertices, 4, 1, f);
    fread(&num_triangles, 4, 1, f);
    
    printf("PLT: %d vertices, %d triangles\n", num_vertices, num_triangles);

    vertices = new float[3*num_vertices];    
    triangles = new uint32_t[3*num_triangles];

    fread(vertices, num_vertices*3*sizeof(float), 1, f);
    fread(triangles, num_triangles*3*sizeof(uint32_t), 1, f);

    fclose(f);    
    
    // Set vertex colors
    
    colors = new float[4*num_vertices];
    
    for (int i = 0; i < num_vertices; i++)
    {
        colors[4*i+0] = 230 / 255.0f;
        colors[4*i+1] = 230 / 255.0f;
        colors[4*i+2] = 110 / 255.0f;
        colors[4*i+3] = 1.0f;
    }
        
    // Create mesh
    
    mesh = ospNewGeometry("triangles");
    
      // XXX use ospRelease() here?
  
      data = ospNewData(num_vertices, OSP_FLOAT3, vertices);    // OSP_FLOAT3A format is also supported for vertex positions
      ospCommit(data);
      ospSetData(mesh, "vertex", data);

      data = ospNewData(num_vertices, OSP_FLOAT4, colors);
      ospCommit(data);
      ospSetData(mesh, "vertex.color", data);

      // XXX are aligned indices, i.e. OSP_INT4, faster to render?
      data = ospNewData(num_triangles, OSP_INT3, triangles);            // OSP_INT4 format is also supported for triangle indices
      ospCommit(data);
      ospSetData(mesh, "index", data);
    
      material = ospNewMaterial2("scivis", "OBJMaterial");
    
      ospSet3f(material, "Kd", 0.8f, 0.8f, 0.8f);
      ospCommit(material);
      ospSetMaterial(mesh, material);
      ospRelease(material);

    ospCommit(mesh);
  
    // Create model (for instancing)
    
    mesh_model_plt = ospNewModel();
        ospAddGeometry(mesh_model_plt, mesh);
    ospCommit(mesh_model_plt);
    
    return true;
}

/*
void
add_ground_plane()
{
    uint32_t  num_vertices, num_triangles;
    float     *vertices, *colors;  
    uint32_t  *triangles;    
    
    num_vertices = 4;
    num_triangles = 2;
    
    vertices = new float[num_vertices*3];
    triangles = new uint32_t[num_triangles*3];
    colors = new float[num_vertices*4];
    
    const float M = 2000.0f;
    const float z = -100.0f;
    
    vertices[0] = 0.0f - M;
    vertices[1] = 0.0f - M;
    vertices[2] = z;

    vertices[3] = 2000.0f + M;
    vertices[4] = 0.0f - M;
    vertices[5] = z;

    vertices[6] = 2000.0f + M;
    vertices[7] = 1000.0f + M;
    vertices[8] = z;

    vertices[9] = 0.0f - M;
    vertices[10] = 1000.0f + M;
    vertices[11] = z;
    
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
    
    OSPGeometry mesh2 = ospNewGeometry("triangles");
  
      OSPData data = ospNewData(num_vertices, OSP_FLOAT3, vertices);   
      ospCommit(data);
      ospSetData(mesh2, "vertex", data);

      data = ospNewData(num_vertices, OSP_FLOAT4, colors);
      ospCommit(data);
      ospSetData(mesh2, "vertex.color", data);

      data = ospNewData(num_triangles, OSP_INT3, triangles);            
      ospCommit(data);
      ospSetData(mesh2, "index", data);

    ospCommit(mesh2);
    
    ospAddGeometry(world, mesh2);    
    
    delete [] vertices;
    delete [] triangles;    
}
*/

extern "C" 
void
load(ModelInstances& model_instances, float *bbox, LoadFunctionResult &result, const json &parameters, const glm::mat4& object2world)
{    
    rbc_data_path = getenv("RBC_DATA_PATH");
    if (!rbc_data_path)
    {
        fprintf(stderr, "ERROR: RBC_DATA_PATH not set!\n");
        result.set_success(false);
        result.set_message("RBC_DATA_PATH not set!");
        return;
    }
    
    int max_rbcs = -1;
    int max_plts = -1;
    
    if (parameters.find("num_rbcs") != parameters.end())
        max_rbcs = parameters["num_rbcs"].get<int>();
    if (parameters.find("num_plts") != parameters.end())
        max_plts = parameters["num_plts"].get<int>();
    
    if (!load_cell_models())
    {
        result.set_success(false);
        result.set_message("Failed to load cell models");
        return;
    }
    
    uint32_t    num_rbc, num_plt, num_wbc;
    float       tx, ty, tz, rx, ry, rz;
    glm::mat4   R;

    char fname[1024];
    sprintf(fname, "%s/cells.bin", rbc_data_path);
    FILE *p = fopen(fname, "rb");
    
    if (!p)
    {
        fprintf(stderr, "ERROR: could not open cells.bin!\n");
        result.set_success(false);
        result.set_message("could not open cells.bin");
        return;
    }
    
    fread(&num_rbc, sizeof(uint32_t), 1, p);
    fread(&num_plt, sizeof(uint32_t), 1, p);
    fread(&num_wbc, sizeof(uint32_t), 1, p);
    printf("On-disk scene: %d rbc, %d plt, %d wbc\n", num_rbc, num_plt, num_wbc);

    // Instantiate RBCs & PLTs
    
    if (max_rbcs == -1)
        max_rbcs = num_rbc;
    
    printf("Adding %d RBCs\n", max_rbcs);    
      
    for (int i = 0; i < max_rbcs; i++)
    {      
        fread(&tx, sizeof(float), 1, p);
        fread(&ty, sizeof(float), 1, p);
        fread(&tz, sizeof(float), 1, p);
        fread(&rx, sizeof(float), 1, p);
        fread(&ry, sizeof(float), 1, p);
        fread(&rz, sizeof(float), 1, p);
        
        R = glm::mat4(1.0f);
        R = glm::translate(R, glm::vec3(tx,ty,tz));        
        R = glm::rotate(R, glm::radians(rx), glm::vec3(1,0,0));
        R = glm::rotate(R, glm::radians(ry), glm::vec3(0,1,0));
        R = glm::rotate(R, glm::radians(rz), glm::vec3(0,0,1));   
        
        // Add instance
        model_instances.push_back(std::make_pair(mesh_model_rbc, R));
    }    
    
    // Skip remaining RBCs in scene file
    
    for (int i = 0; i < num_rbc-max_rbcs; i++)
    {      
        fread(&tx, sizeof(float), 1, p);
        fread(&ty, sizeof(float), 1, p);
        fread(&tz, sizeof(float), 1, p);
        fread(&rx, sizeof(float), 1, p);
        fread(&ry, sizeof(float), 1, p);
        fread(&rz, sizeof(float), 1, p);
    }
    
    if (max_plts == -1)
        max_plts = num_plt;
    
    printf("Adding %d PLTs\n", max_plts);
      
    for (int i = 0; i < max_plts; i++)
    {      
        fread(&tx, sizeof(float), 1, p);
        fread(&ty, sizeof(float), 1, p);
        fread(&tz, sizeof(float), 1, p);
        fread(&rx, sizeof(float), 1, p);
        fread(&ry, sizeof(float), 1, p);
        fread(&rz, sizeof(float), 1, p);

        R = glm::mat4(1.0f);
        R = glm::translate(R, glm::vec3(tx,ty,tz));
        R = glm::rotate(R, glm::radians(rx), glm::vec3(1,0,0));
        R = glm::rotate(R, glm::radians(ry), glm::vec3(0,1,0));
        R = glm::rotate(R, glm::radians(rz), glm::vec3(0,0,1));
        
        /*glm::mat4 T = glm::translate(
        glm::mat4(1.0f), glm::vec3(tx, ty, tz)
        );*/
        
        // Add instance
        model_instances.push_back(std::make_pair(mesh_model_plt, R));
    }        
    
    fclose(p);        
    
    printf("Data loaded...\n");

    bbox[0] = 0.0f;
    bbox[1] = 0.0f;
    bbox[2] = 0.0f;
    
    bbox[3] = 2000.0f;
    bbox[4] = 1000.0f;
    bbox[5] = 1000.0f;
}

PluginFunctions    
functions = {

    NULL,
    NULL,

    load
};

