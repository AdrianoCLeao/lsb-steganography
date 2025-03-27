#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stddef.h>
#include <stdbool.h>
#include "../common/common.h"

StatusCode safe_strcpy(char *dest, const char *src, size_t dest_size);
StatusCode safe_strcat(char *dest, const char *src, size_t dest_size);
StatusCode starts_with(const char *str, const char *prefix, bool *result);
StatusCode ends_with(const char *str, const char *suffix, bool *result);
StatusCode trim(char *str);

#endif
