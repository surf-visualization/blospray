#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>
#include <ospray/ospray.h>

bool    writePNG(const char *fileName, const osp::vec2i &size, const uint32_t *pixel);
void    writePPM(const char *fileName, const osp::vec2i &size, const uint32_t *pixel);

bool    writeEXRFramebuffer(const char *fileName, const osp::vec2i &size, const float *pixel);

#endif
