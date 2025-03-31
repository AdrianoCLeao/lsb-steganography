#include <stdio.h>
#include <stdbool.h>
#include "../include/image/image.h"
#include "../include/common/utils.h"

int main() {
    RGBImage img;
    StatusCode code = load_bmp_image("lena.bmp", &img);
    if (code != STATUS_OK) {
        print_error(code);
        return 1;
    }

    printf("Image loaded: %dx%d\n", img.width, img.height);

    print_pixel(&img, 0, 0); 
    printf("\nLSB da matriz RED (parcial):\n");
    print_lsb_matrix(img.red, img.width, img.height);

    free_rgb_image(&img);
    return 0;
}
