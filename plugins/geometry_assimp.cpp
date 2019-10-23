// ======================================================================== //
// BLOSPRAY - OSPRay as a Blender render engine                             //
// Paul Melis, SURFsara <paul.melis@surfsara.nl>                            //
// Plugin API                                                               //
// ======================================================================== //
// Copyright 2018-2019 SURFsara                                             //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h> 
#include <assimp/scene.h> 
#include <stdint.h>
#include <cstdio>
#include <vector>
#include "plugin.h"

/*
OSPGeometry
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
        colors[4*i+0] = 1.0f;
        colors[4*i+1] = 0.5f;
        colors[4*i+2] = 0.5f;
        colors[4*i+3] = 1.0f;
    }    
    
    OSPGeometry mesh = ospNewGeometry("triangles");
  
        OSPData data = ospNewCopiedData(num_vertices, OSP_VEC3F, vertices);   
        ospCommit(data);
        ospSetData(mesh, "vertex.position", data);

        data = ospNewCopiedData(num_vertices, OSP_VEC4F, colors);
        ospCommit(data);
        //ospSetData(mesh, "vertex.color", data);

        data = ospNewCopiedData(num_triangles, OSP_VEC3UI, triangles);            
        ospCommit(data);
        ospSetData(mesh, "index", data);

    ospCommit(mesh);
    
    delete [] vertices;
    delete [] colors;
    delete [] triangles;    
    
    return mesh;
}
*/

extern "C"
void
load_file(GenerateFunctionResult &result, PluginState *state)
{
    const std::string& file = state->parameters["file"];
    char        msg[1024];

    printf("... Loading %s\n", file.c_str());

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(file.c_str(), aiProcess_Triangulate);

    if (!scene)
    {
        sprintf(msg, "Assimp could not open file '%s': %s", file.c_str(), importer.GetErrorString());
        result.set_success(false);
        result.set_message(msg);
        return;
    }

    if (scene->mNumMeshes == 0)
    {
        result.set_success(false);
        result.set_message("WARNING: no meshes found in scene!\n");
        return;
    }

    if (scene->mNumMeshes > 1)
        printf("WARNING: scene contains %d meshes, only using first!\n", scene->mNumMeshes);

    aiMesh *mesh = scene->mMeshes[0];

    if (!mesh->HasPositions())
    {
        result.set_success(false);
        result.set_message("WARNING: mesh does not have position data");
        return;
    }

    const int&  nvertices = mesh->mNumVertices;
    float       min[3] = {1e6, 1e6, 1e6}, max[3] = {-1e6, -1e6, -1e6};
    float       x, y, z;

    std::vector<float> vertices;
    std::vector<uint32_t> triangles;

    // Triangle mesh
    OSPGeometry geometry = ospNewGeometry("triangles");
    {
        printf("... %d vertices\n", nvertices);

        // Vertices
        auto& mv = mesh->mVertices;

        for (int i = 0; i < nvertices; i++)
        {
            x = mv[i].x;
            y = mv[i].y;
            z = mv[i].z;

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            min[0] = std::min(min[0], x);
            min[1] = std::min(min[1], y);
            min[2] = std::min(min[2], z);

            max[0] = std::max(max[0], x);
            max[1] = std::max(max[1], y);
            max[2] = std::max(max[2], z);
        }

        OSPData data = ospNewCopiedData(nvertices, OSP_VEC3F, vertices.data());
        ospCommit(data);
        ospSetObject(geometry, "vertex.position", data);
        //ospRelease(data);
    }

    if (mesh->HasFaces())
    {
        printf("... %d triangles\n",  mesh->mNumFaces);

        // Triangles
        auto& ff = mesh->mFaces;

        for (int i = 0; i < mesh->mNumFaces; i++)
        {
            triangles.push_back(ff[i].mIndices[0]);
            triangles.push_back(ff[i].mIndices[1]);
            triangles.push_back(ff[i].mIndices[2]);
        }

        // XXX check /3
        OSPData data = ospNewCopiedData(triangles.size()/3, OSP_VEC3UI, triangles.data());
        ospCommit(data);
        ospSetObject(geometry, "index", data);
        //ospRelease(data);
    }
    else
        printf("... Mesh has no faces?\n");

    if (mesh->HasVertexColors(0))
    {
        printf("... Mesh has vertex colors\n");

        std::vector<float>  colors;
        auto& vc = mesh->mColors[0];

        for (int i = 0; i < nvertices; i++)
        {
            colors.push_back(vc[i].r);
            colors.push_back(vc[i].g);
            colors.push_back(vc[i].b);
            colors.push_back(vc[i].a);
        }

        OSPData data = ospNewCopiedData(nvertices, OSP_VEC4F, colors.data());
        ospCommit(data);
        ospSetObject(geometry, "vertex.color", data);
    }

    if (mesh->HasNormals())
    {
        printf("... Mesh has normals\n");

        std::vector<float>  normals;
        auto& nn = mesh->mNormals;

        for (int i = 0; i < nvertices; i++)
        {
            normals.push_back(nn[i].x);
            normals.push_back(nn[i].y);
            normals.push_back(nn[i].z);
        }

        OSPData data = ospNewCopiedData(nvertices, OSP_VEC3F, normals.data());
        ospCommit(data);
        ospSetObject(geometry, "vertex.normal", data);
    }

    if (mesh->HasTextureCoords(0))
    {
        printf("... Mesh has texture coordinates\n");

        std::vector<float>  texcoords;
        auto& tc = mesh->mTextureCoords[0];

        for (int i = 0; i < nvertices; i++)
        {
            texcoords.push_back(tc[i].x);
            texcoords.push_back(tc[i].y);
            texcoords.push_back(tc[i].z);
        }

        OSPData data = ospNewCopiedData(nvertices, OSP_VEC2F, texcoords.data());
        ospCommit(data);
        ospSetObject(geometry, "vertex.texcoord", data); 
    }
    
    ospCommit(geometry);

    /*
    {
        // XXX opt
        geometry = ospNewGeometry("subdivision");

            //ospSetFloat(geometry, "level", 1.0f);

            OSPData data = ospNewCopiedData(nvertices, OSP_VEC3F, vertices.data());
            ospCommit(data);
            ospSetObject(geometry, "vertex.position", data);

            //data = ospNewCopiedData(num_vertices, OSP_VEC4F, colors);
            //ospCommit(data);
            //ospSetObject(mesh, "vertex.color", data);

            // XXX check /3
            data = ospNewCopiedData(faces.size()/3, OSP_VEC3UI, faces.data());
            ospCommit(data);
            ospSetObject(geometry, "index", data);

            data = ospNewCopiedData(face_lengths.size(), OSP_UINT, face_lengths.data());
            ospCommit(data);
            ospSetObject(geometry, "face", data);

        ospCommit(geometry);
    }


    */

    state->geometry = geometry;

    state->bound = BoundingMesh::simplify_qc(
        vertices.data(), vertices.size()/3,
        triangles.data(), triangles.size()/3, 
        10
        );
}

static PluginParameters 
parameters = {
    
    {"file",          PARAM_STRING,     1, FLAG_NONE, "Geometry file to load"},
    //{"divisions",     PARAM_INT,        1, FLAG_NONE, "Divisions (for QC simplification)"},
        
    PARAMETERS_DONE         // Sentinel (signals end of list)
};

static PluginFunctions
functions = {

    NULL,               // Plugin load
    NULL,               // Plugin unload
    
    load_file,          // Generate    
    NULL,               // Clear data
};


extern "C" bool
initialize(PluginDefinition *def)
{
    def->type = PT_GEOMETRY;
    def->uses_renderer_type = false;
    def->parameters = parameters;
    def->functions = functions;
    
    // Do any other plugin-specific initialization here
    
    return true;
}


