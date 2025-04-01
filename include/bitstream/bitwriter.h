#ifndef BITWRITER_H
#define BITWRITER_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t size;     
    uint8_t bit_buffer;  
    int bit_count;       
} BitWriter;

void bitwriter_init(BitWriter *bw, size_t initial_capacity);
void bitwriter_free(BitWriter *bw);
void bitwriter_write_bits(BitWriter *bw, uint32_t value, int count);
void bitwriter_align_to_byte(BitWriter *bw);
uint8_t* bitwriter_get_data(BitWriter *bw, size_t *out_size);

#endif
