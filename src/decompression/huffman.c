#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../../include/decompression/huffman.h"

static HuffmanNode* create_node() {
    HuffmanNode *node = malloc(sizeof(HuffmanNode));
    if (node) {
        node->symbol = -1;
        node->left = node->right = NULL;
    }
    return node;
}

static void insert_code(HuffmanNode *root, int symbol, uint32_t code, int length) {
    HuffmanNode *node = root;
    for (int i = length - 1; i >= 0; i--) {
        int bit = (code >> i) & 1;
        if (bit == 0) {
            if (!node->left) node->left = create_node();
            node = node->left;
        } else {
            if (!node->right) node->right = create_node();
            node = node->right;
        }
    }
    node->symbol = symbol;
}

StatusCode build_huffman_tree(const uint8_t *code_lengths, int num_symbols, HuffmanTree *tree) {
    int bl_count[16] = {0};
    int next_code[16] = {0};

    for (int i = 0; i < num_symbols; i++) {
        if (code_lengths[i] > 15) return STATUS_INVALID_FORMAT;
        bl_count[code_lengths[i]]++;
    }

    int code = 0;
    for (int bits = 1; bits <= 15; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }

    tree->root = create_node();
    if (!tree->root) return STATUS_OUT_OF_MEMORY;

    for (int i = 0; i < num_symbols; i++) {
        int len = code_lengths[i];
        if (len != 0) {
            insert_code(tree->root, i, next_code[len], len);
            next_code[len]++;
        }
    }

    return STATUS_OK;
}

void free_huffman_nodes(HuffmanNode *node) {
    if (!node) return;
    free_huffman_nodes(node->left);
    free_huffman_nodes(node->right);
    free(node);
}

void free_huffman_tree(HuffmanTree *tree) {
    if (!tree || !tree->root) return;
    free_huffman_nodes(tree->root);
    tree->root = NULL;
}

int huffman_read_symbol(BitStream *bs, const HuffmanTree *tree) {
    HuffmanNode *node = tree->root;
    while (node && (node->left || node->right)) {
        int bit = bitstream_read_bits(bs, 1);
        node = bit == 0 ? node->left : node->right;
    }
    return node ? node->symbol : -1;
}

StatusCode inflate_fixed_huffman(BitStream *bs, uint8_t **out, size_t *out_len) {
    uint8_t litlen_lengths[288], dist_lengths[32];

    for (int i = 0; i <= 143; i++) litlen_lengths[i] = 8;
    for (int i = 144; i <= 255; i++) litlen_lengths[i] = 9;
    for (int i = 256; i <= 279; i++) litlen_lengths[i] = 7;
    for (int i = 280; i <= 287; i++) litlen_lengths[i] = 8;

    for (int i = 0; i < 32; i++) dist_lengths[i] = 5;

    HuffmanTree litlen_tree, dist_tree;
    StatusCode code;

    code = build_huffman_tree(litlen_lengths, 288, &litlen_tree);
    if (code != STATUS_OK) return code;

    code = build_huffman_tree(dist_lengths, 32, &dist_tree);
    if (code != STATUS_OK) {
        free_huffman_tree(&litlen_tree);
        return code;
    }

    size_t capacity = 8192;
    size_t size = 0;
    uint8_t *buffer = malloc(capacity);
    if (!buffer) return STATUS_OUT_OF_MEMORY;

    while (1) {
        int sym = huffman_read_symbol(bs, &litlen_tree);
        if (sym < 0) {
            printf("[inflate_fixed_huffman] símbolo inválido\n");
            code = STATUS_INVALID_FORMAT;
            break;
        }

        if (sym < 256) {
            if (size >= capacity) {
                capacity *= 2;
                buffer = realloc(buffer, capacity);
                if (!buffer) {
                    code = STATUS_OUT_OF_MEMORY;
                    break;
                }
            }
            buffer[size++] = (uint8_t)sym;
        } else if (sym == 256) {
            code = STATUS_OK;
            break;
        } else if (sym <= 285) {
            int length_code = sym - 257;
            int length = LENGTH_BASE[length_code];
            int extra = LENGTH_EXTRA[length_code];
            if (extra > 0) length += bitstream_read_bits(bs, extra);

            int dist_code = huffman_read_symbol(bs, &dist_tree);
            if (dist_code < 0 || dist_code >= 30) {
                printf("[inflate_fixed_huffman] dist_code inválido: %d\n", dist_code);
                code = STATUS_INVALID_FORMAT;
                break;
            }

            int distance = DIST_BASE[dist_code];
            int extra_dist = DIST_EXTRA[dist_code];
            if (extra_dist > 0) distance += bitstream_read_bits(bs, extra_dist);

            if (distance > size) {
                printf("[inflate_fixed_huffman] distância maior que dados disponíveis\n");
                code = STATUS_INVALID_FORMAT;
                break;
            }

            if (size + length > capacity) {
                capacity = (size + length) * 2;
                buffer = realloc(buffer, capacity);
                if (!buffer) {
                    code = STATUS_OUT_OF_MEMORY;
                    break;
                }
            }

            for (int i = 0; i < length; i++) {
                buffer[size] = buffer[size - distance];
                size++;
            }
        } else {
            printf("[inflate_fixed_huffman] símbolo fora do intervalo válido\n");
            code = STATUS_INVALID_FORMAT;
            break;
        }
    }

    free_huffman_tree(&litlen_tree);
    free_huffman_tree(&dist_tree);

    if (code == STATUS_OK) {
        *out = buffer;
        *out_len = size;
    } else {
        free(buffer);
    }

    return code;
}

StatusCode inflate_dynamic_huffman(BitStream *bs, uint8_t **buffer, size_t *size, size_t *capacity) {
    int HLIT = bitstream_read_bits(bs, 5);
    if (HLIT < 0) return STATUS_INVALID_FORMAT;
    HLIT += 257;

    int HDIST = bitstream_read_bits(bs, 5);
    if (HDIST < 0) return STATUS_INVALID_FORMAT;
    HDIST += 1;

    int HCLEN = bitstream_read_bits(bs, 4);
    if (HCLEN < 0) return STATUS_INVALID_FORMAT;
    HCLEN += 4;

    printf("[inflate_dynamic_huffman] HLIT=%d, HDIST=%d, HCLEN=%d\n", HLIT, HDIST, HCLEN);

    if (HLIT > 286 || HDIST > 30) {
        printf("[inflate_dynamic_huffman] HLIT ou HDIST fora do limite\n");
        return STATUS_INVALID_FORMAT;
    }

    static const int order[19] = {
        16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
    };

    uint8_t clens[19] = {0};
    for (int i = 0; i < HCLEN; i++) {
        int val = bitstream_read_bits(bs, 3);
        if (val < 0) return STATUS_INVALID_FORMAT;
        clens[order[i]] = val;
    }

    HuffmanTree cl_tree;
    memset(&cl_tree, 0, sizeof(cl_tree));
    StatusCode code = build_huffman_tree(clens, 19, &cl_tree);
    if (code != STATUS_OK) return code;

    uint8_t *all_lengths = malloc(HLIT + HDIST);
    if (!all_lengths) {
        free_huffman_tree(&cl_tree);
        return STATUS_OUT_OF_MEMORY;
    }

    int i = 0;
    while (i < HLIT + HDIST) {
        int sym = huffman_read_symbol(bs, &cl_tree);
        if (sym < 0) goto invalid;

        if (sym <= 15) {
            all_lengths[i++] = sym;
        } else if (sym == 16) {
            if (i == 0) goto invalid;
            int repeat = bitstream_read_bits(bs, 2);
            if (repeat < 0) goto invalid;
            repeat += 3;
            if (i + repeat > HLIT + HDIST) goto invalid;
            uint8_t val = all_lengths[i - 1];
            for (int j = 0; j < repeat; j++) all_lengths[i++] = val;
        } else if (sym == 17) {
            int repeat = bitstream_read_bits(bs, 3);
            if (repeat < 0) goto invalid;
            repeat += 3;
            if (i + repeat > HLIT + HDIST) goto invalid;
            for (int j = 0; j < repeat; j++) all_lengths[i++] = 0;
        } else if (sym == 18) {
            int repeat = bitstream_read_bits(bs, 7);
            if (repeat < 0) goto invalid;
            repeat += 11;
            if (i + repeat > HLIT + HDIST) goto invalid;
            for (int j = 0; j < repeat; j++) all_lengths[i++] = 0;
        }
    }

    for (int k = 0; k < HLIT + HDIST; k++) {
        if (all_lengths[k] > 15) goto invalid;
    }

    HuffmanTree litlen_tree, dist_tree;
    memset(&litlen_tree, 0, sizeof(litlen_tree));
    memset(&dist_tree, 0, sizeof(dist_tree));

    code = build_huffman_tree(all_lengths, HLIT, &litlen_tree);
    if (code != STATUS_OK) goto cleanup;

    code = build_huffman_tree(all_lengths + HLIT, HDIST, &dist_tree);
    if (code != STATUS_OK) {
        free_huffman_tree(&litlen_tree);
        goto cleanup;
    }

    int dist_nonzero = 0;
    for (int k = 0; k < HDIST; k++) {
        if (all_lengths[HLIT + k] != 0) dist_nonzero++;
    }
    printf("[inflate_dynamic_huffman] códigos de distância não-zero: %d\n", dist_nonzero);

    while (1) {
        int sym = huffman_read_symbol(bs, &litlen_tree);
        if (sym < 0) {
            code = STATUS_INVALID_FORMAT;
            break;
        }

        if (sym < 256) {
            if (*size >= *capacity) {
                size_t new_cap = (*capacity) * 2;
                uint8_t *tmp = realloc(*buffer, new_cap);
                if (!tmp) { code = STATUS_OUT_OF_MEMORY; break; }
                *buffer = tmp;
                *capacity = new_cap;
            }
            (*buffer)[(*size)++] = (uint8_t)sym;
        } else if (sym == 256) {
            code = STATUS_OK;
            break;
        } else if (sym <= 285) {
            int length_code = sym - 257;
            int length = LENGTH_BASE[length_code];
            int extra = LENGTH_EXTRA[length_code];
            if (extra > 0) {
                int bits = bitstream_read_bits(bs, extra);
                if (bits < 0) goto deflate_error;
                length += bits;
            }

            int dist_code = huffman_read_symbol(bs, &dist_tree);
            if (dist_code < 0 || dist_code > 29) goto deflate_error;

            int distance = DIST_BASE[dist_code];
            int extra_dist = DIST_EXTRA[dist_code];
            if (extra_dist > 0) {
                int bits = bitstream_read_bits(bs, extra_dist);
                if (bits < 0) goto deflate_error;
                distance += bits;
            }

            if ((size_t)distance > *size) {
                printf("[inflate_dynamic_huffman] ERRO: tentando acessar %zu - %d (negativo!)\n", *size, distance);
                code = STATUS_INVALID_FORMAT;
                break;
            }

            if (*size + length > *capacity) {
                size_t new_cap = (*size + length) * 2;
                uint8_t *tmp = realloc(*buffer, new_cap);
                if (!tmp) { code = STATUS_OUT_OF_MEMORY; break; }
                *buffer = tmp;
                *capacity = new_cap;
            }

            printf("[inflate_dynamic_huffman] size atual = %zu, distance = %d\n", *size, distance);
            for (int j = 0; j < length; j++) {
                (*buffer)[*size] = (*buffer)[*size - distance];
                (*size)++;
            }
        } else {
            code = STATUS_INVALID_FORMAT;
            break;
        }
    }

free_trees:
    free_huffman_tree(&litlen_tree);
    free_huffman_tree(&dist_tree);

cleanup:
    free(all_lengths);
    free_huffman_tree(&cl_tree);
    return code;

invalid:
    printf("[inflate_dynamic_huffman] erro ao ler comprimentos (overflow ou símbolo inválido)\n");
    code = STATUS_INVALID_FORMAT;
    goto cleanup;

deflate_error:
    printf("[inflate_dynamic_huffman] erro ao decodificar comprimento/distância\n");
    code = STATUS_INVALID_FORMAT;
    goto free_trees;
}

StatusCode inflate_deflate_blocks(const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len) {
    if (!input || !output || !output_len) return STATUS_NULL_POINTER;

    BitStream bs;
    bitstream_init(&bs, input, input_len);

    bitstream_read_bits(&bs, 8); 
    bitstream_read_bits(&bs, 8); 

    size_t capacity = 8192;
    size_t size = 0;
    uint8_t *buffer = malloc(capacity);
    if (!buffer) return STATUS_OUT_OF_MEMORY;

    int done = 0;

    while (!done && !bitstream_eof(&bs)) {
        uint8_t bfinal = bitstream_read_bits(&bs, 1);
        uint8_t btype  = bitstream_read_bits(&bs, 2);

        uint8_t *block_out = NULL;
        size_t block_len = 0;
        StatusCode code = STATUS_OK;

        if (btype == 0) {
            bitstream_align_to_byte(&bs);

            if (bs.bit_pos + 32 > bs.bit_len) {
                fprintf(stderr, "[inflate_deflate_blocks] Dados insuficientes para bloco não comprimido\n");
                free(buffer);
                return STATUS_INVALID_FORMAT;
            }

            uint16_t len  = bitstream_read_bits(&bs, 16);
            uint16_t nlen = bitstream_read_bits(&bs, 16);
            if ((len ^ 0xFFFF) != nlen) {
                fprintf(stderr, "[inflate_deflate_blocks] LEN e NLEN não são complementares\n");
                free(buffer);
                return STATUS_INVALID_FORMAT;
            }

            if (size + len > capacity) {
                capacity = (size + len) * 2;
                buffer = realloc(buffer, capacity);
                if (!buffer) return STATUS_OUT_OF_MEMORY;
            }

            for (int i = 0; i < len; i++) {
                buffer[size++] = bitstream_read_bits(&bs, 8);
            }
        }
        else if (btype == 1) {
            code = inflate_fixed_huffman(&bs, &block_out, &block_len);
            if (code == STATUS_INVALID_FORMAT)
                fprintf(stderr, "[inflate_deflate_blocks] Erro no bloco com Huffman fixo\n");
        }
        else if (btype == 2) {
            code = inflate_dynamic_huffman(&bs, &buffer, &size, &capacity);
            if (code == STATUS_INVALID_FORMAT)
                fprintf(stderr, "[inflate_deflate_blocks] Erro no bloco com Huffman dinâmico\n");
        }
        else {
            fprintf(stderr, "[inflate_deflate_blocks] Tipo de bloco inválido: %u\n", btype);
            free(buffer);
            return STATUS_INVALID_FORMAT;
        }

        if (code != STATUS_OK) {
            free(buffer);
            return code;
        }

        if (btype == 1 || btype == 2) {
            if (size + block_len > capacity) {
                capacity = (size + block_len) * 2;
                buffer = realloc(buffer, capacity);
                if (!buffer) {
                    free(block_out);
                    return STATUS_OUT_OF_MEMORY;
                }
            }
            memcpy(buffer + size, block_out, block_len);
            size += block_len;
            free(block_out);
        }

        if (bfinal) done = 1;
    }

    *output = buffer;
    *output_len = size;
    return STATUS_OK;
}
