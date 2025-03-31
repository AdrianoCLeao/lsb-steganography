#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

StatusCode build_huffman_tree(const uint8_t *code_lengths, int num_symbols, HuffmanTree *tree) {
    if (!code_lengths || !tree) return STATUS_NULL_POINTER;

    int bl_count[HUFFMAN_MAX_BITS + 1] = {0};
    int next_code[HUFFMAN_MAX_BITS + 1] = {0};

    for (int i = 0; i < num_symbols; i++) {
        int len = code_lengths[i];
        if (len > HUFFMAN_MAX_BITS) return STATUS_INVALID_FORMAT;
        if (len > 0) bl_count[len]++;
    }

    int code = 0;
    for (int bits = 1; bits <= HUFFMAN_MAX_BITS; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }

    tree->num_symbols = num_symbols;
    tree->max_bits = HUFFMAN_MAX_BITS;
    tree->codes = malloc(sizeof(int) * (1 << HUFFMAN_MAX_BITS));
    tree->lengths = malloc(sizeof(int) * num_symbols);
    if (!tree->codes || !tree->lengths) return STATUS_OUT_OF_MEMORY;

    for (int i = 0; i < (1 << HUFFMAN_MAX_BITS); i++) tree->codes[i] = -1;

    for (int i = 0; i < num_symbols; i++) {
        int len = code_lengths[i];
        if (len > 0) {
            int code_val = next_code[len]++;
            int reversed = 0;
            for (int b = 0; b < len; b++) {
                reversed = (reversed << 1) | ((code_val >> b) & 1);
            }
            for (int j = 0; j < (1 << (HUFFMAN_MAX_BITS - len)); j++) {
                int idx = (reversed << (HUFFMAN_MAX_BITS - len)) | j;
                tree->codes[idx] = i;
            }
            tree->lengths[i] = len;
        }
    }

    return STATUS_OK;
}

StatusCode huffman_read_symbol(BitStream *bs, const HuffmanTree *tree) {
    uint32_t code = 0;
    for (int len = 1; len <= tree->max_bits; len++) {
        code |= bitstream_read_bits(bs, 1) << (len - 1);
        int index = code << (HUFFMAN_MAX_BITS - len);
        int symbol = tree->codes[index];
        if (symbol >= 0 && tree->lengths[symbol] == len) {
            return symbol;
        }
    }
    return -1;
}

StatusCode inflate_dynamic_huffman(BitStream *bs, uint8_t **out, size_t *out_len) {
    if (!bs || !out || !out_len) return STATUS_NULL_POINTER;

    int HLIT  = bitstream_read_bits(bs, 5) + 257;
    int HDIST = bitstream_read_bits(bs, 5) + 1;
    int HCLEN = bitstream_read_bits(bs, 4) + 4;

    if (HLIT > 286 || HDIST > 30) return STATUS_INVALID_FORMAT;

    int code_length_order[19] = {
        16, 17, 18,  0, 8, 7, 9, 6,
        10, 5, 11, 4, 12, 3, 13, 2,
        14, 1, 15
    };

    uint8_t code_lengths[19] = {0};
    for (int i = 0; i < HCLEN; i++) {
        code_lengths[code_length_order[i]] = bitstream_read_bits(bs, 3);
    }

    HuffmanTree cl_tree;
    StatusCode code = build_huffman_tree(code_lengths, 19, &cl_tree);
    if (code != STATUS_OK) return code;

    int total_codes = HLIT + HDIST;
    uint8_t *all_lengths = malloc(total_codes);
    if (!all_lengths) {
        free(cl_tree.codes);
        free(cl_tree.lengths);
        return STATUS_OUT_OF_MEMORY;
    }

    int i = 0;
    while (i < total_codes) {
        int sym = huffman_read_symbol(bs, &cl_tree);
        if (sym < 0) {
            free(cl_tree.codes); free(cl_tree.lengths);
            free(all_lengths);
            return STATUS_INVALID_FORMAT;
        }

        if (sym <= 15) {
            all_lengths[i++] = sym;
        } else if (sym == 16) {
            if (i == 0) return STATUS_INVALID_FORMAT;
            int repeat = bitstream_read_bits(bs, 2) + 3;
            uint8_t prev = all_lengths[i - 1];
            for (int j = 0; j < repeat && i < total_codes; j++) all_lengths[i++] = prev;
        } else if (sym == 17) {
            int repeat = bitstream_read_bits(bs, 3) + 3;
            for (int j = 0; j < repeat && i < total_codes; j++) all_lengths[i++] = 0;
        } else if (sym == 18) {
            int repeat = bitstream_read_bits(bs, 7) + 11;
            for (int j = 0; j < repeat && i < total_codes; j++) all_lengths[i++] = 0;
        }
    }

    free(cl_tree.codes);
    free(cl_tree.lengths);

    HuffmanTree litlen_tree, dist_tree;
    code = build_huffman_tree(all_lengths, HLIT, &litlen_tree);
    if (code != STATUS_OK) {
        free(all_lengths);
        return code;
    }

    code = build_huffman_tree(all_lengths + HLIT, HDIST, &dist_tree);
    if (code != STATUS_OK) {
        free(all_lengths);
        free(litlen_tree.codes);
        free(litlen_tree.lengths);
        return code;
    }

    size_t capacity = 8192;
    size_t size = 0;
    uint8_t *buffer = malloc(capacity);
    if (!buffer) return STATUS_OUT_OF_MEMORY;

    while (1) {
        int sym = huffman_read_symbol(bs, &litlen_tree);
        if (sym < 0) {
            code = STATUS_INVALID_FORMAT;
            break;
        }

        if (sym < 256) {
            if (size >= capacity) {
                capacity *= 2;
                buffer = realloc(buffer, capacity);
                if (!buffer) {
                    code = STATUS_OUT_OF_MEMORY;
                    break;
                }
            }
            buffer[size++] = (uint8_t)sym;
        } else if (sym == 256) {
            code = STATUS_OK;
            break;
        } else if (sym >= 257 && sym <= 285) {
            int length_code = sym - 257;
            int length = LENGTH_BASE[length_code];
            int extra = LENGTH_EXTRA[length_code];
            if (extra > 0) length += bitstream_read_bits(bs, extra);

            int dist_code = huffman_read_symbol(bs, &dist_tree);
            if (dist_code < 0 || dist_code > 29) {
                code = STATUS_INVALID_FORMAT;
                break;
            }
            int distance = DIST_BASE[dist_code];
            int de = DIST_EXTRA[dist_code];
            if (de > 0) distance += bitstream_read_bits(bs, de);

            if (distance > size) {
                code = STATUS_INVALID_FORMAT;
                break;
            }

            if (size + length > capacity) {
                capacity = (size + length) * 2;
                buffer = realloc(buffer, capacity);
                if (!buffer) {
                    code = STATUS_OUT_OF_MEMORY;
                    break;
                }
            }

            for (int i = 0; i < length; i++) {
                buffer[size] = buffer[size - distance];
                size++;
            }
        } else {
            code = STATUS_INVALID_FORMAT;
            break;
        }
    }

    free(all_lengths);
    free(litlen_tree.codes); free(litlen_tree.lengths);
    free(dist_tree.codes); free(dist_tree.lengths);

    if (code == STATUS_OK) {
        *out = buffer;
        *out_len = size;
    } else {
        free(buffer);
    }

    return code;
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

        uint8_t *block_out = NULL;
        size_t block_len = 0;
        StatusCode code;

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
            code = inflate_fixed_huffman(&bs, &block_out, &block_len);
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
            code = inflate_dynamic_huffman(&bs, &block_out, &block_len);
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
