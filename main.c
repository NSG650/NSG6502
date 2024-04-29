#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nsg6502.h"

void dump_hex(const void* data, size_t size) {
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';
    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char*)data)[i]);
        if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char*)data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i+1) % 8 == 0 || i+1 == size) {
            printf(" ");
            if ((i+1) % 16 == 0) {
                printf("|  %s \n", ascii);
            } else if (i+1 == size) {
                ascii[(i+1) % 16] = '\0';
                if ((i+1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i+1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("|  %s \n", ascii);
            }
        }
    }
}

void main_memory_write_callback(struct nsg6502_cpu *c, uint16_t addr, uint8_t data) {
    if (addr == 0x0200) {
        printf("PUTCHAR: %c\n", data);
    }
    else {
        c->memory[addr] = data;
    }
}

int main(void) {
    struct nsg6502_cpu cpu = {0};
    cpu.memory = malloc(0xFFFF);
    memset(cpu.memory, 0, 0xFFFF);
    cpu.memory_write_callback = main_memory_write_callback;

    char code[] = "\x29\x00\x69\x41\x8d\x00\x02\x69\x01\xc9\x5b\xd0\xf7\x60";
    memcpy(&cpu.memory[0x0600], code, sizeof(code));

    cpu.memory[0xFFFC] = 0x00;
    cpu.memory[0xFFFD] = 0x06;

    nsg6502_reset(&cpu);

    while (cpu.pc != 0x60e - 1) { // Lazy way lol
        nsg6502_opcode_execute(&cpu);
    }

    free(cpu.memory);

    return 0;
}
