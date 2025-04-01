#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stdint.h>
#include "../bitstream/bitstream.h"
#include "../common/common.h"

typedef struct HuffmanNode {
    int symbol;
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;

typedef struct {
    HuffmanNode *root;
} HuffmanTree;

StatusCode build_huffman_tree(const uint8_t *code_lengths, int num_symbols, HuffmanTree *tree);
void free_huffman_tree(HuffmanTree *tree);
int huffman_read_symbol(BitStream *bs, const HuffmanTree *tree);

StatusCode inflate_fixed_huffman(BitStream *bs, uint8_t **out, size_t *out_len);
StatusCode inflate_dynamic_huffman(BitStream *bs, uint8_t **buffer, size_t *size, size_t *capacity);;

StatusCode inflate_deflate_blocks(const uint8_t *input, size_t input_len, uint8_t **output, size_t *output_len);

#endif
