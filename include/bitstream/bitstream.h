#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <stdint.h>
#include <stddef.h>

#include "../common/common.h"

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

#endif
