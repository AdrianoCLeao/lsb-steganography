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
