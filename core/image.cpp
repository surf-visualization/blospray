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
#include <OpenEXR/ImfNamespace.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfStringAttribute.h>
#include <OpenEXR/ImfMatrixAttribute.h>
#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfCompressionAttribute.h>
#include "image.h"

using namespace OIIO;
using namespace OPENEXR_IMF_NAMESPACE;

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

// Color only, RGBA, 4 floats per pixel
static bool
writeFramebufferEXRColorOnly(const char *fname, int width, int height, bool compress, const float *color)
{
    auto out = ImageOutput::create(fname);
    
    if (!out)    
        return false;
    
    const int channels = 4; // RGBA
    
    // Framebuffer pixels start at lower-left
    // "The origin of the screen coordinate system in OSPRay is the lower 
    // left corner (as in OpenGL), thus the first pixel addressed by the 
    // returned pointer is the lower left pixel of the image."
    
    int scanlinesize = width * channels * sizeof(float);
    
    ImageSpec spec(width, height, channels, TypeDesc::FLOAT);
    
    if (!compress)
        spec.attribute("Compression", "none");

    out->open(fname, spec);
    out->write_image(TypeDesc::FLOAT, 
        (uint8_t*)color + size_t(height-1)*scanlinesize,
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

bool
writeFramebufferEXR(const char *fname, int width, int height, bool compress, const float *color, const float *depth, const float *normal, const float *albedo)
{
    if (depth == nullptr && normal == nullptr && albedo == nullptr)
        return writeFramebufferEXRColorOnly(fname, width, height, compress, color);

    // XXX currently broken even when only color is used. Apparently the
    // channels names can't be loaded, results in 
    // RuntimeError: Error: RE_layer_load_from_file: failed to load '/dev/shm/blosprayfb.exr'

    Header header(width, height);

    header.channels().insert("View Layer.Combined.R", Channel(FLOAT));
    header.channels().insert("View Layer.Combined.G", Channel(FLOAT));
    header.channels().insert("View Layer.Combined.B", Channel(FLOAT));
    header.channels().insert("View Layer.Combined.A", Channel(FLOAT));
    if (depth != nullptr)
        header.channels().insert("View Layer.Depth.Z", Channel(FLOAT));
    if (normal != nullptr)
    {
        header.channels().insert("View Layer.Normal.X", Channel(FLOAT));
        header.channels().insert("View Layer.Normal.Y", Channel(FLOAT));
        header.channels().insert("View Layer.Normal.Z", Channel(FLOAT));
    }
    if (albedo != nullptr)
    {
        header.channels().insert("Denoise Albedo.R", Channel(FLOAT));
        header.channels().insert("Denoise Albedo.G", Channel(FLOAT));
        header.channels().insert("Denoise Albedo.B", Channel(FLOAT));
    }

    OutputFile file(fname, header);

    if (!compress)
        header.insert("compression", CompressionAttribute(NO_COMPRESSION));

    // Framebuffer pixels start at lower-left
    // "The origin of the screen coordinate system in OSPRay is the lower 
    // left corner (as in OpenGL), thus the first pixel addressed by the 
    // returned pointer is the lower left pixel of the image."

    FrameBuffer framebuffer;

    const int color_scanlinesize = width * 4 * sizeof(float);
    framebuffer.insert("View Layer.Combined.R",
        Slice(FLOAT, (char*)color + size_t(height-1)*color_scanlinesize, 4*sizeof(float), -color_scanlinesize));
    framebuffer.insert("View Layer.Combined.G",
        Slice(FLOAT, (char*)color + size_t(height-1)*color_scanlinesize + 1*sizeof(float), 4*sizeof(float), -color_scanlinesize));    
    framebuffer.insert("View Layer.Combined.B",
        Slice(FLOAT, (char*)color + size_t(height-1)*color_scanlinesize + 2*sizeof(float), 4*sizeof(float), -color_scanlinesize));
    framebuffer.insert("View Layer.Combined.A",
        Slice(FLOAT, (char*)color + size_t(height-1)*color_scanlinesize + 3*sizeof(float), 4*sizeof(float), -color_scanlinesize));

    if (depth != nullptr)
    {
        const int depth_scanlinesize = width * sizeof(float);
        framebuffer.insert("View Layer.Depth.Z",
            Slice(FLOAT, (char*)depth + size_t(height-1)*depth_scanlinesize, sizeof(float), -depth_scanlinesize));
    }

    if (normal != nullptr)
    {
        const int normal_scanlinesize = width * 3 * sizeof(float);
        framebuffer.insert("View Layer.Normal.X",
            Slice(FLOAT, (char*)normal + size_t(height-1)*normal_scanlinesize, 3*sizeof(float), -normal_scanlinesize));
        framebuffer.insert("View Layer.Normal.Y",
            Slice(FLOAT, (char*)normal + size_t(height-1)*normal_scanlinesize + 1*sizeof(float), 3*sizeof(float), -normal_scanlinesize));
        framebuffer.insert("View Layer.Normal.Z",
            Slice(FLOAT, (char*)normal + size_t(height-1)*normal_scanlinesize + 2*sizeof(float), 3*sizeof(float), -normal_scanlinesize));
    }

    if (albedo != nullptr)
    {
        const int albedo_scanlinesize = width * 3 * sizeof(float);
        framebuffer.insert("Denoise Albedo.R",
            Slice(FLOAT, (char*)albedo + size_t(height-1)*albedo_scanlinesize, 3*sizeof(float), -albedo_scanlinesize));
        framebuffer.insert("Denoise Albedo.G",
            Slice(FLOAT, (char*)albedo + size_t(height-1)*albedo_scanlinesize + 1*sizeof(float), 3*sizeof(float), -albedo_scanlinesize));
        framebuffer.insert("Denoise Albedo.B",
            Slice(FLOAT, (char*)albedo + size_t(height-1)*albedo_scanlinesize + 2*sizeof(float), 3*sizeof(float), -albedo_scanlinesize));
    }

    file.setFrameBuffer(framebuffer);
    file.writePixels(height);

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
