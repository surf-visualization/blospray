/*
Ospray render server for blospray

Paul Melis, SURFsara <paul.melis@surfsara.nl>
Copyright (C) 2017-2018
*/

// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
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


/* This is a small example tutorial how to use OSPRay in an application.
 *
 * On Linux build it in the build_directory with
 *   g++ ../apps/ospTutorial.cpp -I ../ospray/include -I .. ./libospray.so -Wl,-rpath,. -o ospTutorial
 * On Windows build it in the build_directory\$Configuration with
 *   cl ..\..\apps\ospTutorial.cpp /EHsc -I ..\..\ospray\include -I ..\.. ospray.lib
 */

#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <dlfcn.h>
#ifdef _WIN32
#  include <malloc.h>
#else
#  include <alloca.h>
#endif

#include <boost/program_options.hpp>
#include <boost/uuid/detail/sha1.hpp>

#include <ospray/ospray.h>

#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "image.h"
#include "tcpsocket.h"
#include "messages.pb.h"
#include "json.hpp"
#include "cool2warm.h"

using json = nlohmann::json;

const int   PORT = 5909;

// image size
osp::vec2i  image_size;

OSPModel        world;
OSPCamera       camera;
OSPRenderer     renderer;
OSPFrameBuffer  framebuffer;
bool            framebuffer_created = false;

OSPLight        lights[2];                          // 0 = ambient, 1 = sun

// Volume loaders

typedef OSPVolume   (*volume_load_function)(json &parameters, const float *obj2world, float *bbox);

typedef std::map<std::string, volume_load_function>     VolumeLoadFunctionMap;
VolumeLoadFunctionMap                                   volume_load_functions;

// Utility

inline double
time_diff(struct timeval t0, struct timeval t1)
{
    return t1.tv_sec - t0.tv_sec + (t1.tv_usec - t0.tv_usec) * 0.000001;
}

// https://stackoverflow.com/a/39833022/9296788
std::string 
get_sha1(const std::string& p_arg)
{
    boost::uuids::detail::sha1 sha1;
    sha1.process_bytes(p_arg.data(), p_arg.size());
    unsigned hash[5] = {0};
    sha1.get_digest(hash);

    // Back to string
    char buf[41] = {0};

    for (int i = 0; i < 5; i++)
    {
        std::sprintf(buf + (i << 3), "%08x", hash[i]);
    }

    return std::string(buf);
}


void 
clear_scene()
{
    
}

#if 0
void
create_scene(int max_rbcs, int max_plts)
{    
    // Do instancing
  
    OSPGeometry     instance;
    osp::affine3f   xform;
    
    glm::mat4       R;
    float           *M;
  
    uint32_t  num_rbc, num_plt, num_wbc;
    float     tx, ty, tz, rx, ry, rz;
    //float     A, B, C, D, E, F, AD, BD;
  
    FILE *p = fopen("cells.bin", "rb");
    fread(&num_rbc, sizeof(uint32_t), 1, p);
    fread(&num_plt, sizeof(uint32_t), 1, p);
    fread(&num_wbc, sizeof(uint32_t), 1, p);
    printf("On-disk scene: %d rbc, %d plt, %d wbc\n", num_rbc, num_plt, num_wbc);

    // Directly add mesh (no instancing)
    //ospAddGeometry(world, mesh);
    
    // Instance RBCs & PLTs
    
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
        
        R = glm::rotate(glm::mat4(1.0f), glm::radians(rx), glm::vec3(1,0,0));
        R = glm::rotate(R, glm::radians(ry), glm::vec3(0,1,0));
        R = glm::rotate(R, glm::radians(rz), glm::vec3(0,0,1));

        M = glm::value_ptr(R);

        xform.l.vx.x = M[0];
        xform.l.vx.y = M[1];
        xform.l.vx.z = M[2];

        xform.l.vy.x = M[4];
        xform.l.vy.y = M[5];
        xform.l.vy.z = M[6];

        xform.l.vz.x = M[8];
        xform.l.vz.y = M[9];
        xform.l.vz.z = M[10];

        xform.p = { tx, ty, tz };

        // Add instance
        instance = ospNewInstance(mesh_model_rbc, xform);
        
        ospAddGeometry(world, instance);
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

        R = glm::rotate(glm::mat4(1.0f), glm::radians(rx), glm::vec3(1,0,0));
        R = glm::rotate(R, glm::radians(ry), glm::vec3(0,1,0));
        R = glm::rotate(R, glm::radians(rz), glm::vec3(0,0,1));

        /*glm::mat4 T = glm::translate(
        glm::mat4(1.0f), glm::vec3(tx, ty, tz)
        );*/

        M = glm::value_ptr(R);

        xform.l.vx.x = M[0];
        xform.l.vx.y = M[1];
        xform.l.vx.z = M[2];

        xform.l.vy.x = M[4];
        xform.l.vy.y = M[5];
        xform.l.vy.z = M[6];

        xform.l.vz.x = M[8];
        xform.l.vz.y = M[9];
        xform.l.vz.z = M[10];

        xform.p = { tx, ty, tz };

        // Add instance
        instance = ospNewInstance(mesh_model_plt, xform);
        
        ospAddGeometry(world, instance);
    }        
    
    fclose(p);        
    
    // Add ground plane
    add_ground_plane();
            
    ospCommit(world);
    
    printf("Data loaded...\n");
}
#endif

std::vector<char>   receive_buffer;

std::vector<float>      vertex_buffer;
std::vector<uint32_t>   triangle_buffer;

ImageSettings   image_settings;
RenderSettings  render_settings;
CameraSettings  camera_settings;
LightSettings   light_settings;

template<typename T>
bool
receive_settings(TCPSocket *sock, T& settings)
{
    uint32_t message_size;
    
    if (sock->recvall(&message_size, 4) == -1)
        return false;
    
    receive_buffer.clear();
    receive_buffer.reserve(message_size);
    
    if (sock->recvall(&receive_buffer[0], message_size) == -1)
        return false;
    
    // XXX this probably makes a copy?
    std::string message(&receive_buffer[0], message_size);
    
    settings.ParseFromString(message);
    
    return true;
}

bool
receive_mesh(TCPSocket *sock)
{
    uint32_t num_vertices, num_triangles;
    
    if (sock->recvall(&num_vertices, 4) == -1)
        return false;
    if (sock->recvall(&num_triangles, 4) == -1)
        return false;
    
    printf("New triangle mesh: %d vertices, %d triangles\n", num_vertices, num_triangles);
    
    vertex_buffer.reserve(num_vertices*3);
    triangle_buffer.reserve(num_triangles*3);
    
    if (sock->recvall(&vertex_buffer[0], num_vertices*3*sizeof(float)) == -1)
        return false;
    if (sock->recvall(&triangle_buffer[0], num_triangles*3*sizeof(uint32_t)) == -1)
        return false;
    
    OSPGeometry mesh = ospNewGeometry("triangles");
  
        OSPData data = ospNewData(num_vertices, OSP_FLOAT3, &vertex_buffer[0]);   
        ospCommit(data);
        ospSetData(mesh, "vertex", data);

        //data = ospNewData(num_vertices, OSP_FLOAT4, colors);
        //ospCommit(data);
        //ospSetData(mesh, "vertex.color", data);

        data = ospNewData(num_triangles, OSP_INT3, &triangle_buffer[0]);            
        ospCommit(data);
        ospSetData(mesh, "index", data);

    ospCommit(mesh);
    
    ospAddGeometry(world, mesh);    
    
    return true;
}

bool
receive_volume(TCPSocket *sock)
{
    uint32_t    properties_size;
    float       obj2world[16];
    
    // Object-to-world matrix
    
    if (sock->recvall(&obj2world, 16*sizeof(float)) == -1)
        return false;   
    
    // Custom properties
    
    if (sock->recvall(&properties_size, 4) == -1)
        return false;
    
    std::string encoded_properties(properties_size, ' ');
    
    if (sock->recvall(&encoded_properties[0], properties_size) == -1)
        return false;
    
    printf("%d\n", encoded_properties.size());
    
    printf("Receive volume properties:\n%s\n", encoded_properties.c_str());
    
    json properties = json::parse(encoded_properties);
    
    printf("New volume:\n%s\n", properties.dump().c_str());
    
    // Find load function 
    
    volume_load_function load_function = NULL;
    
    const std::string& voltype = properties["voltype"];
    
    VolumeLoadFunctionMap::iterator it = volume_load_functions.find(voltype);
    if (it == volume_load_functions.end())
    {
        printf("No load function yet for volume type '%s'\n", voltype.c_str());
        
        std::string plugin_name = "voltype_" + voltype + ".so";
        
        printf("Opening %s\n", plugin_name.c_str());
        
        void *plugin = dlopen(plugin_name.c_str(), RTLD_LAZY);
        
        if (!plugin) 
        {
            fprintf(stderr, "dlopen() error: %s\n", dlerror());
            return false;
        }
        
        // Clear previous error
        dlerror(); 
        
        load_function = (volume_load_function) dlsym(plugin, "load");
        
        if (load_function == NULL)
        {
            fprintf(stderr, "dlsym() error: %s\n", dlerror());
            return false;
        }
        
        volume_load_functions[voltype] = load_function;
    }
    else
        load_function = it->second;
    
    // Let load function do its job
    
    struct timeval t0, t1;
    
    printf("Calling load function\n");
    
    gettimeofday(&t0, NULL);
    
    OSPVolume   volume;
    float       bbox[6];
    
    volume = load_function(properties, obj2world, bbox);
    
    gettimeofday(&t1, NULL);
    printf("Load function done, took %.3fs\n", time_diff(t0, t1));

#if 0
    // See https://github.com/ospray/ospray/pull/165, support for volume transformations was reverted
    ospSetVec3f(volume, "xfm.l.vx", osp::vec3f{ obj2world[0], obj2world[4], obj2world[8] });
    ospSetVec3f(volume, "xfm.l.vy", osp::vec3f{ obj2world[1], obj2world[5], obj2world[9] });
    ospSetVec3f(volume, "xfm.l.vz", osp::vec3f{ obj2world[2], obj2world[6], obj2world[10] });
    ospSetVec3f(volume, "xfm.p", osp::vec3f{ obj2world[3], obj2world[7], obj2world[11] });
#endif

    if (properties.find("sampling_rate") != properties.end())
        ospSetf(volume,  "samplingRate", properties["sampling_rate"].get<float>());
    else
        ospSetf(volume,  "samplingRate", 0.1f);
    
    ospSet1b(volume, "adaptiveSampling", false);
    ospSet1b(volume, "gradientShadingEnabled", true);
    
    // Transfer function
    
    float tf_colors[3*cool2warm_entries];
    float tf_opacities[cool2warm_entries];

    for (int i = 0; i < cool2warm_entries; i++)
    {
        tf_opacities[i]  = cool2warm[4*i+0];
        tf_colors[3*i+0] = cool2warm[4*i+1];
        tf_colors[3*i+1] = cool2warm[4*i+2];
        tf_colors[3*i+2] = cool2warm[4*i+3];
    }

    OSPTransferFunction tf = ospNewTransferFunction("piecewise_linear");
    
    if (properties.find("data_range") != properties.end())
    {
        float minval = properties["data_range"][0];
        float maxval = properties["data_range"][1];
        ospSet2f(tf, "valueRange", minval, maxval);
    }
    
    OSPData color_data = ospNewData(cool2warm_entries, OSP_FLOAT3, tf_colors);
    ospSetData(tf, "colors", color_data);
    ospRelease(color_data);
    
    OSPData opacity_data = ospNewData(cool2warm_entries, OSP_FLOAT, tf_opacities);
    ospSetData(tf, "opacities", opacity_data);
    ospRelease(opacity_data);
    
    ospCommit(tf);
    
    ospSetObject(volume,"transferFunction", tf);
    ospRelease(tf);
    
    ospCommit(volume);
    
    if (properties.find("isovalues") != properties.end())
    {
        // Isosurfacing
        
        printf("Representing volume with isosurface(s)\n");
        
        json isovalues_prop = properties["isovalues"];
        int n = isovalues_prop.size();

        float *isovalues = new float[n];
        for (int i = 0; i < n; i++)
            isovalues[i] = isovalues_prop[i];
        
        OSPData isovaluesData = ospNewData(n, OSP_FLOAT, isovalues);
        ospCommit(isovaluesData);
        
        delete [] isovalues;
        
        OSPGeometry isosurface = ospNewGeometry("isosurfaces");
        
            ospSetObject(isosurface, "volume", volume);
            ospRelease(volume);

            ospSetData(isosurface, "isovalues", isovaluesData);
            ospRelease(isovaluesData);
            
        ospCommit(isosurface);
            
        ospAddGeometry(world, isosurface);
        ospRelease(isosurface);
    }
    else if (properties.find("slice_plane") != properties.end())
    {
        // Slice plane (only a single one supported, atm)
        
        printf("Representing volume with slice plane\n");
        
        json slice_plane_prop = properties["slice_plane"];
        
        if (slice_plane_prop.size() == 4)
        {
            float plane[4];
            for (int i = 0; i < 4; i++)
                plane[i] = slice_plane_prop[i];
            
            OSPData planeData = ospNewData(4, OSP_FLOAT, plane);
            ospCommit(planeData);
            
            OSPGeometry slices = ospNewGeometry("slices");
            
                ospSetObject(slices, "volume", volume);
                ospRelease(volume);

                ospSetData(slices, "planes", planeData);
                ospRelease(planeData);
                
            ospCommit(slices);
                
            ospAddGeometry(world, slices);
            ospRelease(slices);
        }
        else
        {
            fprintf(stderr, "ERROR: slice_plane attribute should contain list of 4 floats values!\n");
        }
    }
    else
    {
        // Represent as volume 
        
        ospAddVolume(world, volume);
        ospRelease(volume);
    }

    
    /*
    OSPModel model = ospNewModel();
        ospAddGeometry(model, isosurface);
    ospCommit(model);
    //ospRelease(isosurface);
    
    // XXX hmm, get a segfault in ospray (null pointer it seems) when
    // instantiating the isosurfaces. This only happens on the 2nd blender render
    osp::affine3f    xform;
    
    xform.l.vx.x = obj2world[0];
    xform.l.vx.y = obj2world[4];
    xform.l.vx.z = obj2world[8];
    
    xform.l.vy.x = obj2world[1];
    xform.l.vy.y = obj2world[5];
    xform.l.vy.z = obj2world[9];
    
    xform.l.vz.x = obj2world[2];
    xform.l.vz.y = obj2world[6];
    xform.l.vz.z = obj2world[10];
    
    xform.p.x = obj2world[3];
    xform.p.y = obj2world[7];
    xform.p.z = obj2world[11];
    
    OSPGeometry imodel = ospNewInstance(model, xform);
    ospAddGeometry(world, imodel);
    //ospRelease(imodel);
    */

    // Send back hash and bbox of loaded volume
    
    std::string hash = get_sha1(encoded_properties);
    
    if (sock->send((uint8_t*)hash.c_str(), 40) == -1)
        return false;
    
    if (sock->sendall((uint8_t*)bbox, 6*4) == -1)
        return false;
    
    return true;
}

// XXX currently has big memory leak as we never release the new objects ;-)
bool
receive_scene(TCPSocket *sock)
{
    receive_buffer.reserve(4);
    
    // Set up renderer
    
    renderer = ospNewRenderer("scivis"); 
    
    float bgcol[] = { 1, 1, 1, 1 };
    
    ospSet1i(renderer, "aoSamples", 2);
    ospSet1i(renderer, "shadowsEnabled", 1);
    ospSet4fv(renderer, "bgColor", bgcol);
        
    // Create/update framebuffer
    
    // XXX use percentage value? or is that handled in the blender side?
    
    receive_settings(sock, image_settings);
    
    if (image_size.x != image_settings.width() || image_size.y != image_settings.height())
    {
        image_size.x = image_settings.width() ;
        image_size.y = image_settings.height();
        
        if (framebuffer_created)
            ospRelease(framebuffer);
        
        framebuffer = ospNewFrameBuffer(image_size, OSP_FB_RGBA32F, OSP_FB_COLOR | /*OSP_FB_DEPTH |*/ OSP_FB_ACCUM);            
        framebuffer_created = true;
    }
    
    // Render settings
    
    receive_settings(sock, render_settings);

    // Update camera
    
    receive_settings(sock, camera_settings);
    
    float cam_pos[3], cam_viewdir[3], cam_updir[3];
    
    cam_pos[0] = camera_settings.position(0);
    cam_pos[1] = camera_settings.position(1);
    cam_pos[2] = camera_settings.position(2);
    
    cam_viewdir[0] = camera_settings.view_dir(0); 
    cam_viewdir[1] = camera_settings.view_dir(1); 
    cam_viewdir[2] = camera_settings.view_dir(2); 

    cam_updir[0] = camera_settings.up_dir(0);
    cam_updir[1] = camera_settings.up_dir(1);
    cam_updir[2] = camera_settings.up_dir(2);
    
    camera = ospNewCamera("perspective");

    ospSetf(camera, "aspect", image_size.x/(float)image_size.y);
    ospSet3fv(camera, "pos", cam_pos);
    ospSet3fv(camera, "dir", cam_viewdir);
    ospSet3fv(camera, "up",  cam_updir);
    ospSetf(camera, "fovy",  camera_settings.fov_y());
    if (camera_settings.dof_focus_distance() > 0.0f)
    {
        // XXX seem to stuck in loop during rendering when distance is 0
        ospSetf(camera, "focusDistance", camera_settings.dof_focus_distance());
        ospSetf(camera, "apertureRadius", camera_settings.dof_aperture());
    }
    ospCommit(camera); 
    
    ospSetObject(renderer, "camera", camera);
    
    // Lights
    
    receive_settings(sock, light_settings);
    
    // AO
    lights[0] = ospNewLight3("ambient");
    ospSet1f(lights[0], "intensity", light_settings.ambient_intensity());
    ospCommit(lights[0]);
    
    // Sun
    float sun_dir[3];
    sun_dir[0] = light_settings.sun_dir(0);
    sun_dir[1] = light_settings.sun_dir(1);
    sun_dir[2] = light_settings.sun_dir(2);

    lights[1] = ospNewLight3("directional");
    ospSet3fv(lights[1], "direction",  sun_dir);
    ospSet1f(lights[1], "intensity", light_settings.sun_intensity());    
    ospCommit(lights[1]);
    
    // All lights
    OSPData light_data = ospNewData(2, OSP_LIGHT, &lights);  
    ospCommit(light_data);
    
    ospSetObject(renderer, "lights", light_data); 
    
    // Setup world and scene objects

    world = ospNewModel();
    
    char    data_type;
    
    while (true)
    {
        if (sock->recvall(&data_type, 1) == -1)
            return false;
        
        if (data_type == 'M')
        {
            if (!receive_mesh(sock))
                return false;
        }
        else if (data_type == 'V')
        {
            if (!receive_volume(sock))
                return false;
        }
        else if (data_type == '!')
        {
            // No more objects
            break;
        }
    }
    
    ospCommit(world);
    
    ospSetObject(renderer, "model",  world);
    
    // Done!
    
    ospCommit(renderer);
    
    return true;
}


void
render_frame(bool clear=true)
{
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    
    if (clear)
    {
        // Clear framebuffer    
        ospFrameBufferClear(framebuffer, OSP_FB_COLOR | OSP_FB_ACCUM);
    }

    // Render N samples
    ospRenderFrame(framebuffer, renderer, OSP_FB_COLOR | OSP_FB_ACCUM);
    //for (int i = 0; i < 8; i++)
    //    ospRenderFrame(framebuffer, renderer, OSP_FB_COLOR | OSP_FB_ACCUM);
    
    gettimeofday(&t1, NULL);
    printf("Frame in %.3f seconds\n", time_diff(t0, t1));
}

void 
send_framebuffer(TCPSocket *sock)
{
    uint32_t bufsize;
    
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    
    // Access framebuffer 
    const float *fb = (float*)ospMapFrameBuffer(framebuffer, OSP_FB_COLOR);
    
#if 1
    // Write to OpenEXR file and send *its* contents
    const char *FBFILE = "/dev/shm/orsfb.exr";
    
    writeEXRFramebuffer(FBFILE, image_size, fb);
    
    struct stat st;
    
    stat(FBFILE, &st);
    
    printf("Sending framebuffer as OpenEXR file, %d byte\n", st.st_size);
    
    bufsize = st.st_size;
    sock->send((uint8_t*)&bufsize, 4);
    sock->sendfile(FBFILE);
#else
    // Send directly
    bufsize = image_size.x*image_size.y*4*4;
    
    printf("Sending %d bytes of framebuffer data\n", bufsize);
    
    sock->send(&bufsize, 4);
    sock->sendall((uint8_t*)fb, image_size.x*image_size.y*4*4);
#endif
    
    // Unmap framebuffer
    ospUnmapFrameBuffer(fb, framebuffer);    
    
    gettimeofday(&t1, NULL);
    printf("Sent framebuffer in %.3f seconds\n", time_diff(t0, t1));
}


int 
main(int argc, const char **argv) 
{
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    
    // Initialize OSPRay.
    // OSPRay parses (and removes) its commandline parameters, e.g. "--osp:debug"
    ospInit(&argc, argv);
    
    // Parse options
    namespace po = boost::program_options;
    
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help",        "produce help message")
    ;
    
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);      
    
    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }    
        
    // Server loop

    // Framebuffer "init"
    image_size.x = image_size.y = 0;
    framebuffer_created = false;
    
    TCPSocket   *listen_sock, *sock;
    
    listen_sock = new TCPSocket;
    listen_sock->bind(PORT);
    listen_sock->listen(1);
    
    printf("Listening...\n");
    
    while (true)
    {            
        sock = listen_sock->accept();
        
        printf("Got new connection\n");
        
        if (receive_scene(sock))
        {
            for (int i = 1; i <= render_settings.samples(); i++)
            {
                printf("Rendering sample %d\n", i);
                render_frame(i == 1);

                printf("Sending framebuffer\n");
                send_framebuffer(sock);
            }
        }
        else
            sock->close();
    }

    return 0;
}