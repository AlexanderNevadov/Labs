#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LZW_DICT_SIZE 65536
#define LZW_INIT_DICT 256
#define HASH_SIZE 131072

typedef struct {
    int prefix;
    unsigned char symbol;
} lzw_entry;

lzw_entry dict[LZW_DICT_SIZE];
int next_code;

typedef struct {
    int used;
    int key;
    int code;
} hash_entry;

hash_entry hash_table[HASH_SIZE];

unsigned int hash_func(int key) {
    return (unsigned int)key % HASH_SIZE;
}

void hash_insert(int prefix, unsigned char symbol, int code) {
    if (next_code >= LZW_DICT_SIZE) return;
    int key = (prefix << 8) | symbol;
    unsigned int h = hash_func(key);
    int attempts = 0;
    while (hash_table[h].used && attempts < HASH_SIZE) {
        h = (h + 1) % HASH_SIZE;
        attempts++;
    }
    if (!hash_table[h].used) {
        hash_table[h].used = 1;
        hash_table[h].key = key;
        hash_table[h].code = code;
    }
}

int hash_lookup(int prefix, unsigned char symbol) {
    int key = (prefix << 8) | symbol;
    unsigned int h = hash_func(key);
    int attempts = 0;
    while (hash_table[h].used && attempts < HASH_SIZE) {
        if (hash_table[h].key == key)
            return hash_table[h].code;
        h = (h + 1) % HASH_SIZE;
        attempts++;
    }
    return -1;
}

void hash_clear() {
    memset(hash_table, 0, sizeof(hash_table));
}

void init_dict() {
    for (int i = 0; i < LZW_INIT_DICT; i++) {
        dict[i].prefix = -1;
        dict[i].symbol = (unsigned char)i;
    }
    next_code = LZW_INIT_DICT;
    hash_clear();
    for (int i = 0; i < LZW_INIT_DICT; i++) {
        hash_insert(-1, (unsigned char)i, i);
    }
}

void lzw_encode(const char* infile, const char* outfile) {
    FILE* in = fopen(infile, "rb");
    if (!in) { perror("input file"); return; }
    FILE* out = fopen(outfile, "wb");
    if (!out) { perror("output file"); fclose(in); return; }

    init_dict();

    int prefix = -1;
    unsigned char c;
    long total_bytes = 0;

    while (fread(&c, 1, 1, in) == 1) {
        total_bytes++;
        int code = hash_lookup(prefix, c);
        if (code != -1) {
            prefix = code;
        }
        else {
            unsigned short out_code = (unsigned short)prefix;
            fwrite(&out_code, 2, 1, out);
            if (next_code < LZW_DICT_SIZE) {
                dict[next_code].prefix = prefix;
                dict[next_code].symbol = c;
                hash_insert(prefix, c, next_code);
                next_code++;
            }
            prefix = (int)c;
        }
    }

    if (prefix != -1) {
        unsigned short out_code = (unsigned short)prefix;
        fwrite(&out_code, 2, 1, out);
    }

    fclose(in);
    fclose(out);
}

void lzw_decode(const char* infile, const char* outfile) {
    FILE* in = fopen(infile, "rb");
    if (!in) { perror("input file"); return; }
    FILE* out = fopen(outfile, "wb");
    if (!out) { perror("output file"); fclose(in); return; }

    for (int i = 0; i < LZW_INIT_DICT; i++) {
        dict[i].prefix = -1;
        dict[i].symbol = (unsigned char)i;
    }
    next_code = LZW_INIT_DICT;

    unsigned short prev_code, curr_code;
    if (fread(&prev_code, 2, 1, in) != 1) {
        fclose(in); fclose(out);
        return;
    }

    unsigned char first_sym = (unsigned char)prev_code;
    fwrite(&first_sym, 1, 1, out);

    unsigned char* stack = (unsigned char*)malloc(65536 * sizeof(unsigned char));
    if (!stack) { fclose(in); fclose(out); return; }

    while (fread(&curr_code, 2, 1, in) == 1) {
        int code = curr_code;
        int sp = 0;

        if (code == next_code) {
            int p = prev_code;
            while (p != -1) {
                stack[sp++] = dict[p].symbol;
                p = dict[p].prefix;
            }
            for (int i = sp - 1; i >= 0; i--) {
                fwrite(&stack[i], 1, 1, out);
            }
            unsigned char first = stack[sp - 1];
            fwrite(&first, 1, 1, out);

            if (next_code < LZW_DICT_SIZE) {
                dict[next_code].prefix = prev_code;
                dict[next_code].symbol = first;
                next_code++;
            }
        }
        else {
            int p = code;
            while (p != -1) {
                stack[sp++] = dict[p].symbol;
                p = dict[p].prefix;
            }
            for (int i = sp - 1; i >= 0; i--) {
                fwrite(&stack[i], 1, 1, out);
            }
            if (next_code < LZW_DICT_SIZE) {
                dict[next_code].prefix = prev_code;
                dict[next_code].symbol = stack[sp - 1];
                next_code++;
            }
        }
        prev_code = curr_code;
    }

    free(stack);
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
        lzw_encode(infile, outfile);
    }
    else if (strcmp(mode, "decode") == 0) {
        lzw_decode(infile, outfile);
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
