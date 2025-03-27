#include "../../include/string/string.h"
#include <string.h>
#include <ctype.h>

StatusCode safe_strcpy(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src) return STATUS_NULL_POINTER;
    if (dest_size == 0) return STATUS_INVALID_ARGUMENT;

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
    return STATUS_OK;
}

StatusCode safe_strcat(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src) return STATUS_NULL_POINTER;

    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size - 1) return STATUS_MESSAGE_TOO_LARGE;

    strncat(dest, src, dest_size - dest_len - 1);
    return STATUS_OK;
}

StatusCode starts_with(const char *str, const char *prefix, bool *result) {
    if (!str || !prefix || !result) return STATUS_NULL_POINTER;

    *result = strncmp(str, prefix, strlen(prefix)) == 0;
    return STATUS_OK;
}

StatusCode ends_with(const char *str, const char *suffix, bool *result) {
    if (!str || !suffix || !result) return STATUS_NULL_POINTER;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) {
        *result = false;
        return STATUS_OK;
    }

    *result = strcmp(str + str_len - suffix_len, suffix) == 0;
    return STATUS_OK;
}

StatusCode trim(char *str) {
    if (!str) return STATUS_NULL_POINTER;

    char *start = str;
    while (isspace((unsigned char)*start)) start++;

    char *end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';

    if (start != str) memmove(str, start, end - start + 2);

    return STATUS_OK;
}
