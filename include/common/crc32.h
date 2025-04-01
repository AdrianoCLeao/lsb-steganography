#ifndef CRC32_H
#define CRC32_H

#include <stddef.h>
#include <stdint.h>

uint32_t crc32(uint32_t crc, const uint8_t *buf, size_t len);

#endif
