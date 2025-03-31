#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>
#include "../common/common.h"

typedef struct {
    int width;
    int height;
    uint8_t **red;
    uint8_t **green;
    uint8_t **blue;
} RGBImage;

StatusCode load_bmp_image(const char *filepath, RGBImage *image);
void free_rgb_image(RGBImage *image);

void print_pixel(RGBImage *image, int x, int y);
uint8_t get_bit(uint8_t value, int bit_position);
void print_lsb_matrix(uint8_t **channel, int width, int height);

#endif 
