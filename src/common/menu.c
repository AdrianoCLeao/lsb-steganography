#include "../../include/common/menu.h"
#include "../../include/common/utils.h"
#include "../../include/image/jpeg.h"
#include "../../include/image/image.h"
#include "../../include/string/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>

#ifdef _WIN32
#define CLEAR_CMD "cls"
#else
#define CLEAR_CMD "clear"
#endif

#define MAX_PATH 256
#define MAX_MSG_LEN 1024

static void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

static void pause() {
    printf("\nPressione ENTER para continuar...");
    getchar();
}

static void clear_screen() {
    system(CLEAR_CMD);
}

static bool is_jpg(const char *filename) {
    bool result;
    ends_with(filename, ".jpg", &result);
    if (!result) ends_with(filename, ".jpeg", &result);
    return result;
}

static bool is_bmp(const char *filename) {
    bool result;
    ends_with(filename, ".bmp", &result);
    return result;
}

static StatusCode read_message_interactive(uint8_t **buffer, size_t *len) {
    int opt;

    while (1) {
        printf("\nEscolha como deseja fornecer a mensagem:\n");
        printf("1. Usar uma mensagem pré-pronta (assets/messages/)\n");
        printf("2. Digitar a mensagem\n");
        printf("3. Passar o caminho de um arquivo .txt\n");
        printf("0. Voltar\n");
        printf("Escolha: ");
        scanf("%d", &opt);
        clear_input_buffer();
        clear_screen();

        if (opt == 0) return STATUS_ERROR;

        if (opt == 1) {
            DIR *dir = opendir("assets/messages");
            if (!dir) {
                printf("Erro ao abrir pasta de mensagens.\n");
                return STATUS_IO_ERROR;
            }
            struct dirent *entry;
            int count = 0;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_REG) {
                    printf("[%d] %s\n", count + 1, entry->d_name);
                    count++;
                }
            }
            closedir(dir);

            if (count == 0) {
                printf("Nenhuma mensagem encontrada.\n");
                return STATUS_FILE_NOT_FOUND;
            }

            printf("Digite o nome do arquivo da mensagem: ");
            char filename[MAX_PATH];
            fgets(filename, MAX_PATH, stdin);
            trim(filename);

            char fullpath[MAX_PATH];
            snprintf(fullpath, MAX_PATH, "assets/messages/%s", filename);
            return read_text_file(fullpath, buffer, len);
        }

        if (opt == 2) {
            char input[MAX_MSG_LEN];
            printf("Digite sua mensagem (max %d caracteres):\n", MAX_MSG_LEN - 1);
            fgets(input, MAX_MSG_LEN, stdin);
            trim(input);
            *len = strlen(input) + 1;
            *buffer = malloc(*len);
            if (!*buffer) return STATUS_OUT_OF_MEMORY;
            memcpy(*buffer, input, *len);
            return STATUS_OK;
        }

        if (opt == 3) {
            char path[MAX_PATH];
            printf("Digite o caminho para o arquivo de texto: ");
            fgets(path, MAX_PATH, stdin);
            trim(path);
            return read_text_file(path, buffer, len);
        }

        printf("Opção inválida.\n");
    }
}

StatusCode interactive_menu(void) {
    while (1) {
        int escolha;
        clear_screen();
        printf("Deseja esteganografar uma imagem já existente nos assets?\n");
        printf("1. Sim (usar 'assets/lena.jpg')\n");
        printf("2. Não (fornecer caminho manualmente)\n");
        printf("0. Sair\n");
        printf("Escolha: ");
        scanf("%d", &escolha);
        clear_input_buffer();
        clear_screen();

        if (escolha == 0) return STATUS_OK;

        char image_path[MAX_PATH];
        if (escolha == 1) {
            strncpy(image_path, "assets/jpg/lena.jpg", MAX_PATH);
        } else if (escolha == 2) {
            printf("Insira o caminho da imagem a ser esteganografada:\n");
            fgets(image_path, MAX_PATH, stdin);
            trim(image_path);
        } else {
            continue;
        }

        clear_screen();

        if (!is_jpg(image_path) && !is_bmp(image_path)) {
            printf("Formato de imagem não suportado. Use apenas .jpg/.jpeg ou .bmp.\n");
            pause();
            continue;
        }

        uint8_t *message = NULL;
        size_t msg_len = 0;
        if (read_message_interactive(&message, &msg_len) != STATUS_OK) {
            pause();
            continue;
        }

        char output_path[MAX_PATH];
        printf("Insira o caminho de saída para salvar a imagem esteganografada:\n");
        fgets(output_path, MAX_PATH, stdin);
        trim(output_path);

        StatusCode code;
        if (is_jpg(image_path)) {
            code = embed_message_jpeg_no_lib(image_path, output_path, message, msg_len);
        } else if (is_bmp(image_path)) {
            RGBImage image;
            code = load_bmp_image(image_path, &image);
            if (code == STATUS_OK) {
                code = embed_message(&image, message, msg_len);
                if (code == STATUS_OK) {
                    code = save_bmp_image(output_path, &image);
                }
                free_rgb_image(&image);
            }
        }

        print_error(code);
        free(message);
        pause();
    }

    return STATUS_OK;
}
