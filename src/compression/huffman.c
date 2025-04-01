#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../../include/compression/huffman.h"
#include "../../include/decompression/huffman.h"

static void count_frequencies(const uint8_t *data, size_t len, int *freq_table) {
    memset(freq_table, 0, NUM_SYMBOLS * sizeof(int));
    for (size_t i = 0; i < len; i++) {
        freq_table[data[i]]++;
    }
    freq_table[256] = 1; 
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

StatusCode compress_huffman_block(const uint8_t *input, size_t input_len, BitWriter *bw) {
    int freq[NUM_SYMBOLS];
    count_frequencies(input, input_len, freq);

    uint8_t code_lengths[NUM_SYMBOLS];
    for (int i = 0; i < NUM_SYMBOLS; i++) {
        code_lengths[i] = freq[i] > 0 ? 8 : 0;
    }

    HuffmanTree tree;
    StatusCode code = build_huffman_tree(code_lengths, NUM_SYMBOLS, &tree);
    if (code != STATUS_OK) return code;

    HuffmanCode codes[NUM_SYMBOLS];
    generate_huffman_codes(&tree, codes, NUM_SYMBOLS);

    bitwriter_write_bits(bw, 1, 1);
    bitwriter_write_bits(bw, 1, 2); 

    for (size_t i = 0; i < input_len; i++) {
        int sym = input[i];
        bitwriter_write_bits(bw, codes[sym].code, codes[sym].length);
    }

    bitwriter_write_bits(bw, codes[256].code, codes[256].length);

    free_huffman_tree(&tree);
    return STATUS_OK;
}
