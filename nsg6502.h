/*
 * Copyright 2024 - (__DATE__ + 7) NSG650
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NSG6502_H
#define NSG6502_H

#include <stdint.h>
#include <stddef.h>

#ifndef NSG6502_NO_LIBC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NSG6502_MALLOC malloc
#define NSG6502_FREE free
#define NSG6502_DEBUG_PRINT(...) (printf(__VA_ARGS__)) 

#endif

#define NSG6502_FLAG_SET(s, x) (s |= x)
#define NSG6502_FLAG_CLEAR(s, x) (s &= ~(x))
#define NSG6502_FLAG_IS_SET(s, x) (s & x)

#define NSG6502_STATUS_REGISTER_CARRY (1 << 0)
#define NSG6502_STATUS_REGISTER_ZERO (1 << 1)
#define NSG6502_STATUS_REGISTER_INTERRUPT_DISABLE (1 << 2)
#define NSG6502_STATUS_REGISTER_DECIMAL (1 << 3)
#define NSG6502_STATUS_REGISTER_BREAK (1 << 4)
#define NSG6502_STATUS_REGISTER_OVERFLOW (1 << 6)
#define NSG6502_STATUS_REGISTER_NEGATIVE (1 << 7)

#define NSG6502_IS_SYSTEM_BIG_ENDIAN (1 != *(unsigned char *)&(const uint16_t){1})

struct nsg6502_cpu {
    uint8_t a;
    uint8_t y;
    uint8_t x;

    uint16_t pc;
    uint8_t sp;
    uint8_t status;

    uint8_t *memory;

    size_t ticks;
};

static void nsg6502_reset(struct nsg6502_cpu *c) {
    c->pc = 0xFCE2;
    c->sp = 0x00FD; // the SP will be 0x01FD
    NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_INTERRUPT_DISABLE);
    NSG6502_FLAG_CLEAR(c->status, NSG6502_STATUS_REGISTER_DECIMAL);
    NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_BREAK);
}

static uint8_t nsg6502_read_byte(struct nsg6502_cpu *c, uint16_t addr) {
    c->ticks++;
    return c->memory[addr];
}

static void nsg6502_write_byte(struct nsg6502_cpu *c, uint16_t addr, uint8_t data) {
    c->ticks++;
    c->memory[addr] = data;
}

static uint16_t nsg6502_read_word(struct nsg6502_cpu *c, uint16_t addr) {
    if (NSG6502_IS_SYSTEM_BIG_ENDIAN) { return (nsg6502_read_byte(c, addr) << 8) | (nsg6502_read_byte(c, addr + 1)); }
    else { return nsg6502_read_byte(c, addr) | (nsg6502_read_byte(c, addr + 1) << 8); }
}

static void nsg6502_write_word(struct nsg6502_cpu *c, uint16_t addr, uint16_t data) {
    if (NSG6502_IS_SYSTEM_BIG_ENDIAN) {
        nsg6502_write_byte(c, addr, (data >> 8) & 0xFF);
        nsg6502_write_byte(c, addr + 1, data & 0xFF);
    }
    else {
        nsg6502_write_byte(c, addr, data & 0xFF);
        nsg6502_write_byte(c, addr + 1, (data >> 8) & 0xFF);
    }
}

static uint8_t nsg6502_fetch_byte(struct nsg6502_cpu *c) {
    return nsg6502_read_byte(c, c->pc++);
}

static uint16_t nsg6502_fetch_word(struct nsg6502_cpu *c) {
    uint16_t ret = nsg6502_read_word(c, c->pc++);
    c->pc++;
    return ret;
}

static uint8_t nsg6502_stack_pop_byte(struct nsg6502_cpu *c) {
    c->ticks++;
    return c->memory[0x100 + ++c->sp];
}

static void nsg6502_stack_push_byte(struct nsg6502_cpu *c, uint8_t d) {
    c->ticks++;
    c->memory[0x100 + c->sp--] = d;
}

static void nsg6502_evaluate_flags(struct nsg6502_cpu *c, uint8_t res) {
    NSG6502_FLAG_CLEAR(c->status, NSG6502_STATUS_REGISTER_ZERO);
    if ((res & 0x80)) {
        NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_NEGATIVE);
    }
    if (res == 0) {
        NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_ZERO);
    }
}

struct nsg6502_opcode {
    char *name;
    size_t ticks;
    void (*function)(struct nsg6502_cpu *);
};

static void nsg6502_opcode_nop(struct nsg6502_cpu *c) {
    return;
}

static void nsg6502_opcode_clc(struct nsg6502_cpu *c) {
    NSG6502_FLAG_CLEAR(c->status, NSG6502_STATUS_REGISTER_CARRY);
}

static void nsg6502_opcode_cld(struct nsg6502_cpu *c) {
    NSG6502_FLAG_CLEAR(c->status, NSG6502_STATUS_REGISTER_DECIMAL);
}

static void nsg6502_opcode_cli(struct nsg6502_cpu *c) {
    NSG6502_FLAG_CLEAR(c->status, NSG6502_STATUS_REGISTER_INTERRUPT_DISABLE);
}

static void nsg6502_opcode_clv(struct nsg6502_cpu *c) {
    NSG6502_FLAG_CLEAR(c->status, NSG6502_STATUS_REGISTER_OVERFLOW);
}

static void nsg6502_opcode_dec_zp(struct nsg6502_cpu *c) {
    uint8_t addr = nsg6502_fetch_byte(c);
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) - 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
}

static void nsg6502_opcode_dec_zpx(struct nsg6502_cpu *c) {
    uint8_t addr = (nsg6502_fetch_byte(c) + c->x) & 0xFF;
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) - 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
}

static void nsg6502_opcode_dec_abs(struct nsg6502_cpu *c) {
    uint16_t addr = nsg6502_fetch_word(c);
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) - 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
}

static void nsg6502_opcode_dec_absx(struct nsg6502_cpu *c) {
    uint16_t addr = nsg6502_fetch_word(c) + c->x;
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) - 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
}

static void nsg6502_opcode_dex(struct nsg6502_cpu *c) {
    c->x--;
    nsg6502_evaluate_flags(c, c->x);
}

static void nsg6502_opcode_dey(struct nsg6502_cpu *c) {
    c->y--;
    nsg6502_evaluate_flags(c, c->y);
}

static void nsg6502_opcode_inc_zp(struct nsg6502_cpu *c) {
    uint8_t addr = nsg6502_fetch_byte(c);
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) + 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
}

static void nsg6502_opcode_inc_zpx(struct nsg6502_cpu *c) {
    uint8_t addr = (nsg6502_fetch_byte(c) + c->x) & 0xFF;
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) + 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
}

static void nsg6502_opcode_inc_abs(struct nsg6502_cpu *c) {
    uint16_t addr = nsg6502_fetch_word(c);
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) + 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
}

static void nsg6502_opcode_inc_absx(struct nsg6502_cpu *c) {
    uint16_t addr = nsg6502_fetch_word(c) + c->x;
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) + 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
}

static void nsg6502_opcode_inx(struct nsg6502_cpu *c) {
    c->x++;
    nsg6502_evaluate_flags(c, c->x);
}

static void nsg6502_opcode_iny(struct nsg6502_cpu *c) {
    c->y++;
    nsg6502_evaluate_flags(c, c->y);
}

static void nsg6502_opcode_lda_imm(struct nsg6502_cpu *c) {
    c->a = nsg6502_fetch_byte(c);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_lda_zp(struct nsg6502_cpu *c) {
    c->a = nsg6502_read_byte(c, nsg6502_fetch_byte(c));
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_lda_zpx(struct nsg6502_cpu *c) {
    c->a = nsg6502_read_byte(c, (nsg6502_fetch_byte(c) + c->x) & 0xFF);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_lda_abs(struct nsg6502_cpu *c) {
    c->a = nsg6502_read_byte(c, nsg6502_fetch_word(c));
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_lda_abx(struct nsg6502_cpu *c) {
    c->a = nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->x);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_lda_aby(struct nsg6502_cpu *c) {
    c->a = nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->y);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_lda_inx(struct nsg6502_cpu *c) {
    c->a = nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c)) + c->x);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_lda_iny(struct nsg6502_cpu *c) {
    c->a = nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c)) + c->y);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_ldx_imm(struct nsg6502_cpu *c) {
    c->x = nsg6502_fetch_byte(c);
    nsg6502_evaluate_flags(c, c->x);
}

static void nsg6502_opcode_ldx_zp(struct nsg6502_cpu *c) {
    c->x = nsg6502_read_byte(c, nsg6502_fetch_byte(c));
    nsg6502_evaluate_flags(c, c->x);
}

static void nsg6502_opcode_ldx_zpy(struct nsg6502_cpu *c) {
    c->x = nsg6502_read_byte(c, (nsg6502_fetch_byte(c) + c->y) & 0xFF);
    nsg6502_evaluate_flags(c, c->x);
}

static void nsg6502_opcode_ldx_abs(struct nsg6502_cpu *c) {
    c->x = nsg6502_read_byte(c, nsg6502_fetch_word(c));
    nsg6502_evaluate_flags(c, c->x);
}

static void nsg6502_opcode_ldx_aby(struct nsg6502_cpu *c) {
    c->x = nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->y);
    nsg6502_evaluate_flags(c, c->x);
}

static void nsg6502_opcode_ldy_imm(struct nsg6502_cpu *c) {
    c->y = nsg6502_fetch_byte(c);
    nsg6502_evaluate_flags(c, c->y);
}

static void nsg6502_opcode_ldy_zp(struct nsg6502_cpu *c) {
    c->y = nsg6502_read_byte(c, nsg6502_fetch_byte(c));
    nsg6502_evaluate_flags(c, c->y);
}

static void nsg6502_opcode_ldy_zpx(struct nsg6502_cpu *c) {
    c->y = nsg6502_read_byte(c, (nsg6502_fetch_byte(c) + c->x) & 0xFF);
    nsg6502_evaluate_flags(c, c->y);
}

static void nsg6502_opcode_ldy_abs(struct nsg6502_cpu *c) {
    c->y = nsg6502_read_byte(c, nsg6502_fetch_word(c));
    nsg6502_evaluate_flags(c, c->y);
}

static void nsg6502_opcode_ldy_abx(struct nsg6502_cpu *c) {
    c->y = nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->x);
    nsg6502_evaluate_flags(c, c->y);
}

static void nsg6502_opcode_sec(struct nsg6502_cpu *c) {
    NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY);
}

static void nsg6502_opcode_sed(struct nsg6502_cpu *c) {
    NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_DECIMAL);
}

static void nsg6502_opcode_sei(struct nsg6502_cpu *c) {
    NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_INTERRUPT_DISABLE);
}

static void nsg6502_opcode_sta_zp(struct nsg6502_cpu *c) {
    nsg6502_write_byte(c, nsg6502_fetch_byte(c), c->a);
}

static void nsg6502_opcode_sta_zpx(struct nsg6502_cpu *c) {
    nsg6502_write_byte(c, (nsg6502_fetch_byte(c) + c->x) & 0xFF, c->a);
}

static void nsg6502_opcode_sta_abs(struct nsg6502_cpu *c) {
    nsg6502_write_byte(c, nsg6502_fetch_word(c), c->a);
}

static void nsg6502_opcode_sta_abx(struct nsg6502_cpu *c) {
    nsg6502_write_byte(c, nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->x), c->a);
}

static void nsg6502_opcode_sta_aby(struct nsg6502_cpu *c) {
    nsg6502_write_byte(c, nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->y), c->a);
}

static void nsg6502_opcode_sta_inx(struct nsg6502_cpu *c) {
    nsg6502_write_byte(c, nsg6502_read_byte(c, nsg6502_read_word(c, (nsg6502_fetch_byte(c) + c->x) & 0xFF)), c->a);
}

static void nsg6502_opcode_sta_iny(struct nsg6502_cpu *c) {
    nsg6502_write_byte(c, nsg6502_read_byte(c, nsg6502_read_word(c, (nsg6502_fetch_byte(c) + c->y) & 0xFF)), c->a);
}

static void nsg6502_opcode_stx_zp(struct nsg6502_cpu *c) {
    nsg6502_write_byte(c, nsg6502_fetch_byte(c), c->x);
}

static void nsg6502_opcode_stx_zpy(struct nsg6502_cpu *c) {
    nsg6502_write_byte(c, (nsg6502_fetch_byte(c) + c->y) & 0xFF, c->x);
}

static void nsg6502_opcode_stx_abs(struct nsg6502_cpu *c) {
    nsg6502_write_byte(c, nsg6502_fetch_word(c), c->x);
}

static void nsg6502_opcode_sty_zp(struct nsg6502_cpu *c) {
    nsg6502_write_byte(c, nsg6502_fetch_byte(c), c->y);
}

static void nsg6502_opcode_sty_zpx(struct nsg6502_cpu *c) {
    nsg6502_write_byte(c, (nsg6502_fetch_byte(c) + c->x) & 0xFF, c->y);
}

static void nsg6502_opcode_sty_abs(struct nsg6502_cpu *c) {
    nsg6502_write_byte(c, nsg6502_fetch_word(c), c->y);
}

static void nsg6502_opcode_tax(struct nsg6502_cpu *c) {
    c->x = c->a;
    nsg6502_evaluate_flags(c, c->x);
}

static void nsg6502_opcode_tay(struct nsg6502_cpu *c) {
    c->y = c->a;
    nsg6502_evaluate_flags(c, c->y);
}

static void nsg6502_opcode_tsx(struct nsg6502_cpu *c) {
    c->x = c->sp;
    nsg6502_evaluate_flags(c, c->x);
}

static void nsg6502_opcode_txa(struct nsg6502_cpu *c) {
    c->a = c->x;
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_tya(struct nsg6502_cpu *c) {
    c->a = c->y;
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_txs(struct nsg6502_cpu *c) {
    c->sp = c->x;
    nsg6502_evaluate_flags(c, c->sp);
}

static void nsg6502_opcode_pha(struct nsg6502_cpu *c) {
    nsg6502_stack_push_byte(c, c->a);
}

static void nsg6502_opcode_pla(struct nsg6502_cpu *c) {
    c->a = nsg6502_stack_pop_byte(c);
}

static void nsg6502_opcode_php(struct nsg6502_cpu *c) {
    nsg6502_stack_push_byte(c, c->status);
}

static void nsg6502_opcode_plp(struct nsg6502_cpu *c) {
    c->status = nsg6502_stack_pop_byte(c);
}

// Cycle count might be incorrect
// Not bothered to fix it
const struct nsg6502_opcode NSG6502_OPCODES[256] = {
        [0x08] = {"PHP", 1, nsg6502_opcode_php},
        [0x28] = {"PLP", 1, nsg6502_opcode_plp},

        [0x48] = {"PHA", 1, nsg6502_opcode_pha},
        [0x68] = {"PLA", 1, nsg6502_opcode_pla},

        [0x8A] = {"TXA", 1, nsg6502_opcode_txa},
        [0x98] = {"TYA", 1, nsg6502_opcode_tya},
        [0x9A] = {"TXS", 1, nsg6502_opcode_txs},

        [0xAA] = {"TAX", 1, nsg6502_opcode_tax},
        [0xA8] = {"TAY", 1, nsg6502_opcode_tay},
        [0xBA] = {"TSX", 1, nsg6502_opcode_tsx},

        [0x84] = {"STY ZP", 1, nsg6502_opcode_sty_zp},
        [0x94] = {"STY ZP, X", 2, nsg6502_opcode_sty_zpx},
        [0x8C] = {"STY ABS", 1, nsg6502_opcode_sty_abs},

        [0x86] = {"STX ZP", 1, nsg6502_opcode_stx_zp},
        [0x96] = {"STX ZP, Y", 2, nsg6502_opcode_stx_zpy},
        [0x8E] = {"STX ABS", 1, nsg6502_opcode_stx_abs},

        [0x85] = {"STA ZP", 1, nsg6502_opcode_sta_zp},
        [0x95] = {"STA ZP, X", 2, nsg6502_opcode_sta_zpx},
        [0x8D] = {"STA ABS", 1, nsg6502_opcode_sta_abs},
        [0x9D] = {"STA ABX", 1, nsg6502_opcode_sta_abx},
        [0x99] = {"STA ABY", 1, nsg6502_opcode_sta_aby},
        [0x81] = {"STA INX", 1, nsg6502_opcode_sta_inx},
        [0x91] = {"STA INY", 1, nsg6502_opcode_sta_iny},

        [0x38] = {"SEC", 1, nsg6502_opcode_sec},
        [0xF8] = {"SED", 1, nsg6502_opcode_sed},
        [0x78] = {"SEI", 1, nsg6502_opcode_sei},

        [0xA0] = {"LDY #", 1, nsg6502_opcode_ldy_imm},
        [0xA4] = {"LDY ZP", 1, nsg6502_opcode_ldy_zp},
        [0xB4] = {"LDY ZP, X", 2, nsg6502_opcode_ldy_zpx},
        [0xAC] = {"LDY ABS", 1, nsg6502_opcode_ldy_abs},
        [0xBC] = {"LDY ABX", 1, nsg6502_opcode_ldy_zpx},

        [0xA2] = {"LDX #", 1, nsg6502_opcode_ldx_imm},
        [0xA6] = {"LDX ZP", 1, nsg6502_opcode_ldx_zp},
        [0xB6] = {"LDX ZP, Y", 2, nsg6502_opcode_ldx_zpy},
        [0xAE] = {"LDX ABS", 1, nsg6502_opcode_ldx_abs},
        [0xBE] = {"LDX ABY", 1, nsg6502_opcode_ldx_aby},

        [0xA9] = {"LDA #", 1, nsg6502_opcode_lda_imm},
        [0xA5] = {"LDA ZP", 1, nsg6502_opcode_lda_zp},
        [0xB5] = {"LDA ZP, X", 2, nsg6502_opcode_lda_zpx},
        [0xAD] = {"LDA ABS", 1, nsg6502_opcode_lda_abs},
        [0xBD] = {"LDA ABX", 1, nsg6502_opcode_lda_abx},
        [0xB9] = {"LDA ABY", 1, nsg6502_opcode_lda_aby},
        [0xA1] = {"LDA INX", 1, nsg6502_opcode_lda_inx},
        [0xB1] = {"LDA INY", 1, nsg6502_opcode_lda_iny},

        [0xE8] = {"INX", 1, nsg6502_opcode_inx},
        [0xC8] = {"INY", 1, nsg6502_opcode_iny},

        [0xE6] = {"INC ZP", 1, nsg6502_opcode_inc_zp},
        [0xF6] = {"INC ZP, X", 2, nsg6502_opcode_inc_zpx},
        [0xEE] = {"INC ABS", 1, nsg6502_opcode_inc_abs},
        [0xFE] = {"INC ABS, X", 2, nsg6502_opcode_inc_absx},

        [0xCA] = {"DEX", 1, nsg6502_opcode_dex},
        [0x88] = {"DEY", 1, nsg6502_opcode_dey},

        [0xC6] = {"DEC ZP", 1, nsg6502_opcode_dec_zp},
        [0xD6] = {"DEC ZP, X", 2, nsg6502_opcode_dec_zpx},
        [0xCE] = {"DEC ABS", 1, nsg6502_opcode_dec_abs},
        [0xDE] = {"DEC ABS, X", 2, nsg6502_opcode_dec_absx},

        [0x18] = {"CLC", 1, nsg6502_opcode_clc},
        [0xD8] = {"CLD", 1, nsg6502_opcode_cld},
        [0x58] = {"CLI", 1, nsg6502_opcode_cli},
        [0xB8] = {"CLV", 1, nsg6502_opcode_clv},

        [0xEA] = {"NOP", 1, nsg6502_opcode_nop}
};

void nsg6502_opcode_execute(struct nsg6502_cpu *c) {
    uint8_t opcode_byte = nsg6502_fetch_byte(c);

    struct nsg6502_opcode opcode = NSG6502_OPCODES[opcode_byte];
    if (!opcode.function) { return; }

    c->ticks += opcode.ticks;
    opcode.function(c);
}

#endif
