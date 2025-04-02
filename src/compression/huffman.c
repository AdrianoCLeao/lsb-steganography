#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../../include/compression/huffman.h"
#include "../../include/decompression/huffman.h"

RunLength* encode_run_lengths(const uint8_t *code_lengths, int total, int *out_count) {
    RunLength *rle = malloc(sizeof(RunLength) * total * 2);

    if (!rle) return NULL;

    int rle_len = 0;

    int prev = -1;
    for (int i = 0; i < total;) {
        int curr = code_lengths[i];
        int run = 1;

        while (i + run < total && code_lengths[i + run] == curr)
            run++;

        int actual_run = run; 

        if (curr == 0) {
            while (run > 0) {
                if (run < 3) {
                    rle[rle_len++] = (RunLength){0, 1};
                    run--;
                } else if (run <= 10) {
                    rle[rle_len++] = (RunLength){17, run};
                    run = 0;
                } else {
                    int len = (run > 138) ? 138 : run;
                    rle[rle_len++] = (RunLength){18, len};
                    run -= len;
                }
            }
        } else {
            rle[rle_len++] = (RunLength){curr, 1};
            run--;

            while (run > 0) {
                int len = (run > 6) ? 6 : run;
                if (len >= 3) {
                    rle[rle_len++] = (RunLength){16, len};
                    run -= len;
                } else {
                    for (int j = 0; j < len; j++) {
                        rle[rle_len++] = (RunLength){curr, 1};
                    }
                    run = 0;
                }
            }
        }

        i += actual_run;
    }

    *out_count = rle_len;
    return rle;
}


void count_code_length_frequencies(RunLength *rle, int rle_len, int *cl_freq) {
    memset(cl_freq, 0, 19 * sizeof(int));
    for (int i = 0; i < rle_len; i++) {
        int symbol = rle[i].symbol;
        cl_freq[symbol]++;
    }
}

static void count_frequencies(const uint8_t *data, size_t len, int *litlen_freq, int *dist_freq) {
    memset(litlen_freq, 0, 288 * sizeof(int));
    memset(dist_freq, 0, 32 * sizeof(int));
    for (size_t i = 0; i < len; i++) {
        litlen_freq[data[i]]++;
    }
    litlen_freq[256] = 1; 
}

static void generate_codes_recursive(HuffmanNode *node, HuffmanCode *codes, uint32_t current_code, int depth) {
    if (!node) return;
    if (!node->left && !node->right && node->symbol >= 0) {
        codes[node->symbol].code = current_code;
        codes[node->symbol].length = depth;
        return;
    }
    generate_codes_recursive(node->left, codes, (current_code << 1), depth + 1);
    generate_codes_recursive(node->right, codes, (current_code << 1) | 1, depth + 1);
}

static void generate_huffman_codes(HuffmanTree *tree, HuffmanCode *codes, int num_symbols) {
    for (int i = 0; i < num_symbols; i++) {
        codes[i].code = 0;
        codes[i].length = 0;
    }
    generate_codes_recursive(tree->root, codes, 0, 0);
}

StatusCode compress_huffman_fixed(const uint8_t *input, size_t input_len, BitWriter *bw) {
    if (!input || !bw) return STATUS_NULL_POINTER;

    HuffmanCode codes[288];
    for (int i = 0; i <= 143; i++) {
        codes[i].code = 0x30 + i;
        codes[i].length = 8;
    }
    for (int i = 144; i <= 255; i++) {
        codes[i].code = 0x190 + (i - 144);
        codes[i].length = 9;
    }
    for (int i = 256; i <= 279; i++) {
        codes[i].code = i - 256;
        codes[i].length = 7;
    }
    for (int i = 280; i <= 287; i++) {
        codes[i].code = 0xC0 + (i - 280);
        codes[i].length = 8;
    }

    HuffmanCode dist_codes[32];
    for (int i = 0; i < 32; i++) {
        dist_codes[i].code = i;
        dist_codes[i].length = 5;
    }

    for (size_t i = 0; i < input_len; i++) {
        int sym = input[i];
        bitwriter_write_bits(bw, codes[sym].code, codes[sym].length);
    }

    printf("[compress_huffman_fixed] bytes até aqui: %zu\n", bw->size);
    bitwriter_write_bits(bw, codes[256].code, codes[256].length); 
    bitwriter_align_to_byte(bw);
    printf("[debug] bit_count final: %d\n", bw->bit_count);

    return STATUS_OK;
}

StatusCode compress_huffman_dynamic(const uint8_t *input, size_t input_len, BitWriter *bw) {
    int litlen_freq[288], dist_freq[32];
    count_frequencies(input, input_len, litlen_freq, dist_freq);

    int cl_order[19] = {
        16, 17, 18, 0, 8, 7, 9, 6,
        10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };

    uint8_t litlen_lengths[288] = {0}, dist_lengths[32] = {0};
    for (int i = 0; i < 288; i++) litlen_lengths[i] = (litlen_freq[i] > 0) ? 8 : 0;
    for (int i = 0; i < 32; i++) dist_lengths[i] = (dist_freq[i] > 0) ? 5 : 0;
    dist_lengths[0] = 5;

    HuffmanTree litlen_tree, dist_tree;
    if (build_huffman_tree(litlen_lengths, 288, &litlen_tree) != STATUS_OK) return STATUS_INVALID_FORMAT;
    if (build_huffman_tree(dist_lengths, 32, &dist_tree) != STATUS_OK) {
        free_huffman_tree(&litlen_tree);
        return STATUS_INVALID_FORMAT;
    }

    HuffmanCode litlen_codes[288], dist_codes[32];
    generate_huffman_codes(&litlen_tree, litlen_codes, 288);
    generate_huffman_codes(&dist_tree, dist_codes, 32);

    uint8_t combined[288 + 32];

    int HLIT = 257;
    int HDIST = 1;

    memcpy(combined, litlen_lengths, HLIT);
    memcpy(combined + HLIT, dist_lengths, HDIST);
    int total = HLIT + HDIST;

    int rle_len = 0;
    RunLength *rle = encode_run_lengths(combined, total, &rle_len);

    int covered = 0;
    for (int i = 0; i < rle_len; i++) {
        covered += (rle[i].symbol == 16) ? rle[i].count : 
                    (rle[i].symbol == 17) ? rle[i].count :
                    (rle[i].symbol == 18) ? rle[i].count :
                    1;
    }
    printf("[debug] RLE cobre %d comprimentos\n", covered);

    int cl_freq[19] = {0};
    count_code_length_frequencies(rle, rle_len, cl_freq);

    uint8_t cl_lengths_linear[19] = {0};
    for (int i = 0; i < 19; i++) {
        int sym = cl_order[i];
        cl_lengths_linear[sym] = (cl_freq[sym] > 0) ? 4 : 0;
    }

    HuffmanTree cl_tree;
    if (build_huffman_tree(cl_lengths_linear, 19, &cl_tree) != STATUS_OK) {
        free(rle);
        free_huffman_tree(&litlen_tree);
        free_huffman_tree(&dist_tree);
        return STATUS_INVALID_FORMAT;
    }

    HuffmanCode cl_codes[19];
    generate_huffman_codes(&cl_tree, cl_codes, 19);

    for (int i = 0; i < 19; i++) {
        cl_lengths_linear[i] = cl_codes[i].length;
    }

    int hclen = 19;
    while (hclen > 4 && cl_lengths_linear[cl_order[hclen - 1]] == 0)
        hclen--;

    bitwriter_write_bits(bw, HLIT - 257, 5);         
    bitwriter_write_bits(bw, HDIST - 1, 5);         
    bitwriter_write_bits(bw, hclen - 4, 4);       

    for (int i = 0; i < hclen; i++) {
        bitwriter_write_bits(bw, cl_lengths_linear[cl_order[i]], 3);
    }

    for (int i = 0; i < rle_len; i++) {
        int sym = rle[i].symbol;
        if (cl_codes[sym].length == 0) {
            fprintf(stderr, "Erro: símbolo %d (%d repetições) não tem código válido na cl_tree!\n", sym, rle[i].count);
            free(rle);
            free_huffman_tree(&litlen_tree);
            free_huffman_tree(&dist_tree);
            free_huffman_tree(&cl_tree);
            return STATUS_INVALID_FORMAT;
        }
    }    

    for (int i = 0; i < rle_len; i++) {
        int sym = rle[i].symbol;
        bitwriter_write_bits(bw, cl_codes[sym].code, cl_codes[sym].length);

        if (sym < 0 || sym > 18) {
            fprintf(stderr, "Simbolo invalido na RLE: %d\n", sym);
            return STATUS_INVALID_FORMAT;
        }

        if (sym == 16) {
            if (rle[i].count < 3 || rle[i].count > 6) return STATUS_INVALID_FORMAT;
            bitwriter_write_bits(bw, rle[i].count - 3, 2);
        } else if (sym == 17) {
            if (rle[i].count < 3 || rle[i].count > 10) return STATUS_INVALID_FORMAT;
            bitwriter_write_bits(bw, rle[i].count - 3, 3);
        } else if (sym == 18) {
            if (rle[i].count < 11 || rle[i].count > 138) return STATUS_INVALID_FORMAT;
            bitwriter_write_bits(bw, rle[i].count - 11, 7);
        }        
    }

    printf("[debug] Escrevendo RLE com %d símbolos\n", rle_len);

    printf("[debug] RLE symbols: ");
    for (int i = 0; i < rle_len; i++) {
        int sym = rle[i].symbol;
        printf(">> sym=%d, count=%d | code=0x%X, len=%d", 
            sym, rle[i].count, cl_codes[sym].code, cl_codes[sym].length);
        
        if (sym == 16) {
            printf(" +extra=%d (2 bits)\n", rle[i].count - 3);
        } else if (sym == 17) {
            printf(" +extra=%d (3 bits)\n", rle[i].count - 3);
        } else if (sym == 18) {
            printf(" +extra=%d (7 bits)\n", rle[i].count - 11);
        } else {
            printf("\n");
        }
    }
    printf("\n");

    for (size_t i = 0; i < input_len; i++) {
        bitwriter_write_bits(bw, litlen_codes[input[i]].code, litlen_codes[input[i]].length);
    }

    bitwriter_write_bits(bw, litlen_codes[256].code, litlen_codes[256].length);

    free(rle);
    free_huffman_tree(&litlen_tree);
    free_huffman_tree(&dist_tree);
    free_huffman_tree(&cl_tree);
    return STATUS_OK;
}

StatusCode compress_huffman_block(const uint8_t *input, size_t input_len, BitWriter *bw) {
    if (!input || !bw) return STATUS_NULL_POINTER;

    int freq[288] = {0};
    int unique = 0;

    for (size_t i = 0; i < input_len; i++) {
        if (freq[input[i]] == 0) unique++;
        freq[input[i]]++;
    }
    if (freq[256] == 0) unique++;

    bitwriter_write_bits(bw, 0x78, 8);
    bitwriter_write_bits(bw, 0x9C, 8);

    bitwriter_write_bits(bw, 1, 1); 

    if (unique > 128) {
        bitwriter_write_bits(bw, 0b10, 2); 
        return compress_huffman_dynamic(input, input_len, bw);
    } else {
        bitwriter_write_bits(bw, 0b01, 2); 
        return compress_huffman_fixed(input, input_len, bw);
    }
}
