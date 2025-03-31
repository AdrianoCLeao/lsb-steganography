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
