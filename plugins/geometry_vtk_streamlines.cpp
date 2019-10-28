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

#include <stdint.h>
#include <cstdio>
#include <vector>
#include "config.h"
#include "plugin.h"

#ifdef PLUGIN_VTK_STREAMLINES // XXX
#include <vtkPolyData.h>
#include <vtkDataSetReader.h>
#include <vtkSmartPointer.h>
#endif

extern "C"
void
load_file(GenerateFunctionResult &result, PluginState *state)
{
    const json& parameters = state->parameters;

    const std::string& file = parameters["file"];
    char        msg[1024];

    printf("... Loading %s\n", file.c_str());

    vtkSmartPointer<vtkDataSetReader> reader = vtkSmartPointer<vtkDataSetReader>::New();
    reader->SetFileName(file.c_str());
    reader->Update();

    /*
    if (!scene)
    {
        sprintf(msg, "Assimp could not open file '%s': %s", file.c_str(), importer.GetErrorString());
        result.set_success(false);
        result.set_message(msg);
        return;
    }
    */

    vtkSmartPointer<vtkDataSet> dataset = reader->GetOutput();

    const vtkIdType np = dataset->GetNumberOfPoints();
    const vtkIdType nc = dataset->GetNumberOfCells();

    printf("... %d points\n", np);
    printf("... %d cells\n", nc);

    std::vector<float>      positions;
    std::vector<uint32_t>   indices;
    uint32_t    curindex = 0;

    for (vtkIdType i = 0; i < nc; i++)
    {
        vtkCell *cell = dataset->GetCell(i);

        if (cell->GetCellType() != VTK_LINE)
        {
            printf("Skipping cell %d, of non-line type %d\n", i, cell->GetCellType());
            continue;
        }

        if (cell->GetNumberOfPoints() != 2)
        {
            printf("Skipping cell %d which has %d (!= 2) points\n", i, cell->GetNumberOfPoints());
            continue;
        }

        vtkIdType pi = cell->GetPointId(0);
        vtkIdType qi = cell->GetPointId(1);

        const double *p = dataset->GetPoint(pi);
        const double *q = dataset->GetPoint(qi);        

        positions.push_back(p[0]);
        positions.push_back(p[1]);
        positions.push_back(p[2]);

        positions.push_back(q[0]);
        positions.push_back(q[1]);
        positions.push_back(q[2]);

        indices.push_back(curindex);

        curindex += 2;
    }

    OSPGeometry geometry = ospNewGeometry("streamlines");
    {
        printf("... %d positions\n", positions.size()/3);
        printf("... %d indices\n", indices.size());

        OSPData data = ospNewCopiedData(positions.size()/3, OSP_VEC3F, positions.data());
        ospCommit(data);
        ospSetObject(geometry, "vertex.position", data);
        ospRelease(data);

        data = ospNewCopiedData(indices.size(), OSP_UINT, indices.data());
        ospCommit(data);
        ospSetObject(geometry, "index", data);
        ospRelease(data);
    }

    float radius = 1.0f;
    if (parameters.find("radius") != parameters.end())
        radius = parameters["radius"];

    ospSetFloat(geometry, "radius", radius);

    ospSetBool(geometry, "smooth", false);

#if 0
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
    
#endif    

    ospCommit(geometry);

    state->geometry = geometry;

    const double *bbox = dataset->GetBounds();

    state->bound = BoundingMesh::bbox(
        bbox[0], bbox[2], bbox[4],
        bbox[1], bbox[3], bbox[5],
        true
        );    
}

static PluginParameters 
parameters = {
    
    {"file",        PARAM_STRING,   1, FLAG_NONE, "VTK file to load"},
    {"radius",      PARAM_FLOAT,    1, FLAG_NONE, "Radius"},
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


