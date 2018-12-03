#include <OpenImageIO/imageio.h>
#include "image.h"

using namespace OIIO;

bool
writePNG(const char *fileName, const osp::vec2i &size, const uint32_t *pixel)
{
    ImageOutput *out = ImageOutput::create(fileName);
    if (!out)    
        return false;
    
    const int channels = 4; // RGBA
    
    // Framebuffer pixels start at lower-left
    // "The origin of the screen coordinate system in OSPRay is the lower 
    // left corner (as in OpenGL), thus the first pixel addressed by the 
    // returned pointer is the lower left pixel of the image."
    
    int scanlinesize = size.x * channels;
    
    ImageSpec spec(size.x, size.y, channels, TypeDesc::UINT8);
    out->open(fileName, spec);
    out->write_image(TypeDesc::UINT8, 
        (uint8_t*)pixel + (size.y-1)*scanlinesize,
        AutoStride,
        -scanlinesize, 
        AutoStride
    );
    out->close();
    ImageOutput::destroy(out);
    
    return true;
}

// RGBA, i.e. 4 floats per pixel
bool
writeEXRFramebuffer(const char *fileName, const osp::vec2i &size, const float *pixel)
{
    ImageOutput *out = ImageOutput::create(fileName);
    if (!out)    
        return false;
    
    const int channels = 4; // RGBA
    
    // Framebuffer pixels start at lower-left
    // "The origin of the screen coordinate system in OSPRay is the lower 
    // left corner (as in OpenGL), thus the first pixel addressed by the 
    // returned pointer is the lower left pixel of the image."
    
    int scanlinesize = size.x * channels * 4;
    
    ImageSpec spec(size.x, size.y, channels, TypeDesc::FLOAT);
    out->open(fileName, spec);
    out->write_image(TypeDesc::FLOAT, 
        (uint8_t*)pixel + (size.y-1)*scanlinesize,
        AutoStride,
        -scanlinesize, 
        AutoStride
    );
    out->close();
    ImageOutput::destroy(out);
    
    return true;
}

void 
writePPM(const char *fileName, const osp::vec2i &size, const uint32_t *pixel)
{
    FILE *file = fopen(fileName, "wb");
    fprintf(file, "P6\n%i %i\n255\n", size.x, size.y);
    unsigned char *out = (unsigned char *)alloca(3*size.x);
    for (int y = 0; y < size.y; y++) {
        const unsigned char *in = (const unsigned char *)&pixel[(size.y-1-y)*size.x];
        for (int x = 0; x < size.x; x++) {
          out[3*x + 0] = in[4*x + 0];
          out[3*x + 1] = in[4*x + 1];
          out[3*x + 2] = in[4*x + 2];
        }
        fwrite(out, 3*size.x, sizeof(char), file);
    }
    fprintf(file, "\n");
    fclose(file);
}
