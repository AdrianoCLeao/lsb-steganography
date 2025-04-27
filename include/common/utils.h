#ifndef UTILS_H
#define UTILS_H

#include "common.h"
#include <stdlib.h>
#include <stdint.h>

void print_error(StatusCode code);
int read_text_file(const char *path, uint8_t **out_buf, size_t *out_len);

#endif 
