#include <stdio.h>

#include "../../include/common/common.h"
#include "../../include/common/utils.h"

void print_error(StatusCode code) {
    switch (code) {
        case STATUS_OK:
            printf("Sucesso.\n");
            break;
        case STATUS_ERROR:
            printf("Erro desconhecido.\n");
            break;
        case STATUS_FILE_NOT_FOUND:
            printf("Arquivo nao encontrado.\n");
            break;
        case STATUS_INVALID_FORMAT:
            printf("Formato de arquivo invalido.\n");
            break;
        case STATUS_OUT_OF_MEMORY:
            printf("Memoria insuficiente.\n");
            break;
        case STATUS_INVALID_ARGUMENT:
            printf("Argumento invalido.\n");
            break;
        case STATUS_IO_ERROR:
            printf("Erro de entrada/saida.\n");
            break;
        case STATUS_MESSAGE_TOO_LARGE:
            printf("Mensagem muito grande para ser escondida na imagem.\n");
            break;
        case STATUS_NULL_POINTER:
            printf("Ponteiro nulo passado como argumento.\n");
            break;
        case STATUS_NOT_IMPLEMENTED:
            printf("Esta funcionalidade ainda nao foi implementada.\n");
            break;
        default:
            printf("Codigo de erro desconhecido: %d\n", code);
    }
}

int read_text_file(const char *path, uint8_t **out_buf, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -2; }
    long len = ftell(f);
    if (len < 0)        { fclose(f); return -3; }
    rewind(f);
    *out_buf = malloc((size_t)len);
    if (!*out_buf)      { fclose(f); return -4; }
    if (fread(*out_buf, 1, (size_t)len, f) != (size_t)len) {
        free(*out_buf);
        fclose(f);
        return -5;
    }
    fclose(f);
    *out_len = (size_t)len;
    return 0;
}