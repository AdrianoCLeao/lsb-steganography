#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <stdint.h>
#include <stddef.h>

#include "../common/common.h"

extern const int LENGTH_BASE[29];
extern const int LENGTH_EXTRA[29];
extern const int DIST_BASE[30];
extern const int DIST_EXTRA[30];

typedef struct {
    const uint8_t *data;
    size_t bit_len;
    size_t bit_pos;
} BitStream;

void bitstream_init(BitStream *bs, const uint8_t *data, size_t byte_len);
uint32_t bitstream_read_bits(BitStream *bs, int n);
uint32_t bitstream_peek_bits(BitStream *bs, int n);
void bitstream_align_to_byte(BitStream *bs);
int bitstream_eof(BitStream *bs);

StatusCode inflate_fixed_huffman(BitStream *bs, uint8_t **out, size_t *out_len);
StatusCode inflate_deflate_blocks(const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len);

#endif
