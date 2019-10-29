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
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkPointData.h>
#include <vtkDataSetReader.h>
#include <vtkColorTransferFunction.h>
#endif

extern "C"
void
load_file(GenerateFunctionResult &result, PluginState *state)
{
    //char        msg[1024];

    const json& parameters = state->parameters;
    const std::string& file = parameters["file"];

    float radius = 1.0f;
    if (parameters.find("radius") != parameters.end())
        radius = parameters["radius"];    

    vtkSmartPointer<vtkColorTransferFunction> cool2warm;
    bool        color_by_scalars = false;
    std::string scalars;
    float       scalar_range[2];
    float       scalar_value_range;

    scalar_range[0] = 0.0f;
    scalar_range[1] = 1.0f;
    if (parameters.find("scalar_range") != parameters.end())
    {
        auto& p_scalar_range = parameters["scalar_range"];
        scalar_range[0] = p_scalar_range[0];
        scalar_range[1] = p_scalar_range[1];        
    }
    scalar_value_range = scalar_range[1] - scalar_range[0];
    
    if (parameters.find("scalars") != parameters.end())
    {
        scalars = parameters["scalars"];        

        cool2warm = vtkSmartPointer<vtkColorTransferFunction>::New();
        cool2warm->SetColorSpaceToDiverging();
        cool2warm->AddRGBPoint(0.0, 0.230, 0.299, 0.754);
        cool2warm->AddRGBPoint(1.0, 0.706, 0.016, 0.150);

        color_by_scalars = true;
        printf("... Using scalar data range %.6f, %.6f\n", scalar_range[0], scalar_range[1]);
    }

    printf("... Loading %s\n", file.c_str());

    vtkSmartPointer<vtkDataSetReader> reader = vtkSmartPointer<vtkDataSetReader>::New();
    reader->SetFileName(file.c_str());
    reader->Update();
    // XXX check file was read correctly

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
    vtkSmartPointer<vtkDataArray> scalar_data;

    if (color_by_scalars)
    {
        vtkSmartPointer<vtkPointData> point_data = dataset->GetPointData();
        point_data->SetActiveScalars(scalars.c_str());
        scalar_data = point_data->GetScalars();
        if (scalar_data == nullptr)
        {
            printf("... WARNING: could not find point data scalar array '%s'!\n", scalars.c_str());
            color_by_scalars = false;
        }
    }

    const vtkIdType np = dataset->GetNumberOfPoints();
    const vtkIdType nc = dataset->GetNumberOfCells();

    printf("... %d points\n", np);
    printf("... %d cells\n", nc);

    std::vector<float>      positions;
    std::vector<uint32_t>   indices;
    std::vector<float>      vcolors;
    uint32_t    curindex = 0;
    double  p[3], *col;
    float v, f;

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
        
        dataset->GetPoint(pi, p);
        positions.push_back(p[0]);
        positions.push_back(p[1]);
        positions.push_back(p[2]);

        dataset->GetPoint(qi, p);
        positions.push_back(p[0]);
        positions.push_back(p[1]);
        positions.push_back(p[2]);

        indices.push_back(curindex);
        
        curindex += 2;

        if (color_by_scalars)
        {
            v = (float)(scalar_data->GetTuple1(pi));
            f = (v - scalar_range[0]) / scalar_value_range;
            col = cool2warm->GetColor(f);
            vcolors.push_back(col[0]);
            vcolors.push_back(col[1]);
            vcolors.push_back(col[2]);
            vcolors.push_back(f);           // Note: alpha = normalized value

            v = (float)(scalar_data->GetTuple1(qi));
            f = (v - scalar_range[0]) / scalar_value_range;
            col = cool2warm->GetColor(f);
            vcolors.push_back(col[0]);
            vcolors.push_back(col[1]);
            vcolors.push_back(col[2]);
            vcolors.push_back(f);
        }
    }

    OSPData data;

    OSPGeometry geometry = ospNewGeometry("streamlines");
    {
        printf("... %d positions\n", positions.size()/3);
        printf("... %d indices\n", indices.size());

        data = ospNewCopiedData(positions.size()/3, OSP_VEC3F, positions.data());
        ospCommit(data);
        ospSetObject(geometry, "vertex.position", data);
        ospRelease(data);

        data = ospNewCopiedData(indices.size(), OSP_UINT, indices.data());
        ospCommit(data);
        ospSetObject(geometry, "index", data);
        ospRelease(data);
    }

    ospSetFloat(geometry, "radius", radius);
    ospSetBool(geometry, "smooth", false);

    if (color_by_scalars)
    {
        data = ospNewCopiedData(vcolors.size()/4, OSP_VEC4F, vcolors.data());
        ospCommit(data);
        ospSetObject(geometry, "vertex.color", data);
        ospRelease(data);
    }

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
    {"scalars",     PARAM_STRING,   1, FLAG_OPTIONAL, "Scalar values to show (array name)"},
    {"scalar_range", PARAM_FLOAT,   2, FLAG_OPTIONAL, "Scalar value range to use for coloring"},
        
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


