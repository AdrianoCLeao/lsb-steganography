#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/image/image.h"
#include "../include/common/utils.h"

char *read_text_file(const char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, size, file);
    buffer[size] = '\0'; 

    fclose(file);
    return buffer;
}


int main() {
    RGBImage image;
    RGBImage modified;
    StatusCode code;

    const char *input_image = "assets/test.bmp";
    const char *output_image = "assets/output_test.bmp";
    
    char *message = read_text_file("assets/messages/message.txt");
    if (!message) {
        fprintf(stderr, "Failed to read message from file.\n");
        return 1;
    }

    code = load_bmp_image(input_image, &image);
    if (code != STATUS_OK) {
        print_error(code);
        return 1;
    }

    printf("Imagem carregada: %dx%d\n", image.width, image.height);

    size_t max_bits = image.width * image.height * 3;
    uint8_t *bit_array = malloc(max_bits);
    size_t bit_count = 0;

    code = string_to_bits(message, bit_array, max_bits, &bit_count);
    if (code != STATUS_OK) {
        print_error(code);
        free_rgb_image(&image);
        free(bit_array);
        return 1;
    }

    code = embed_message(&image, (const uint8_t *)message, strlen(message) + 1);
    if (code != STATUS_OK) {
        print_error(code);
        free_rgb_image(&image);
        free(bit_array);
        return 1;
    }

    code = save_bmp_image(output_image, &image);
    if (code != STATUS_OK) {
        print_error(code);
        free_rgb_image(&image);
        free(bit_array);
        return 1;
    }

    printf("A mensagem foi escrita e a imagem foi salva: %s\n", output_image);

    code = load_bmp_image(output_image, &modified);
    if (code != STATUS_OK) {
        print_error(code);
        free_rgb_image(&image);
        free(bit_array);
        return 1;
    }

    free(message);

    char extracted[256];
    code = extract_message(&modified, (uint8_t *)extracted, sizeof(extracted));
    if (code != STATUS_OK) {
        print_error(code);
    } else {
        printf("Mensagem Extraida: '%s'\n", extracted);
    }

    free_rgb_image(&image);
    free_rgb_image(&modified);
    free(bit_array);

    return 0;
}
