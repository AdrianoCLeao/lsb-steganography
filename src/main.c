#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/image/image.h"
#include "../include/image/png.h"
#include "../include/common/utils.h"

int main() {
    PNGImage image;
    StatusCode code;

    const char *input_image = "assets/test.png";
    const char *message_path = "assets/messages/message.txt";

    code = load_png_image(input_image, &image);
    if (code != STATUS_OK) {
        print_error(code);
        return 1;
    }

    printf("Metadados da imagem carregados com sucesso.\n");

    FILE *msg_fp = fopen(message_path, "rb");
    if (!msg_fp) {
        perror("Erro ao abrir arquivo de mensagem");
        free_png_image(&image);
        return 1;
    }

    fseek(msg_fp, 0, SEEK_END);
    long msg_len = ftell(msg_fp);
    fseek(msg_fp, 0, SEEK_SET);

    if (msg_len <= 0) {
        fprintf(stderr, "Arquivo de mensagem vazio ou invalido.\n");
        fclose(msg_fp);
        free_png_image(&image);
        return 1;
    }

    uint8_t *msg = malloc(msg_len);
    fread(msg, 1, msg_len, msg_fp);
    fclose(msg_fp);

    code = png_embed_message(&image, msg, msg_len);
    free(msg);

    if (code != STATUS_OK) {
        print_error(code);
        free_png_image(&image);
        return 1;
    }

    printf("Mensagem embutida com sucesso na imagem!\n");

    free_png_image(&image);
    return 0;
}
