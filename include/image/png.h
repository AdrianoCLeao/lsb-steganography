#ifndef PNG_IMAGE_H
#define PNG_IMAGE_H

#include <stdint.h>
#include "../common/common.h"

typedef struct {
    int width;
    int height;
    uint8_t **red;
    uint8_t **green;
    uint8_t **blue;
} PNGImage;

StatusCode load_png_image(const char *filepath, PNGImage *image);
void free_png_image(PNGImage *image);

#endif 
