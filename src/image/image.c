#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../../include/image/image.h"
#include "../../include/common/common.h"

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BMPHeader;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} DIBHeader;
#pragma pack(pop)

StatusCode load_bmp_image(const char *filepath, RGBImage *image) {
    if (!filepath || !image) return STATUS_NULL_POINTER;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) return STATUS_FILE_NOT_FOUND;

    BMPHeader bmp;
    fread(&bmp, sizeof(BMPHeader), 1, fp);
    if (bmp.bfType != 0x4D42) {
        fclose(fp);
        return STATUS_INVALID_FORMAT;
    }

    DIBHeader dib;
    fread(&dib, sizeof(DIBHeader), 1, fp);
    if (dib.biBitCount != 24 || dib.biCompression != 0) {
        fclose(fp);
        return STATUS_INVALID_FORMAT;
    }

    int width = dib.biWidth;
    int height = dib.biHeight;
    int row_padded = (width * 3 + 3) & (~3);

    image->width = width;
    image->height = height;

    image->red   = malloc(sizeof(uint8_t *) * height);
    image->green = malloc(sizeof(uint8_t *) * height);
    image->blue  = malloc(sizeof(uint8_t *) * height);
    if (!image->red || !image->green || !image->blue) {
        fclose(fp);
        return STATUS_OUT_OF_MEMORY;
    }

    for (int i = 0; i < height; i++) {
        image->red[i]   = malloc(sizeof(uint8_t) * width);
        image->green[i] = malloc(sizeof(uint8_t) * width);
        image->blue[i]  = malloc(sizeof(uint8_t) * width);
        if (!image->red[i] || !image->green[i] || !image->blue[i]) {
            fclose(fp);
            return STATUS_OUT_OF_MEMORY;
        }
    }

    fseek(fp, bmp.bfOffBits, SEEK_SET);

    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            uint8_t bgr[3];
            fread(bgr, 3, 1, fp);
            image->blue[y][x]  = bgr[0];
            image->green[y][x] = bgr[1];
            image->red[y][x]   = bgr[2];
        }
        fseek(fp, row_padded - (width * 3), SEEK_CUR);
    }

    fclose(fp);
    return STATUS_OK;
}

void free_rgb_image(RGBImage *image) {
    if (!image) return;
    for (int i = 0; i < image->height; i++) {
        free(image->red[i]);
        free(image->green[i]);
        free(image->blue[i]);
    }
    free(image->red);
    free(image->green);
    free(image->blue);
}

void print_pixel(RGBImage *image, int x, int y) {
    if (!image || x < 0 || y < 0 || x >= image->width || y >= image->height) {
        printf("Invalid pixel coordinates\n");
        return;
    }
    printf("Pixel (%d, %d): R=%d, G=%d, B=%d\n",
           x, y,
           image->red[y][x],
           image->green[y][x],
           image->blue[y][x]);
}

uint8_t get_bit(uint8_t value, int bit_position) {
    return (value >> bit_position) & 1;
}

void print_lsb_matrix(uint8_t **channel, int width, int height) {
    for (int y = 0; y < height && y < 10; y++) {
        for (int x = 0; x < width && x < 20; x++) {
            printf("%d ", get_bit(channel[y][x], 0));
        }
        printf("\n");
    }
}

uint8_t set_bit(uint8_t value, int bit_position, uint8_t bit) {
    if (bit)
        return value | (1 << bit_position);
    else
        return value & ~(1 << bit_position);
}

StatusCode embed_message(RGBImage *image, const uint8_t *message, size_t len) {
    if (!image || !message) return STATUS_NULL_POINTER;

    size_t total_bits = len * 8;
    size_t capacity = image->width * image->height * 3;
    if (total_bits > capacity) return STATUS_MESSAGE_TOO_LARGE;

    size_t bit_index = 0;
    for (int y = 0; y < image->height && bit_index < total_bits; y++) {
        for (int x = 0; x < image->width && bit_index < total_bits; x++) {
            uint8_t byte = message[bit_index / 8];
            int bit = (byte >> (7 - (bit_index % 8))) & 1;

            if (bit_index % 3 == 0)
                image->red[y][x] = set_bit(image->red[y][x], 0, bit);
            else if (bit_index % 3 == 1)
                image->green[y][x] = set_bit(image->green[y][x], 0, bit);
            else
                image->blue[y][x] = set_bit(image->blue[y][x], 0, bit);

            bit_index++;
        }
    }

    return STATUS_OK;
}

StatusCode extract_message(RGBImage *image, uint8_t *buffer, size_t max_len) {
    if (!image || !buffer) return STATUS_NULL_POINTER;

    size_t bit_index = 0;
    size_t byte_index = 0;
    uint8_t current_byte = 0;

    for (int y = 0; y < image->height && byte_index < max_len; y++) {
        for (int x = 0; x < image->width && byte_index < max_len; x++) {
            int bit;
            if (bit_index % 3 == 0)
                bit = get_bit(image->red[y][x], 0);
            else if (bit_index % 3 == 1)
                bit = get_bit(image->green[y][x], 0);
            else
                bit = get_bit(image->blue[y][x], 0);

            current_byte = (current_byte << 1) | bit;

            if (bit_index % 8 == 7) {
                buffer[byte_index++] = current_byte;
                if (current_byte == '\0') return STATUS_OK; 
                current_byte = 0;
            }

            bit_index++;
        }
    }

    return STATUS_OK;
}

StatusCode save_bmp_image(const char *filepath, const RGBImage *image) {
    if (!filepath || !image) return STATUS_NULL_POINTER;

    FILE *fp = fopen(filepath, "wb");
    if (!fp) return STATUS_IO_ERROR;

    int width = image->width;
    int height = image->height;
    int row_padded = (width * 3 + 3) & ~3;
    int image_size = row_padded * height;

    BMPHeader bmp = {
        .bfType = 0x4D42,
        .bfSize = sizeof(BMPHeader) + sizeof(DIBHeader) + image_size,
        .bfReserved1 = 0,
        .bfReserved2 = 0,
        .bfOffBits = sizeof(BMPHeader) + sizeof(DIBHeader)
    };

    DIBHeader dib = {
        .biSize = sizeof(DIBHeader),
        .biWidth = width,
        .biHeight = height,
        .biPlanes = 1,
        .biBitCount = 24,
        .biCompression = 0,
        .biSizeImage = image_size,
        .biXPelsPerMeter = 0,
        .biYPelsPerMeter = 0,
        .biClrUsed = 0,
        .biClrImportant = 0
    };

    fwrite(&bmp, sizeof(BMPHeader), 1, fp);
    fwrite(&dib, sizeof(DIBHeader), 1, fp);

    uint8_t *row = calloc(row_padded, sizeof(uint8_t));
    if (!row) {
        fclose(fp);
        return STATUS_OUT_OF_MEMORY;
    }

    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            row[x * 3 + 0] = image->blue[y][x];
            row[x * 3 + 1] = image->green[y][x];
            row[x * 3 + 2] = image->red[y][x];
        }
        fwrite(row, 1, row_padded, fp);
    }

    free(row);
    fclose(fp);
    return STATUS_OK;
}

StatusCode string_to_bits(const char *str, uint8_t *bit_array, size_t max_bits, size_t *out_bits) {
    if (!str || !bit_array || !out_bits) return STATUS_NULL_POINTER;

    size_t len = strlen(str) + 1;
    size_t total_bits = len * 8;
    if (total_bits > max_bits) return STATUS_MESSAGE_TOO_LARGE;

    for (size_t i = 0; i < len; i++) {
        for (int b = 7; b >= 0; b--) {
            bit_array[i * 8 + (7 - b)] = (str[i] >> b) & 1;
        }
    }

    *out_bits = total_bits;
    return STATUS_OK;
}

StatusCode bits_to_string(const uint8_t *bit_array, size_t bit_len, char *out_str, size_t max_len) {
    if (!bit_array || !out_str) return STATUS_NULL_POINTER;
    if (bit_len % 8 != 0) return STATUS_INVALID_ARGUMENT;

    size_t byte_len = bit_len / 8;
    if (byte_len > max_len) return STATUS_MESSAGE_TOO_LARGE;

    for (size_t i = 0; i < byte_len; i++) {
        uint8_t c = 0;
        for (int b = 0; b < 8; b++) {
            c = (c << 1) | bit_array[i * 8 + b];
        }
        out_str[i] = c;
    }

    return STATUS_OK;
}
