#ifndef JPEG_STEG_NO_LIB_H
#define JPEG_STEG_NO_LIB_H

#include <stddef.h>
#include <stdint.h>

int embed_message_jpeg_no_lib(const char *input_path,
                              const char *output_path,
                              const uint8_t *message,
                              size_t message_len);

int extract_message_jpeg_no_lib(const char *input_path,
                                uint8_t **out_message,
                                size_t *out_len);

#endif 
