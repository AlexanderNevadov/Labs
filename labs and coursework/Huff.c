#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define _CRT_SECURE_NO_WARNINGS
typedef struct {
    FILE* file;
    unsigned char buffer;
    int bits_in_buffer;
} bitfile;

void bf_open_write(bitfile* bf, FILE* f) {
    bf->file = f;
    bf->buffer = 0;
    bf->bits_in_buffer = 0;
}

void bf_open_read(bitfile* bf, FILE* f) {
    bf->file = f;
    bf->buffer = 0;
    bf->bits_in_buffer = 0;
}

void bf_write_bit(bitfile* bf, int bit) {
    bf->buffer = (bf->buffer << 1) | (bit & 1);
    bf->bits_in_buffer++;
    if (bf->bits_in_buffer == 8) {
        fwrite(&bf->buffer, 1, 1, bf->file);
        bf->buffer = 0;
        bf->bits_in_buffer = 0;
    }
}

void bf_flush(bitfile* bf) {
    if (bf->bits_in_buffer > 0) {
        bf->buffer <<= (8 - bf->bits_in_buffer);
        fwrite(&bf->buffer, 1, 1, bf->file);
        bf->buffer = 0;
        bf->bits_in_buffer = 0;
    }
}

int bf_read_bit(bitfile* bf) {
    if (bf->bits_in_buffer == 0) {
        if (fread(&bf->buffer, 1, 1, bf->file) != 1)
            return -1;
        bf->bits_in_buffer = 8;
    }
    int bit = (bf->buffer >> (bf->bits_in_buffer - 1)) & 1;
    bf->bits_in_buffer--;
    return bit;
}

typedef struct node {
    unsigned char symbol;
    int freq;
    struct node* left, * right;
} node;

typedef struct {
    node** nodes;
    int size;
    int capacity;
} min_heap;

min_heap* heap_create(int cap) {
    min_heap* h = (min_heap*)malloc(sizeof(min_heap));
    h->nodes = (node**)malloc(cap * sizeof(node*));
    h->size = 0;
    h->capacity = cap;
    return h;
}

void heap_swap(node** a, node** b) {
    node* t = *a;
    *a = *b;
    *b = t;
}

void heapify(min_heap* h, int i) {
    int smallest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;
    if (left < h->size && h->nodes[left]->freq < h->nodes[smallest]->freq)
        smallest = left;
    if (right < h->size && h->nodes[right]->freq < h->nodes[smallest]->freq)
        smallest = right;
    if (smallest != i) {
        heap_swap(&h->nodes[i], &h->nodes[smallest]);
        heapify(h, smallest);
    }
}

void heap_push(min_heap* h, node* n) {
    int i = h->size++;
    h->nodes[i] = n;
    while (i > 0 && h->nodes[(i - 1) / 2]->freq > h->nodes[i]->freq) {
        heap_swap(&h->nodes[(i - 1) / 2], &h->nodes[i]);
        i = (i - 1) / 2;
    }
}

node* heap_pop(min_heap* h) {
    if (h->size == 0) return NULL;
    node* top = h->nodes[0];
    h->nodes[0] = h->nodes[--h->size];
    heapify(h, 0);
    return top;
}

node* create_node(unsigned char sym, int freq, node* l, node* r) {
    node* n = (node*)malloc(sizeof(node));
    n->symbol = sym;
    n->freq = freq;
    n->left = l;
    n->right = r;
    return n;
}

void free_tree(node* root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

node* build_tree(int* freq) {
    min_heap* heap = heap_create(256);
    int count = 0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            node* n = create_node((unsigned char)i, freq[i], NULL, NULL);
            heap_push(heap, n);
            count++;
        }
    }
    if (count == 0) return NULL;
    if (count == 1) {
        node* root = heap_pop(heap);
        free(heap->nodes);
        free(heap);
        return root;
    }
    while (heap->size > 1) {
        node* l = heap_pop(heap);
        node* r = heap_pop(heap);
        node* parent = create_node(0, l->freq + r->freq, l, r);
        heap_push(heap, parent);
    }
    node* root = heap_pop(heap);
    free(heap->nodes);
    free(heap);
    return root;
}

void generate_codes(node* n, int* codes, int* lens, int cur_code, int cur_len) {
    if (!n->left && !n->right) {
        codes[n->symbol] = cur_code;
        lens[n->symbol] = cur_len;
        return;
    }
    if (n->left) generate_codes(n->left, codes, lens, (cur_code << 1) | 0, cur_len + 1);
    if (n->right) generate_codes(n->right, codes, lens, (cur_code << 1) | 1, cur_len + 1);
}
void write_code_table(FILE* out, int* codes, int* lens, int total_symbols) {
    uint16_t num = (uint16_t)total_symbols;
    fwrite(&num, sizeof(uint16_t), 1, out);
    for (int i = 0; i < 256; i++) {
        if (lens[i] > 0) {
            unsigned char sym = (unsigned char)i;
            unsigned char len = (unsigned char)lens[i];
            uint16_t code = (uint16_t)codes[i];
            fwrite(&sym, 1, 1, out);
            fwrite(&len, 1, 1, out);
            fwrite(&code, sizeof(uint16_t), 1, out);
        }
    }
}

int read_code_table(FILE* in, int* codes, int* lens) {
    uint16_t num;
    if (fread(&num, sizeof(uint16_t), 1, in) != 1) return 0;
    memset(codes, 0, 256 * sizeof(int));
    memset(lens, 0, 256 * sizeof(int));
    for (int i = 0; i < num; i++) {
        unsigned char sym, len;
        uint16_t code;
        if (fread(&sym, 1, 1, in) != 1) return 0;
        if (fread(&len, 1, 1, in) != 1) return 0;
        if (fread(&code, sizeof(uint16_t), 1, in) != 1) return 0;
        codes[sym] = code;
        lens[sym] = len;
    }
    return num;
}

void huffman_encode(const char* infile, const char* outfile) {
    FILE* in = fopen(infile, "rb");
    if (!in) { perror("input file"); return; }
    FILE* out = fopen(outfile, "wb");
    if (!out) { perror("output file"); fclose(in); return; }

    int freq[256] = { 0 };
    unsigned char buf[4096];
    size_t n;
    uint32_t total_bytes = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        total_bytes += n;
        for (size_t i = 0; i < n; i++)
            freq[buf[i]]++;
    }
    rewind(in);

    node* root = build_tree(freq);
    int codes[256] = { 0 }, lens[256] = { 0 };
    if (root) {
        if (!root->left && !root->right) {
            codes[root->symbol] = 0;
            lens[root->symbol] = 1;
        }
        else {
            generate_codes(root, codes, lens, 0, 0);
        }
    }

    fwrite(&total_bytes, sizeof(uint32_t), 1, out);
    int unique = 0;
    for (int i = 0; i < 256; i++) if (lens[i] > 0) unique++;
    write_code_table(out, codes, lens, unique);

    bitfile bf;
    bf_open_write(&bf, out);
    rewind(in);
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        for (size_t i = 0; i < n; i++) {
            int code = codes[buf[i]];
            int len = lens[buf[i]];
            for (int j = len - 1; j >= 0; j--)
                bf_write_bit(&bf, (code >> j) & 1);
        }
    }
    bf_flush(&bf);

    free_tree(root);
    fclose(in);
    fclose(out);
}

node* build_tree_from_codes(int* codes, int* lens) {
    node* root = create_node(0, 0, NULL, NULL);
    for (int sym = 0; sym < 256; sym++) {
        int len = lens[sym];
        if (len == 0) continue;
        int code = codes[sym];
        node* cur = root;
        for (int j = len - 1; j >= 0; j--) {
            int bit = (code >> j) & 1;
            if (bit == 0) {
                if (!cur->left) cur->left = create_node(0, 0, NULL, NULL);
                cur = cur->left;
            }
            else {
                if (!cur->right) cur->right = create_node(0, 0, NULL, NULL);
                cur = cur->right;
            }
        }
        cur->symbol = (unsigned char)sym;
    }
    return root;
}

void huffman_decode(const char* infile, const char* outfile) {
    FILE* in = fopen(infile, "rb");
    if (!in) { perror("input file"); return; }
    FILE* out = fopen(outfile, "wb");
    if (!out) { perror("output file"); fclose(in); return; }

    uint32_t total_bytes;
    if (fread(&total_bytes, sizeof(uint32_t), 1, in) != 1) {
        fclose(in); fclose(out); return;
    }

    int codes[256] = { 0 }, lens[256] = { 0 };
    int unique = read_code_table(in, codes, lens);
    if (unique == 0) {
        fclose(in); fclose(out); return;
    }

    node* root = build_tree_from_codes(codes, lens);

    bitfile bf;
    bf_open_read(&bf, in);
    long decoded = 0;
    node* cur = root;

    if (!root->left && !root->right) {
        for (uint32_t i = 0; i < total_bytes; i++) {
            fwrite(&root->symbol, 1, 1, out);
        }
        free_tree(root);
        fclose(in);
        fclose(out);
        return;
    }

    while (decoded < total_bytes) {
        int bit = bf_read_bit(&bf);
        if (bit == -1) break;
        cur = (bit == 0) ? cur->left : cur->right;
        if (!cur->left && !cur->right) {
            fwrite(&cur->symbol, 1, 1, out);
            decoded++;
            cur = root;
        }
    }

    free_tree(root);
    fclose(in);
    fclose(out);
}

int main(int argc, char* argv[]) {
    if (argc < 7) {
        fprintf(stderr, "Usage: %s --mode <encode|decode> --input <infile> --output <outfile>\n", argv[0]);
        return 1;
    }

    char* mode = NULL, * infile = NULL, * outfile = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) mode = argv[++i];
        else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) infile = argv[++i];
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) outfile = argv[++i];
    }
    if (!mode || !infile || !outfile) {
        fprintf(stderr, "Missing arguments\n");
        return 1;
    }

    clock_t start = clock();

    if (strcmp(mode, "encode") == 0) {
        huffman_encode(infile, outfile);
    }
    else if (strcmp(mode, "decode") == 0) {
        huffman_decode(infile, outfile);
    }
    else {
        fprintf(stderr, "Unknown mode: %s\n", mode);
        return 1;
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Time: %.3f s\n", elapsed);

    if (strcmp(mode, "encode") == 0) {
        FILE* fin = fopen(infile, "rb");
        FILE* fout = fopen(outfile, "rb");
        if (fin && fout) {
            fseek(fin, 0, SEEK_END);
            fseek(fout, 0, SEEK_END);
            long in_size = ftell(fin);
            long out_size = ftell(fout);
            printf("Input size: %ld bytes\n", in_size);
            printf("Output size: %ld bytes\n", out_size);
            printf("Compression ratio: %.3f\n", (double)out_size / in_size);
            fclose(fin); fclose(fout);
        }
    }

    return 0;
}
