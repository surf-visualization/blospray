// ======================================================================== //
// BLOSPRAY - OSPRay as a Blender render engine                             //
// Paul Melis, SURFsara <paul.melis@surfsara.nl>                            //
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

#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>
#include <ospray/ospray.h>

bool    writePNG(const char *fileName, const osp::vec2i &size, const uint32_t *pixel);
void    writePPM(const char *fileName, const osp::vec2i &size, const uint32_t *pixel);

bool    writeEXRFramebuffer(const char *fileName, const osp::vec2i &size, const float *pixel);

#endif
