#include <stdio.h>
#include <stdbool.h>
#include "../include/string/string.h"
#include "../include/common/utils.h"

int main() {
    char buffer[50];

    StatusCode code = safe_strcpy(buffer, "Hello", sizeof(buffer));
    if (code != STATUS_OK) {
        print_error(code);
        return 1;
    }

    code = safe_strcat(buffer, ", world!", sizeof(buffer));
    if (code != STATUS_OK) {
        print_error(code);
        return 1;
    }

    printf("After strcat: '%s'\n", buffer);

    bool result = false;
    code = starts_with(buffer, "Hello", &result);
    if (code == STATUS_OK) {
        printf("Starts with 'Hello': %s\n", result ? "true" : "false");
    } else {
        print_error(code);
    }

    code = ends_with(buffer, "world!", &result);
    if (code == STATUS_OK) {
        printf("Ends with 'world!': %s\n", result ? "true" : "false");
    } else {
        print_error(code);
    }

    char to_trim[] = "   trimmed text   ";
    printf("Before trim: '%s'\n", to_trim);
    code = trim(to_trim);
    if (code == STATUS_OK) {
        printf("After trim: '%s'\n", to_trim);
    } else {
        print_error(code);
    }

    return 0;
}
