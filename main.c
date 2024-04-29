#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nsg6502.h"

#include <SDL2/SDL.h>
uint32_t *fb = NULL;

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

uint32_t color_to_rgba(uint8_t c) {
    uint32_t colors[] =
            {
                0x000000, // Black
                0xFFFFFF, // White
                0x880000, // Red
                0xAAFFEE, // Cyan
                0xCC44CC, // Magenta
                0x00CC55, // Green
                0x0000AA, // Blue
                0xEEEE77, // Yellow
                0xDD8855, // Orange
                0x664400, // Brown
                0xFF7777, // Light red
                0x333333, // Dark grey
                0x777777, // Grey
                0xAAFF66, // Light green
                0x0088FF, // Light blue
                0xBBBBBB // Light grey
            };
    return colors[c];
}

void main_memory_write_callback(struct nsg6502_cpu *c, uint16_t addr, uint8_t data) {
    if (addr >= 0x0200 && addr <= 0x05ff) {
        size_t offset = addr - 0x0200;
        fb[offset] = color_to_rgba(data % 16);
    }
    else {
        c->memory[addr] = data;
    }
}

uint8_t main_memory_read_callback(struct nsg6502_cpu *c, uint16_t addr) {
    if (addr == 0x0FE) {
        return rand() % 256;
    }
    else {
        return c->memory[addr];
    }
}

int main(void) {
    struct nsg6502_cpu cpu = {0};
    cpu.memory = malloc(0xFFFF);

    cpu.memory_write_callback = main_memory_write_callback;
    cpu.memory_read_callback = main_memory_read_callback;

    srand(time(NULL));

    char code[] = "\xa0\xff\xa2\x00\xa5\xfe\x9d\x00\x02\x29\x07\x9d\x00\x03\x29\x03\x9d\x00\x04\x29\x01\x9d\x00\x05\xe8\x88\xd0\xe8\x60";
    memcpy(&cpu.memory[0x0600], code, sizeof(code));

    cpu.memory[0xFFFC] = 0x00;
    cpu.memory[0xFFFD] = 0x06;

    nsg6502_reset(&cpu);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window * window = SDL_CreateWindow("NSG6502",
                                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           32, 32,
                                           0);
    SDL_Surface *window_surface = SDL_GetWindowSurface(window);
    fb = window_surface->pixels;
    memset(fb, 0, 32 * 32 * 4);

    while (cpu.pc != 0x0600 + sizeof(code) - 2) {
        SDL_Event event = {0};

        SDL_UpdateWindowSurface(window);

        nsg6502_opcode_execute(&cpu);

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) exit(0);
        }
    }

    free(cpu.memory);

    return 0;
}
