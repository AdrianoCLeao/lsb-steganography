#include "../include/image/jpeg.h"
#include "../include/common/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *in_jpeg   = "assets/jpg/lena.jpg";
    const char *out_jpeg  = "assets/jpg/lena_stego.jpg";
    const char *msg_path  = "assets/messages/message.txt";

    uint8_t *message = NULL;
    size_t  msg_len  = 0;
    if (read_text_file(msg_path, &message, &msg_len) != 0) {
        fprintf(stderr, "Erro ao ler mensagem de '%s'\n", msg_path);
        return 1;
    }

    if (embed_message_jpeg_no_lib(in_jpeg, out_jpeg, message, msg_len) != 0) {
        fprintf(stderr, "Erro ao embutir mensagem em '%s'\n", out_jpeg);
        free(message);
        return 1;
    }
    printf("Mensagem embutida em '%s' (%zu bytes).\n", out_jpeg, msg_len);

    free(message);

    uint8_t *extracted = NULL;
    size_t  ex_len     = 0;
    if (extract_message_jpeg_no_lib(out_jpeg, &extracted, &ex_len) != 0) {
        fprintf(stderr, "Erro ao extrair mensagem de '%s'\n", out_jpeg);
        return 1;
    }

    printf("Mensagem extraída (%zu bytes):\n%.*s\n",
           ex_len, (int)ex_len, extracted);
    free(extracted);

    return 0;
}