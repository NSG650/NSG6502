#include <stdio.h>
#include <stdlib.h>

#include "nsg6502.h"

int main(void) {
    struct nsg6502_cpu cpu = {0};
    cpu.memory = malloc(0xFFFF);

    nsg6502_reset(&cpu);

    cpu.memory[0xFCE2] = 0x08; // PHP
    cpu.memory[0xFCE3] = 0x68; // PLA
    cpu.memory[0xFCE4] = 0x69; // ADC
    cpu.memory[0xFCE5] = 0x02; // #
    cpu.memory[0xFCE7] = 0xE9; // SBC
    cpu.memory[0xFCE8] = 0x01; // #

    while (cpu.pc < 0xFCE8) {
        nsg6502_opcode_execute(&cpu);
    }

    printf("A: 0x%hhx X: 0x%hhx Y: 0x%hhx PC: 0x%hx SP: 0x%x STATUS: 0x%hhx\n",
           cpu.a, cpu.x, cpu.y, cpu.pc, 0x100 + cpu.sp, cpu.status);

    free(cpu.memory);

    return 0;
}
