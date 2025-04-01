#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../../include/compression/huffman.h"
#include "../../include/decompression/huffman.h"

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

    bitwriter_write_bits(bw, 1, 1);  
    bitwriter_write_bits(bw, 1, 2); 

    for (size_t i = 0; i < input_len; i++) {
        int sym = input[i];
        bitwriter_write_bits(bw, codes[sym].code, codes[sym].length);
    }

    bitwriter_write_bits(bw, codes[256].code, codes[256].length); 

    return STATUS_OK;
}

StatusCode compress_huffman_dynamic(const uint8_t *input, size_t input_len, BitWriter *bw) {
    int litlen_freq[288], dist_freq[32];
    count_frequencies(input, input_len, litlen_freq, dist_freq);

    uint8_t litlen_lengths[288] = {0}, dist_lengths[32] = {0};
    for (int i = 0; i < 288; i++) litlen_lengths[i] = (litlen_freq[i] > 0) ? 8 : 0;
    for (int i = 0; i < 32; i++) dist_lengths[i] = (dist_freq[i] > 0) ? 5 : 0;

    HuffmanTree litlen_tree, dist_tree;
    if (build_huffman_tree(litlen_lengths, 288, &litlen_tree) != STATUS_OK) return STATUS_INVALID_FORMAT;
    if (build_huffman_tree(dist_lengths, 32, &dist_tree) != STATUS_OK) {
        free_huffman_tree(&litlen_tree);
        return STATUS_INVALID_FORMAT;
    }

    HuffmanCode litlen_codes[288], dist_codes[32];
    generate_huffman_codes(&litlen_tree, litlen_codes, 288);
    generate_huffman_codes(&dist_tree, dist_codes, 32);

    bitwriter_write_bits(bw, 1, 1); 
    bitwriter_write_bits(bw, 2, 2); 

    bitwriter_write_bits(bw, 257 - 257, 5); 
    bitwriter_write_bits(bw, 1 - 1, 5);     
    bitwriter_write_bits(bw, 4 - 4, 4);     

    for (int i = 0; i < 4; i++) bitwriter_write_bits(bw, 3, 3);

    for (int i = 0; i < 288; i++) bitwriter_write_bits(bw, litlen_lengths[i], 3);

    for (int i = 0; i < 32; i++) bitwriter_write_bits(bw, dist_lengths[i], 3);

    for (size_t i = 0; i < input_len; i++) {
        bitwriter_write_bits(bw, litlen_codes[input[i]].code, litlen_codes[input[i]].length);
    }

    bitwriter_write_bits(bw, litlen_codes[256].code, litlen_codes[256].length);

    free_huffman_tree(&litlen_tree);
    free_huffman_tree(&dist_tree);
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

    if (unique > 128) {
        return compress_huffman_dynamic(input, input_len, bw);
    } else {
        return compress_huffman_fixed(input, input_len, bw);
    }
}
