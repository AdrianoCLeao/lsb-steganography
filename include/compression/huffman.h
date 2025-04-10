#ifndef HUFFMAN_COMPRESS_H
#define HUFFMAN_COMPRESS_H

#include <stdint.h>
#include <stddef.h>
#include "../bitstream/bitwriter.h"
#include "../common/common.h"

#define NUM_SYMBOLS 288

typedef struct {
    uint32_t code;
    int length;
} HuffmanCode;

typedef struct {
    int symbol;
    int count;
} RunLength;

StatusCode compress_huffman_fixed(const uint8_t *input, size_t input_len, BitWriter *bw);
StatusCode compress_huffman_dynamic(const uint8_t *input, size_t input_len, BitWriter *bw);
StatusCode compress_huffman_block(const uint8_t *input, size_t input_len, BitWriter *bw);

#endif
