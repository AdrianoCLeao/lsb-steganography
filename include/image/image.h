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

StatusCode save_bmp_image(const char *filepath, const RGBImage *image);

uint8_t set_bit(uint8_t value, int bit_position, uint8_t bit);

StatusCode embed_message(RGBImage *image, const uint8_t *message, size_t len);
StatusCode extract_message(RGBImage *image, uint8_t *buffer, size_t max_len);

StatusCode string_to_bits(const char *str, uint8_t *bit_array, size_t max_bits, size_t *out_bits);
StatusCode bits_to_string(const uint8_t *bit_array, size_t bit_len, char *out_str, size_t max_len);

#endif 
