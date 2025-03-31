#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/image/png.h"

const uint8_t PNG_SIGNATURE[8] = {137, 80, 78, 71, 13, 10, 26, 10};

typedef struct {
    uint32_t length;
    char type[5];
    uint8_t *data;
} PNGChunk;

uint32_t read_uint32_be(FILE *fp) {
    uint8_t bytes[4];
    fread(bytes, 1, 4, fp);
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

int validate_png_signature(FILE *fp) {
    uint8_t sig[8];
    fread(sig, 1, 8, fp);
    return memcmp(sig, PNG_SIGNATURE, 8) == 0;
}

StatusCode load_png_image(const char *filepath, PNGImage *image) {
    if (!filepath || !image) return STATUS_NULL_POINTER;

    FILE *fp = fopen(filepath, "rb");
    if (!fp) return STATUS_FILE_NOT_FOUND;

    if (!validate_png_signature(fp)) {
        fclose(fp);
        return STATUS_INVALID_FORMAT;
    }

    uint8_t *idat_data = NULL;
    size_t idat_size = 0;
    int width = 0, height = 0, bit_depth = 0, color_type = 0;

    printf("Reading PNG chunks:\n");

    while (!feof(fp)) {
        uint32_t length = read_uint32_be(fp);
        if (feof(fp)) break;

        char type[5] = {0};
        fread(type, 1, 4, fp);

        uint8_t *data = malloc(length);
        fread(data, 1, length, fp);
        fseek(fp, 4, SEEK_CUR); 

        printf("  Chunk: %s | Length: %u bytes\n", type, length);

        if (strcmp(type, "IHDR") == 0) {
            width      = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            height     = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
            bit_depth  = data[8];
            color_type = data[9];

            if (bit_depth != 8 || (color_type != 2 && color_type != 6)) {
                free(data);
                fclose(fp);
                return STATUS_INVALID_FORMAT;
            }
        } else if (strcmp(type, "IDAT") == 0) {
            idat_data = realloc(idat_data, idat_size + length);
            memcpy(idat_data + idat_size, data, length);
            idat_size += length;
        } else if (strcmp(type, "IEND") == 0) {
            free(data);
            break;
        }

        free(data);
    }

    fclose(fp);

    if (idat_size < 2) {
        free(idat_data);
        return STATUS_INVALID_FORMAT;
    }

    uint8_t cmf = idat_data[0];
    uint8_t flg = idat_data[1];
    int compression_method = cmf & 0x0F;

    if (compression_method != 8) {
        free(idat_data);
        return STATUS_INVALID_FORMAT;
    }

    printf("\nPNG loaded: %dx%d | IDAT size: %zu bytes | Color type: %d\n", width, height, idat_size, color_type);

    free(idat_data);
    return STATUS_OK;
}

void free_png_image(PNGImage *image) {
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
