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
