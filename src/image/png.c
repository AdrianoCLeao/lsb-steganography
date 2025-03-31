#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/image/png.h"
#include "../include/bitstream/bitstream.h"

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

StatusCode inflate_uncompressed_blocks(const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len) {
    if (!input || !output || !output_len) return STATUS_NULL_POINTER;
    if (input_len < 6) return STATUS_INVALID_FORMAT;

    size_t pos = 2;
    size_t capacity = 1024;
    size_t out_pos = 0;

    uint8_t *out = malloc(capacity);
    if (!out) return STATUS_OUT_OF_MEMORY;

    int bfinal = 0;

    while (!bfinal && pos + 5 <= input_len) {
        uint8_t header = input[pos++];
        bfinal = header & 0x01;
        uint8_t btype = (header >> 1) & 0x03;

        if (btype != 0) {
            free(out);
            return STATUS_NOT_IMPLEMENTED;
        }

        if (pos + 4 > input_len) {
            free(out);
            return STATUS_INVALID_FORMAT;
        }

        uint16_t len  = input[pos] | (input[pos + 1] << 8);
        uint16_t nlen = input[pos + 2] | (input[pos + 3] << 8);
        pos += 4;

        if ((len ^ 0xFFFF) != nlen) {
            free(out);
            return STATUS_INVALID_FORMAT;
        }

        if (pos + len > input_len) {
            free(out);
            return STATUS_INVALID_FORMAT;
        }

        if (out_pos + len > capacity) {
            capacity = (out_pos + len) * 2;
            out = realloc(out, capacity);
            if (!out) return STATUS_OUT_OF_MEMORY;
        }

        memcpy(out + out_pos, input + pos, len);
        out_pos += len;
        pos += len;
    }

    *output = out;
    *output_len = out_pos;
    return STATUS_OK;
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

    uint8_t *decompressed = NULL;
    size_t decompressed_len = 0;

    code = inflate_deflate_blocks(idat_data, idat_size, &decompressed, &decompressed_len);
    free(idat_data); 
    if (code != STATUS_OK) return code;

    printf("  Decompressed size: %zu bytes\n", decompressed_len);

    size_t bytes_per_pixel = 3;
    size_t stride = width * bytes_per_pixel + 1; 

    if (decompressed_len != stride * height) {
        free(decompressed);
        return STATUS_INVALID_FORMAT;
    }

    image->width = width;
    image->height = height;

    image->red = malloc(sizeof(uint8_t *) * height);
    image->green = malloc(sizeof(uint8_t *) * height);
    image->blue = malloc(sizeof(uint8_t *) * height);
    if (!image->red || !image->green || !image->blue) {
        free(decompressed);
        return STATUS_OUT_OF_MEMORY;
    }

    for (int y = 0; y < height; y++) {
        image->red[y]   = malloc(sizeof(uint8_t) * width);
        image->green[y] = malloc(sizeof(uint8_t) * width);
        image->blue[y]  = malloc(sizeof(uint8_t) * width);
        if (!image->red[y] || !image->green[y] || !image->blue[y]) {
            free(decompressed);
            return STATUS_OUT_OF_MEMORY;
        }
    }

    for (int y = 0; y < height; y++) {
        size_t row_start = y * stride;
        uint8_t filter_type = decompressed[row_start];

        switch (filter_type) {
            case 0: break; 
            case 1: 
            case 2: 
            case 3: 
            case 4:
                fprintf(stderr, "PNG filter type %d not implemented.\n", filter_type);
                free(decompressed);
                return STATUS_NOT_IMPLEMENTED;
            default:
                fprintf(stderr, "Invalid filter type: %d\n", filter_type);
                free(decompressed);
                return STATUS_INVALID_FORMAT;
        }

        for (int x = 0; x < width; x++) {
            size_t pixel_offset = row_start + 1 + x * 3;
            image->red[y][x]   = decompressed[pixel_offset + 0];
            image->green[y][x] = decompressed[pixel_offset + 1];
            image->blue[y][x]  = decompressed[pixel_offset + 2];
        }
    }

    free(decompressed);
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
