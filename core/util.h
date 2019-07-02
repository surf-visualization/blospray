// ======================================================================== //
// BLOSPRAY - OSPRay as a Blender render engine                             //
// Paul Melis, SURFsara <paul.melis@surfsara.nl>                            //
// Utility routines                                                         //
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

#ifndef UTIL_H
#define UTIL_H

#include <sys/time.h>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <boost/version.hpp>
#if BOOST_VERSION >= 106600
#include <boost/uuid/detail/sha1.hpp>
#else
#include <boost/uuid/sha1.hpp>
#endif
#include <glm/gtc/type_ptr.hpp>

#include <ospray.h>
#include "messages.pb.h"
#include "tcpsocket.h"

/*
void
affine3f_from_rowmajor(osp::affine3f& xform, float *m)
{
    xform.l.vx = osp::vec3f{ m[0], m[4], m[8] };
    xform.l.vy = osp::vec3f{ m[1], m[5], m[9] };
    xform.l.vz = osp::vec3f{ m[2], m[6], m[10] };
    xform.p    = osp::vec3f{ m[3], m[7], m[11] };
}

void
affine3f_from_colmajor(osp::affine3f& xform, float *m)
{
    xform.l.vx = osp::vec3f{ m[0], m[1], m[2] };
    xform.l.vy = osp::vec3f{ m[4], m[5], m[6] };
    xform.l.vz = osp::vec3f{ m[8], m[9], m[10] };
    xform.p    = osp::vec3f{ m[12], m[13], m[14] };
}

void
affine3f_from_mat4(osp::affine3f& xform, const glm::mat4 &mat)
{
    const float *M = glm::value_ptr(mat);

    xform.l.vx.x = M[0];
    xform.l.vx.y = M[1];
    xform.l.vx.z = M[2];

    xform.l.vy.x = M[4];
    xform.l.vy.y = M[5];
    xform.l.vy.z = M[6];

    xform.l.vz.x = M[8];
    xform.l.vz.y = M[9];
    xform.l.vz.z = M[10];

    xform.p.x = M[12];
    xform.p.y = M[13];
    xform.p.z = M[14];
}

osp::affine3f
affine3f_from_mat4(const glm::mat4 &mat)
{
    osp::affine3f xform;
    affine3f_from_mat4(xform, mat);
    return xform;
}
*/

void
affine3fv_from_mat4(float *xform, const glm::mat4 &mat)
{
    const float *M = glm::value_ptr(mat);

    xform[0] = M[0];
    xform[1] = M[1];
    xform[2] = M[2];

    xform[3] = M[4];
    xform[4] = M[5];
    xform[5] = M[6];

    xform[6] = M[8];
    xform[7] = M[9];
    xform[8] = M[10];

    xform[9] = M[12];
    xform[10] = M[13];
    xform[11] = M[14];
}

inline double
time_diff(struct timeval t0, struct timeval t1)
{
    return t1.tv_sec - t0.tv_sec + (t1.tv_usec - t0.tv_usec) * 0.000001;
}

inline float 
float_swap(float value)
{
    union v {
        float       f;
        uint32_t    i;
    };

    v val;
    val.f = value;
    val.i = htonl(val.i);                 
    return val.f;
};

template<typename T>
bool
receive_protobuf(TCPSocket *sock, T& protobuf)
{
    std::vector<char> receive_buffer;
    uint32_t message_size;
    
    if (sock->recvall(&message_size, 4) == -1)
        return false;
    
    receive_buffer.reserve(message_size);
        
    if (sock->recvall(&receive_buffer[0], message_size) == -1)
        return false;
    
    // XXX this probably makes a copy?
    std::string message(&receive_buffer[0], message_size);
    
    protobuf.ParseFromString(message);
    
    return true;
}

template<typename T>
bool
send_protobuf(TCPSocket *sock, T& protobuf)
{
    std::string message;
    uint32_t message_size;
    
    protobuf.SerializeToString(&message);
    
    message_size = message.size();
    
    if (sock->send((uint8_t*)&message_size, 4) == -1)
        return false;
    
    if (sock->sendall((uint8_t*)&message[0], message_size) == -1)
        return false;
    
    return true;
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

// Linux only
float
memory_usage()
{
    // Based on https://stackoverflow.com/a/12675172/9296788
    int tSize = 0, resident = 0, share = 0;
    std::ifstream buffer("/proc/self/statm");
    buffer >> tSize >> resident >> share;
    buffer.close();
    
    float page_size_mb = 1.0f * sysconf(_SC_PAGE_SIZE) / (1024*1024); 
    float rss = resident * page_size_mb;
    /*
    cout << "RSS - " << rss << " kB\n";

    double shared_mem = share * page_size_kb;
    cout << "Shared Memory - " << shared_mem << " kB\n";

    cout << "Private Memory - " << rss - shared_mem << "kB\n";*/
    
    return rss;
}

#endif
