#include <stdlib.h>

#include "../../include/bitstream/bitstream.h"

void bitstream_init(BitStream *bs, const uint8_t *data, size_t byte_len) {
    bs->data = data;
    bs->bit_len = byte_len * 8;
    bs->bit_pos = 0;
}

uint32_t bitstream_read_bits(BitStream *bs, int n) {
    uint32_t result = 0;
    for (int i = 0; i < n; i++) {
        if (bs->bit_pos >= bs->bit_len) return result;

        size_t byte_pos = bs->bit_pos / 8;
        int bit_offset = bs->bit_pos % 8;
        uint8_t bit = (bs->data[byte_pos] >> bit_offset) & 1;

        result |= (bit << i);
        bs->bit_pos++;
    }
    return result;
}

uint32_t bitstream_peek_bits(BitStream *bs, int n) {
    size_t original_pos = bs->bit_pos;
    uint32_t val = bitstream_read_bits(bs, n);
    bs->bit_pos = original_pos;
    return val;
}

void bitstream_align_to_byte(BitStream *bs) {
    if (bs->bit_pos % 8 != 0) {
        bs->bit_pos += (8 - bs->bit_pos % 8);
    }
}

int bitstream_eof(BitStream *bs) {
    return bs->bit_pos >= bs->bit_len;
}

StatusCode inflate_fixed_huffman(BitStream *bs, uint8_t **out, size_t *out_len) {
    size_t capacity = 4096;
    size_t size = 0;
    uint8_t *buffer = malloc(capacity);
    if (!buffer) return STATUS_OUT_OF_MEMORY;

    while (!bitstream_eof(bs)) {
        uint32_t symbol = 0;

        symbol = bitstream_peek_bits(bs, 9);
        if (symbol <= 0b10111111) { 
            symbol = bitstream_read_bits(bs, 8);
        } else if (symbol >= 0b110010000) { 
            symbol = bitstream_read_bits(bs, 9);
        } else {
            free(buffer);
            return STATUS_INVALID_FORMAT;
        }

        if (symbol < 256) {
            if (size >= capacity) {
                capacity *= 2;
                buffer = realloc(buffer, capacity);
                if (!buffer) return STATUS_OUT_OF_MEMORY;
            }
            buffer[size++] = (uint8_t)symbol;
        } else if (symbol >= 257 && symbol <= 285) {
            int length_code = symbol - 257;
            int length = LENGTH_BASE[length_code];
            int extra_bits = LENGTH_EXTRA[length_code];
            if (extra_bits > 0) {
                length += bitstream_read_bits(bs, extra_bits);
            }

            int dist_code = bitstream_read_bits(bs, 5);
            if (dist_code >= 30) {
                free(buffer);
                return STATUS_INVALID_FORMAT;
            }
            int distance = DIST_BASE[dist_code];
            int dist_extra = DIST_EXTRA[dist_code];
            if (dist_extra > 0) {
                distance += bitstream_read_bits(bs, dist_extra);
            }

            if (distance > size) {
                free(buffer);
                return STATUS_INVALID_FORMAT;
            }

            if (size + length > capacity) {
                capacity = (size + length) * 2;
                buffer = realloc(buffer, capacity);
                if (!buffer) return STATUS_OUT_OF_MEMORY;
            }

            for (int i = 0; i < length; i++) {
                buffer[size] = buffer[size - distance];
                size++;
            }
        } else {
            free(buffer);
            return STATUS_NOT_IMPLEMENTED;
        }
    }

    *out = buffer;
    *out_len = size;
    return STATUS_OK;
}