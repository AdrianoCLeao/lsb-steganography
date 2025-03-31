#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/image/png.h"

const uint8_t PNG_SIGNATURE[8] = {137, 80, 78, 71, 13, 10, 26, 10};

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

StatusCode parse_png_chunks(
    const char *filepath,
    int *width,
    int *height,
    int *bit_depth,
    int *color_type,
    uint8_t **out_idat_data,
    size_t *out_idat_size
) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return STATUS_FILE_NOT_FOUND;

    if (!validate_png_signature(fp)) {
        fclose(fp);
        return STATUS_INVALID_FORMAT;
    }

    uint8_t *idat_data = NULL;
    size_t idat_size = 0;

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
            *width      = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
            *height     = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
            *bit_depth  = data[8];
            *color_type = data[9];

            if (*bit_depth != 8 || (*color_type != 2 && *color_type != 6)) {
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

    *out_idat_data = idat_data;
    *out_idat_size = idat_size;

    return STATUS_OK;
}

StatusCode load_png_image(const char *filepath, PNGImage *image) {
    if (!filepath || !image) return STATUS_NULL_POINTER;

    int width, height, bit_depth, color_type;
    uint8_t *idat_data = NULL;
    size_t idat_size = 0;

    StatusCode code = parse_png_chunks(
        filepath,
        &width,
        &height,
        &bit_depth,
        &color_type,
        &idat_data,
        &idat_size
    );
    if (code != STATUS_OK) return code;

    printf("\nPNG metadata:\n");
    printf("  Width: %d\n", width);
    printf("  Height: %d\n", height);
    printf("  Bit depth: %d\n", bit_depth);
    printf("  Color type: %d\n", color_type);
    printf("  IDAT size: %zu bytes\n", idat_size);

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
