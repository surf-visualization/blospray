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
#include <ospray/ospray.h>

// XXX 
inline OSPData ospNewCopiedData(size_t numItems,
                          OSPDataType type,
                          const void *source)
{
    OSPData src = ospNewSharedData(source, type, numItems);
    OSPData dst = ospNewData(type, numItems);
    ospCopyData(src, dst);
    ospRelease(src);    
    return dst;
}


inline double
time_diff(struct timeval t0, struct timeval t1)
{
    return t1.tv_sec - t0.tv_sec + (t1.tv_usec - t0.tv_usec) * 0.000001;
}

inline float 
uint16_swap(uint16_t value)
{
    return ((value & 0xff) << 8) | (value >> 8);
};

inline float 
int16_swap(int16_t value)
{
    return ((value & 0xff) << 8) | (value >> 8);
};

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

#endif
