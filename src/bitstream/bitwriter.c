#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/bitstream/bitwriter.h"

void bitwriter_init(BitWriter *bw, size_t initial_capacity) {
    bw->buffer = malloc(initial_capacity);
    memset(bw->buffer, 0, initial_capacity);
    bw->capacity = initial_capacity;
    bw->size = 0;
    bw->bit_buffer = 0;
    bw->bit_count = 0;
}

void bitwriter_free(BitWriter *bw) {
    free(bw->buffer);
    bw->buffer = NULL;
    bw->capacity = 0;
    bw->size = 0;
    bw->bit_buffer = 0;
    bw->bit_count = 0;
}

static void bitwriter_flush(BitWriter *bw) {
    if (bw->bit_count == 0) return;

    if (bw->size >= bw->capacity) {
        bw->capacity *= 2;
        bw->buffer = realloc(bw->buffer, bw->capacity);
    }

    bw->buffer[bw->size++] = bw->bit_buffer;
    bw->bit_buffer = 0;
    bw->bit_count = 0;
}

void bitwriter_write_bits(BitWriter *bw, uint32_t value, int count) {
    for (int i = 0; i < count; i++) {
        bw->bit_buffer |= ((value >> i) & 1) << bw->bit_count;
        bw->bit_count++;

        if (bw->bit_count == 8) {
            bitwriter_flush(bw);
        }
    }
}

void bitwriter_align_to_byte(BitWriter *bw) {
    if (bw->bit_count > 0) {
        bitwriter_flush(bw);
    }
}

uint8_t* bitwriter_get_data(BitWriter *bw, size_t *out_size) {
    *out_size = bw->size;
    return bw->buffer;
}
