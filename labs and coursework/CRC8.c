#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint8_t crc8_gsm_b(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    uint8_t poly = 0x49;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ poly;
            else
                crc = crc << 1;
        }
    }
    return crc ^ 0xFF;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    uint8_t* data = malloc(size);
    if (!data) { fclose(f); return 1; }
    fread(data, 1, size, f);
    fclose(f);
    uint8_t crc = crc8_gsm_b(data, size);
    free(data);
    printf("CRC-8/GSM-B for '%s':\n", argv[1]);
    printf("  decimal: %u\n", crc);
    printf("  hexadecimal: 0x%02X\n", crc);
    printf("  binary: ");
    for (int i = 7; i >= 0; i--) putchar((crc >> i) & 1 ? '1' : '0');
    putchar('\n');
    printf("  char: %c\n", (crc >= 32 && crc <= 126) ? crc : '.');
    return 0;
}
