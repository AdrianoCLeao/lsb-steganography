#include <stdio.h>
#include "../../include/common/common.h"

void print_error(StatusCode code) {
    switch (code) {
        case STATUS_OK:
            printf("Sucesso.\n");
            break;
        case STATUS_ERROR:
            printf("Erro desconhecido.\n");
            break;
        case STATUS_FILE_NOT_FOUND:
            printf("Arquivo não encontrado.\n");
            break;
        case STATUS_INVALID_FORMAT:
            printf("Formato de arquivo inválido.\n");
            break;
        case STATUS_OUT_OF_MEMORY:
            printf("Memória insuficiente.\n");
            break;
        case STATUS_INVALID_ARGUMENT:
            printf("Argumento inválido.\n");
            break;
        case STATUS_IO_ERROR:
            printf("Erro de entrada/saída.\n");
            break;
        case STATUS_IMAGE_TOO_SMALL:
            printf("Imagem muito pequena para esconder a mensagem.\n");
            break;
        case STATUS_MESSAGE_TOO_LARGE:
            printf("Mensagem muito grande para ser escondida na imagem.\n");
            break;
        case STATUS_NULL_POINTER:
            printf("Ponteiro nulo passado como argumento.\n");
            break;
        default:
            printf("Código de erro desconhecido: %d\n", code);
    }
}
