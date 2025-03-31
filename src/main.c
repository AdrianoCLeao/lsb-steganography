#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/image/image.h"
#include "../include/image/png.h"
#include "../include/common/utils.h"

int main() {
    PNGImage image;
    StatusCode code;

    const char *input_image = "assets/teste.png";  

    code = load_png_image(input_image, &image);
    if (code != STATUS_OK) {
        print_error(code);
        return 1;
    }

    printf("Metadados da imagem carregados com sucesso.\n");
    return 0;
}

