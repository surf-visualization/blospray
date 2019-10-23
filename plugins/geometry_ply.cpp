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

// Uses RPly (http://w3.impa.br/~diego/software/rply/) by Diego Nehab
#include <rply.h>
#include <stdint.h>
#include <cstdio>
#include <vector>
#include "plugin.h"

static std::vector<float> vertices;
static int      next_vertex_element_index;

static std::vector<uint32_t> faces;          // XXX vertex_indices
static std::vector<uint32_t> face_lengths;
static int      face_indices_size;
static int      next_face_offset;
static int      next_face_element_index;
static int      num_triangles, num_quads;

static std::vector<float> vertex_normals;
static int      next_vertex_normal_element_index;

static std::vector<float> vertex_colors;
static int      next_vertex_color_element_index;
static float    vertex_color_scale_factor;

static std::vector<float>    vertex_texcoords;
static int      next_vertex_texcoord_element_index;

// Vertex callbacks

static int
vertex_cb(p_ply_argument argument)
{
    vertices.push_back(ply_get_argument_value(argument));
    next_vertex_element_index++;

    return 1;
}

static int
vertex_color_cb(p_ply_argument argument)
{
    vertex_colors.push_back(ply_get_argument_value(argument) * vertex_color_scale_factor);
    next_vertex_color_element_index++;

    return 1;
}

static int
vertex_normal_cb(p_ply_argument argument)
{
    vertex_normals.push_back(ply_get_argument_value(argument));
    next_vertex_normal_element_index++;

    return 1;
}

static int
vertex_texcoord_cb(p_ply_argument argument)
{
    vertex_texcoords.push_back(ply_get_argument_value(argument));
    next_vertex_texcoord_element_index++;

    return 1;
}

// Face callback

static int
face_cb(p_ply_argument argument)
{
    long    length, value_index;
    int     vertex_index;

    ply_get_argument_property(argument, NULL, &length, &value_index);

    if (value_index == -1)
    {
        // First value of a list property, the one that gives the 
        // number of entries, i.e. start of new face
        next_face_offset++;

        face_lengths.push_back(length);
        
        return 1;
    }
    
    faces.push_back(ply_get_argument_value(argument));

    return 1;
}

extern "C"
void
load_ply_file(GenerateFunctionResult &result, PluginState *state)
{
    const std::string& plyfile = state->parameters["file"];
    char        msg[1024];
    int         vertex_values_per_loop = 1;

    p_ply ply = ply_open(plyfile.c_str(), NULL, 0, NULL);
    if (!ply)
    {
        
        sprintf(msg, "Could not open PLY file %s", plyfile.c_str());
        result.set_success(false);
        result.set_message(msg);
        printf("%s\n", msg);
        return;
    }

    if (!ply_read_header(ply))
    {
        strcpy(msg, "Could not read PLY header");
        result.set_success(false);
        result.set_message(msg);
        printf("%s\n", msg);
        return;
    }

    // Check elements

    p_ply_element   vertex_element=NULL, face_element=NULL;
    const char      *name;

    p_ply_element element = ply_get_next_element(ply, NULL);
    while (element)
    {
        ply_get_element_info(element, &name, NULL);

        if (strcmp(name, "vertex") == 0)
            vertex_element = element;
        else if (strcmp(name, "face") == 0)
            face_element = element;

        element = ply_get_next_element(ply, element);
    }

    // XXX turn into actual checks
    assert(vertex_element && "Don't have a vertex element");
    assert(face_element && "Don't have a face element");

    // Set vertex and face property callbacks
    
    long nvertices, nfaces;

    nvertices = ply_set_read_cb(ply, "vertex", "x", vertex_cb, NULL, 0);
    ply_set_read_cb(ply, "vertex", "y", vertex_cb, NULL, 0);
    ply_set_read_cb(ply, "vertex", "z", vertex_cb, NULL, 1);

    nfaces = ply_set_read_cb(ply, "face", "vertex_indices", face_cb, NULL, 0);

    //printf("%ld vertices\n%ld faces\n", nvertices, nfaces);

    // Set optional per-vertex callbacks

    int have_vertex_colors = 0;
    int have_vertex_normals = 0;
    int have_vertex_texcoords = 0;      // Either s,t or u,v sets will be used, but not both

    p_ply_property  prop;
    e_ply_type      ptype, plength_type, pvalue_type;
    
    // XXX check ply_set_read_cb() return values below

    prop = ply_get_next_property(vertex_element, NULL);
    while (prop)
    {
        ply_get_property_info(prop, &name, &ptype, &plength_type, &pvalue_type);

        //printf("property '%s'\n", name);

        if (strcmp(name, "red") == 0)
        {
            // Assumes green and blue properties are also available
            // XXX is there ever an alpha value?
            have_vertex_colors = 1;

            if (ptype == PLY_UCHAR)
                vertex_color_scale_factor = 1.0f / 255;
            else if (ptype == PLY_FLOAT)
                vertex_color_scale_factor = 1.0f;
            else
                fprintf(stderr, "Warning: vertex color value type is %d, don't know how to handle!\n", ptype);

            ply_set_read_cb(ply, "vertex", "red", vertex_color_cb, NULL, 0);
            ply_set_read_cb(ply, "vertex", "green", vertex_color_cb, NULL, 0);
            ply_set_read_cb(ply, "vertex", "blue", vertex_color_cb, NULL, 1);
        }
        else if (strcmp(name, "nx") == 0)
        {
            // Assumes ny and nz properties are also available
            have_vertex_normals = 1;

            ply_set_read_cb(ply, "vertex", "nx", vertex_normal_cb, NULL, 0);
            ply_set_read_cb(ply, "vertex", "ny", vertex_normal_cb, NULL, 0);
            ply_set_read_cb(ply, "vertex", "nz", vertex_normal_cb, NULL, 1);
        }
        else if (strcmp(name, "s") == 0 && !have_vertex_texcoords)
        {
            // Assumes t property is also available
            have_vertex_texcoords = 1;

            ply_set_read_cb(ply, "vertex", "s", vertex_texcoord_cb, NULL, 0);
            ply_set_read_cb(ply, "vertex", "t", vertex_texcoord_cb, NULL, 1);
        }
        else if (strcmp(name, "u") == 0 && !have_vertex_texcoords)
        {
            // Assumes v property is also available
            have_vertex_texcoords = 1;

            ply_set_read_cb(ply, "vertex", "u", vertex_texcoord_cb, NULL, 0);
            ply_set_read_cb(ply, "vertex", "v", vertex_texcoord_cb, NULL, 1);
        }

        prop = ply_get_next_property(vertex_element, prop);
    }

    // Allocate memory and initialize some values

    next_vertex_element_index = 0;

    // As we don't know the number of indices needed in advance we assume
    // quads. For a pure-triangle mesh this will overallocate by 1/4th,
    // but for a mesh with general n-gons we might have to reallocate
    // later on.
    
    next_face_offset = 0;
    next_face_element_index = 0;
    
    face_indices_size = nfaces > 128 ? nfaces*4 : 512;

    if (have_vertex_normals)
    {
        next_vertex_normal_element_index = 0;
    }

    if (have_vertex_colors)
    {
        next_vertex_color_element_index = 0;
    }

    if (have_vertex_texcoords)
    {
        next_vertex_texcoord_element_index = 0;
    }

    // Let rply process the file using the callbacks we set above

    num_triangles = num_quads = 0;

    if (!ply_read(ply))
    {
        // Failed!
        printf("Could not read PLY data!\n");

        ply_close(ply);

        return;
    }

    // Clean up PLY reader

    ply_close(ply);

    // Create geometry

    int min_gon=1000, max_gon=0;

    for (int l : face_lengths)
    {
        min_gon = std::min(min_gon, l);
        max_gon = std::max(max_gon, l);
    }

    printf("n-gon sizes in [%d, %d]\n", min_gon,  max_gon);

    OSPGeometry geometry;

    if (max_gon == 3)
    {
        // Triangle mesh
        geometry = ospNewGeometry("triangles");

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

        ospCommit(geometry);
    }
    else
    {
        // XXX opt
        geometry = ospNewGeometry("subdivision");

            //ospSetFloat(geometry, "level", 1.0f);

            OSPData data = ospNewCopiedData(nvertices, OSP_VEC3F, vertices.data());
            ospCommit(data);
            ospSetObject(geometry, "vertex.position", data);

            //data = ospNewCopiedData(num_vertices, OSP_VEC4F, colors);
            //ospCommit(data);
            //ospSetData(mesh, "vertex.color", data);

            // XXX check /3
            data = ospNewCopiedData(faces.size()/3, OSP_VEC3UI, faces.data());
            ospCommit(data);
            ospSetObject(geometry, "index", data);

            data = ospNewCopiedData(face_lengths.size(), OSP_UINT, face_lengths.data());
            ospCommit(data);
            ospSetObject(geometry, "face", data);

        ospCommit(geometry);
    }

    state->geometry = geometry;

    // Bounding box edges based on vertices

    float min[3] = {1e6, 1e6, 1e6}, max[3] = {-1e6, -1e6, -1e6};
    int i;

    i = 0;
    while (i < vertices.size())
    {
        for (int j = 0; j < 3; j++)
            min[j] = std::min(min[j], vertices[i+j]);
        for (int j = 0; j < 3; j++)
            max[j] = std::max(max[j], vertices[i+j]);
        i += 3;
    }

    state->bound = BoundingMesh::bbox(
        min[0], min[1], min[2], 
        max[0], max[1], max[2],
        true
    );
}

static PluginParameters 
parameters = {
    
    {"file",          PARAM_STRING,    1, FLAG_NONE, "PLY file to load"},
        
    PARAMETERS_DONE         // Sentinel (signals end of list)
};

static PluginFunctions
functions = {

    NULL,               // Plugin load
    NULL,               // Plugin unload
    
    load_ply_file,      // Generate    
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


