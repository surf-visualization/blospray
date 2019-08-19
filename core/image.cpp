// ======================================================================== //
// BLOSPRAY - OSPRay as a Blender render engine                             //
// Paul Melis, SURFsara <paul.melis@surfsara.nl>                            //
// Image output support                                                     //
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

#include <OpenImageIO/imageio.h>
#include "image.h"

using namespace OIIO;

bool
writePNG(const char *fileName, int width, int height, const uint32_t *pixel)
{
    auto out = ImageOutput::create(fileName);
    
    if (!out)    
        return false;
    
    const int channels = 4; // RGBA
    
    // Framebuffer pixels start at lower-left
    // "The origin of the screen coordinate system in OSPRay is the lower 
    // left corner (as in OpenGL), thus the first pixel addressed by the 
    // returned pointer is the lower left pixel of the image."
    
    int scanlinesize = width * channels;
    
    ImageSpec spec(width, height, channels, TypeDesc::UINT8);
    out->open(fileName, spec);
    out->write_image(TypeDesc::UINT8, 
        (uint8_t*)pixel + size_t(height-1)*scanlinesize,
        AutoStride,
        -scanlinesize, 
        AutoStride
    );
    
    out->close();    
#if OIIO_VERSION < 10903
    ImageOutput::destroy(out);
#endif
    
    return true;
}

// RGBA, i.e. 4 floats per pixel
bool
writeEXRFramebuffer(const char *fileName, int width, int height, const float *pixel)
{
    auto out = ImageOutput::create(fileName);
    
    if (!out)    
        return false;
    
    const int channels = 4; // RGBA
    
    // Framebuffer pixels start at lower-left
    // "The origin of the screen coordinate system in OSPRay is the lower 
    // left corner (as in OpenGL), thus the first pixel addressed by the 
    // returned pointer is the lower left pixel of the image."
    
    int scanlinesize = width * channels * 4;
    
    ImageSpec spec(width, height, channels, TypeDesc::FLOAT);
    out->open(fileName, spec);
    out->write_image(TypeDesc::FLOAT, 
        (uint8_t*)pixel + size_t(height-1)*scanlinesize,
        AutoStride,
        -scanlinesize, 
        AutoStride
    );
    
    out->close();
#if OIIO_VERSION < 10903    
    ImageOutput::destroy(out);
#endif    
    
    return true;
}

void 
writePPM(const char *fileName, int width, int height, const uint32_t *pixel)
{
    FILE *file = fopen(fileName, "wb");
    fprintf(file, "P6\n%i %i\n255\n", width, height);
    unsigned char *out = (unsigned char *)alloca(3*width);
    for (int y = 0; y < height; y++) {
        const unsigned char *in = (const unsigned char *)&pixel[(height-1-y)*width];
        for (int x = 0; x < width; x++) {
          out[3*x + 0] = in[4*x + 0];
          out[3*x + 1] = in[4*x + 1];
          out[3*x + 2] = in[4*x + 2];
        }
        fwrite(out, 3*width, sizeof(char), file);
    }
    fprintf(file, "\n");
    fclose(file);
}
