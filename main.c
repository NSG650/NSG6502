#include <stdio.h>
#include <stdlib.h>

#include "nsg6502.h"

void main_memory_write_callback(struct nsg6502_cpu *c, uint16_t addr, uint8_t data) {
    if (addr == 0) {
        printf("PUTCHAR: %c\n", data);
    }
    else {
        c->memory[addr] = data;
    }
}

int main(void) {
    struct nsg6502_cpu cpu = {0};
    cpu.memory = malloc(0xFFFF);
    cpu.memory_write_callback = main_memory_write_callback;

    nsg6502_reset(&cpu);

    cpu.memory[0xFCE2] = 0xA2; // LDX
    cpu.memory[0xFCE3] = 0x41; // #
    cpu.memory[0xFCE4] = 0x8E; // STX
    cpu.memory[0xFCE5] = 0x00; // ABS
    cpu.memory[0xFCE6] = 0x00; // ABS

    while (cpu.pc < 0xFCE7) {
        nsg6502_opcode_execute(&cpu);
    }

    printf("A: 0x%hhx X: 0x%hhx Y: 0x%hhx PC: 0x%hx SP: 0x%x STATUS: 0x%hhx\n",
           cpu.a, cpu.x, cpu.y, cpu.pc, 0x100 + cpu.sp, cpu.status);

    free(cpu.memory);

    return 0;
}
