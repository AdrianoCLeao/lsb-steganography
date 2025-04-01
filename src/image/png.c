#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/image/png.h"
#include "../include/bitstream/bitstream.h"
#include "../include/bitstream/bitwriter.h"
#include "../include/decompression/huffman.h"
#include "../include/compression/huffman.h"
#include "../../include/common/crc32.h"

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

        uint8_t *row = &decompressed[row_start + 1];
        uint8_t *prev_row = y > 0 ? &decompressed[(y - 1) * stride + 1] : NULL;

        switch (filter_type) {
            case 0:
                break;
            case 1: // Sub
                for (size_t i = 3; i < width * 3; i++)
                    row[i] = (row[i] + row[i - 3]) & 0xFF;
                break;
            case 2: // Up
                if (prev_row)
                    for (size_t i = 0; i < width * 3; i++)
                        row[i] = (row[i] + prev_row[i]) & 0xFF;
                break;
            case 3: // Average
                for (size_t i = 0; i < width * 3; i++) {
                    uint8_t left = (i >= 3) ? row[i - 3] : 0;
                    uint8_t up = prev_row ? prev_row[i] : 0;
                    row[i] = (row[i] + ((left + up) / 2)) & 0xFF;
                }
                break;
            case 4: // Paeth
                for (size_t i = 0; i < width * 3; i++) {
                    uint8_t a = (i >= 3) ? row[i - 3] : 0;
                    uint8_t b = prev_row ? prev_row[i] : 0;
                    uint8_t c = (prev_row && i >= 3) ? prev_row[i - 3] : 0;

                    int p = a + b - c;
                    int pa = abs(p - a);
                    int pb = abs(p - b);
                    int pc = abs(p - c);

                    uint8_t pr;
                    if (pa <= pb && pa <= pc) pr = a;
                    else if (pb <= pc) pr = b;
                    else pr = c;

                    row[i] = (row[i] + pr) & 0xFF;
                }
                break;
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

StatusCode png_embed_message(PNGImage *image, const uint8_t *msg, size_t msg_len) {
    if (!image || !msg) return STATUS_NULL_POINTER;

    size_t total_bits = 32 + msg_len * 8;
    size_t capacity = image->width * image->height * 3;

    if (total_bits > capacity)
        return STATUS_MESSAGE_TOO_LARGE; 

    size_t bit_index = 0;

    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            for (int c = 0; c < 3; c++) {
                uint8_t *channel = (c == 0) ? &image->red[y][x] :
                                   (c == 1) ? &image->green[y][x] :
                                             &image->blue[y][x];

                if (bit_index < 32) {
                    uint32_t len = (uint32_t)msg_len;
                    int bit = (len >> (31 - bit_index)) & 1;
                    *channel = (*channel & ~1) | bit;
                } else if (bit_index < total_bits) {
                    size_t msg_bit = bit_index - 32;
                    int bit = (msg[msg_bit / 8] >> (7 - (msg_bit % 8))) & 1;
                    *channel = (*channel & ~1) | bit;
                }

                bit_index++;
                if (bit_index >= total_bits)
                    return STATUS_OK;
            }
        }
    }

    return STATUS_OK;
}

StatusCode png_extract_message(PNGImage *image, char **message_out) {
    if (!image || !message_out) return STATUS_NULL_POINTER;

    int width = image->width;
    int height = image->height;

    int bit_index = 0;
    uint8_t byte = 0;
    int msg_len = 0;
    int reading_len = 1;
    int len_bytes_collected = 0;

    uint8_t *msg_data = NULL;
    int msg_capacity = 0;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            for (int c = 0; c < 3; c++) {
                uint8_t color = c == 0 ? image->red[y][x]
                                : c == 1 ? image->green[y][x]
                                         : image->blue[y][x];

                byte = (byte << 1) | (color & 1);
                bit_index++;

                if (bit_index == 8) {
                    if (reading_len) {
                        msg_len = (msg_len << 8) | byte;
                        len_bytes_collected++;
                        if (len_bytes_collected == 4) {
                            msg_capacity = msg_len + 1;
                            msg_data = malloc(msg_capacity);
                            if (!msg_data) return STATUS_OUT_OF_MEMORY;
                            reading_len = 0;
                            bit_index = 0;
                            byte = 0;
                        }
                    } else {
                        static int msg_pos = 0;
                        if (msg_pos < msg_len) {
                            msg_data[msg_pos++] = byte;
                            byte = 0;
                            bit_index = 0;
                        } else {
                            msg_data[msg_pos] = '\0';
                            *message_out = (char *)msg_data;
                            return STATUS_OK;
                        }
                    }
                    bit_index = 0;
                    byte = 0;
                }
            }
        }
    }

    return STATUS_INVALID_FORMAT;
}

static void write_chunk(FILE *fp, const char *type, const uint8_t *data, uint32_t length) {
    uint8_t len_buf[4] = {
        (length >> 24) & 0xFF,
        (length >> 16) & 0xFF,
        (length >> 8) & 0xFF,
        length & 0xFF
    };
    fwrite(len_buf, 1, 4, fp);
    fwrite(type, 1, 4, fp);
    if (length > 0 && data) {
        fwrite(data, 1, length, fp);
    }

    uint32_t crc = crc32(0, (const uint8_t *)type, 4);
    if (length > 0 && data) {
        crc = crc32(crc, data, length);
    }

    uint8_t crc_buf[4] = {
        (crc >> 24) & 0xFF,
        (crc >> 16) & 0xFF,
        (crc >> 8) & 0xFF,
        crc & 0xFF
    };
    fwrite(crc_buf, 1, 4, fp);
}

StatusCode save_png_image(const char *path, PNGImage *image) {
    if (!path || !image) return STATUS_NULL_POINTER;

    FILE *fp = fopen(path, "wb");
    if (!fp) return STATUS_FILE_NOT_FOUND;

    fwrite(PNG_SIGNATURE, 1, 8, fp);

    uint8_t ihdr[13];
    ihdr[0] = (image->width >> 24) & 0xFF;
    ihdr[1] = (image->width >> 16) & 0xFF;
    ihdr[2] = (image->width >> 8) & 0xFF;
    ihdr[3] = image->width & 0xFF;
    ihdr[4] = (image->height >> 24) & 0xFF;
    ihdr[5] = (image->height >> 16) & 0xFF;
    ihdr[6] = (image->height >> 8) & 0xFF;
    ihdr[7] = image->height & 0xFF;
    ihdr[8] = 8; 
    ihdr[9] = 2; 
    ihdr[10] = 0; 
    ihdr[11] = 0;
    ihdr[12] = 0; 
    write_chunk(fp, "IHDR", ihdr, 13);

    int width = image->width;
    int height = image->height;
    size_t stride = 1 + 3 * width;
    size_t raw_size = stride * height;

    uint8_t *raw = malloc(raw_size);
    if (!raw) {
        fclose(fp);
        return STATUS_OUT_OF_MEMORY;
    }

    for (int y = 0; y < height; y++) {
        size_t row_start = y * stride;
        raw[row_start] = 0; // filter type 0 (None)
        for (int x = 0; x < width; x++) {
            raw[row_start + 1 + x * 3 + 0] = image->red[y][x];
            raw[row_start + 1 + x * 3 + 1] = image->green[y][x];
            raw[row_start + 1 + x * 3 + 2] = image->blue[y][x];
        }
    }

    BitWriter bw;
    bitwriter_init(&bw, 1024);

    bitwriter_write_bits(&bw, 1, 1); 
    bitwriter_write_bits(&bw, 1, 2); 

    StatusCode code = compress_huffman_fixed(raw, raw_size, &bw);
    if (code != STATUS_OK) {
        bitwriter_free(&bw);
        free(raw);
        fclose(fp);
        return code;
    }

    bitwriter_align_to_byte(&bw);
    size_t compressed_len;
    uint8_t *compressed_data = bitwriter_get_data(&bw, &compressed_len);

    write_chunk(fp, "IDAT", compressed_data, compressed_len);
    write_chunk(fp, "IEND", NULL, 0);

    bitwriter_free(&bw);
    free(raw);
    fclose(fp);
    return STATUS_OK;
}