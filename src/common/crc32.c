#include <stdint.h>
#include "../../include/common/crc32.h"

static uint32_t crc_table[256];
static int table_computed = 0;

static void make_crc_table(void) {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) {
            if (c & 1) c = 0xEDB88320U ^ (c >> 1);
            else c >>= 1;
        }
        crc_table[n] = c;
    }
    table_computed = 1;
}

uint32_t crc32(uint32_t crc, const uint8_t *buf, size_t len) {
    if (!table_computed) make_crc_table();

    crc = crc ^ 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++) {
        crc = crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}
