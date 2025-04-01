#include <stdlib.h>
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
