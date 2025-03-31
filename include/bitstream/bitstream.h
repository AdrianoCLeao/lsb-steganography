#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <stdint.h>
#include <stddef.h>

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

#endif
