#include <stdlib.h>
#include <string.h>

#include "../../include/bitstream/bitstream.h"

const int LENGTH_BASE[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
    35,43,51,59,67,83,99,115,131,163,195,227,258
};
const int LENGTH_EXTRA[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
    3,3,3,3,4,4,4,4,5,5,5,5,0
};

const int DIST_BASE[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
    257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
};
const int DIST_EXTRA[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
    7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

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

StatusCode inflate_deflate_blocks(const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len) {
    if (!input || !output || !output_len) return STATUS_NULL_POINTER;

    BitStream bs;
    bitstream_init(&bs, input, input_len);

    bitstream_read_bits(&bs, 8); 
    bitstream_read_bits(&bs, 8); 

    size_t capacity = 8192;
    size_t size = 0;
    uint8_t *buffer = malloc(capacity);
    if (!buffer) return STATUS_OUT_OF_MEMORY;

    int done = 0;

    while (!done && !bitstream_eof(&bs)) {
        uint8_t bfinal = bitstream_read_bits(&bs, 1);
        uint8_t btype  = bitstream_read_bits(&bs, 2);

        if (btype == 0) {
            bitstream_align_to_byte(&bs);

            if (bs.bit_pos + 32 > bs.bit_len) {
                free(buffer);
                return STATUS_INVALID_FORMAT;
            }

            uint16_t len  = bitstream_read_bits(&bs, 16);
            uint16_t nlen = bitstream_read_bits(&bs, 16);
            if ((len ^ 0xFFFF) != nlen) {
                free(buffer);
                return STATUS_INVALID_FORMAT;
            }

            if (size + len > capacity) {
                capacity = (size + len) * 2;
                buffer = realloc(buffer, capacity);
                if (!buffer) return STATUS_OUT_OF_MEMORY;
            }

            for (int i = 0; i < len; i++) {
                buffer[size++] = bitstream_read_bits(&bs, 8);
            }
        }
        else if (btype == 1) {
            uint8_t *block_out = NULL;
            size_t block_len = 0;
            StatusCode code = inflate_fixed_huffman(&bs, &block_out, &block_len);
            if (code != STATUS_OK) {
                free(buffer);
                return code;
            }

            if (size + block_len > capacity) {
                capacity = (size + block_len) * 2;
                buffer = realloc(buffer, capacity);
                if (!buffer) return STATUS_OUT_OF_MEMORY;
            }

            memcpy(buffer + size, block_out, block_len);
            size += block_len;
            free(block_out);
        }
        else if (btype == 2) {
            free(buffer);
            return STATUS_NOT_IMPLEMENTED; 
        }
        else {
            free(buffer);
            return STATUS_INVALID_FORMAT;
        }

        if (bfinal) done = 1;
    }

    *output = buffer;
    *output_len = size;
    return STATUS_OK;
}