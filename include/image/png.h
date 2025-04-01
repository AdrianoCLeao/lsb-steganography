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

StatusCode png_embed_message(PNGImage *image, const uint8_t *msg, size_t msg_len);
StatusCode png_extract_message(PNGImage *image, char **message_out);
StatusCode save_png_image(const char *path, PNGImage *image);

#endif 
