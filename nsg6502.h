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
    NSG6502_FLAG_CLEAR(c->status, NSG6502_STATUS_REGISTER_CARRY);

    if ((res & 0x80)) {
        NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_NEGATIVE);
    }
    if (res == 0) {
        NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_ZERO);
    }
}

static void nsg6502_adc(struct nsg6502_cpu *c, uint8_t d) {
    int32_t tmp = c->a + d + (NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY) ? 1 : 0);

    if (!(tmp & 0xFF)) {
        NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_ZERO);
    }

    if (NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_DECIMAL)) {
        if ((c->a & 0xF) + (d & 0xF) + (NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY) ? 1 : 0) > 9) {
            tmp += 6;
        }
        if (tmp > 0x99) {
            tmp += 96;
            NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY);
        }
    }
    else {
        if (tmp > 0xFF) {
            NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY);
        }
    }

    if ((tmp & 0x80)) {
        NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_NEGATIVE);
    }

    if (!((c->a ^ d) & 0x80) && ((c->a ^ tmp) & 0x80)) {
        NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_OVERFLOW);
    }

    c->a = (uint8_t)(tmp & 0xFF);
}

static void nsg6502_sbc(struct nsg6502_cpu *c, uint8_t d) {
    int32_t tmp = c->a - d - (NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY) ? 1 : 0);

    if (!(tmp & 0xFF)) {
        NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_ZERO);
    }

    if ((tmp & 0x80)) {
        NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_NEGATIVE);
    }

    if (!((c->a ^ d) & 0x80) && ((c->a ^ tmp) & 0x80)) {
        NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_OVERFLOW);
    }

    if (NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_DECIMAL)) {
        if (((c->a & 0xF) - ((NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY) ? 1 : 0)) < (d & 0x0F))) {
            tmp -= 6;
        }
        if (tmp > 0x99) {
            tmp -= 0x60;
        }
    }

    if (tmp < 0x100) {
        NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY);
    }

    c->a = (uint8_t)(tmp & 0xFF);
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

static void nsg6502_opcode_dec_abx(struct nsg6502_cpu *c) {
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

static void nsg6502_opcode_ora_imm(struct nsg6502_cpu *c) {
    c->a |= nsg6502_fetch_byte(c);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_ora_zp(struct nsg6502_cpu *c) {
    c->a |= nsg6502_read_byte(c, nsg6502_fetch_byte(c));
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_ora_zpx(struct nsg6502_cpu *c) {
    c->a |= nsg6502_read_byte(c, (nsg6502_fetch_byte(c) + c->x) & 0xFF);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_ora_abs(struct nsg6502_cpu *c) {
    c->a |= nsg6502_read_byte(c, nsg6502_fetch_word(c));
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_ora_abx(struct nsg6502_cpu *c) {
    c->a |= nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->x);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_ora_aby(struct nsg6502_cpu *c) {
    c->a |= nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->y);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_ora_inx(struct nsg6502_cpu *c) {
    c->a |= nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c)) + c->x);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_ora_iny(struct nsg6502_cpu *c) {
    c->a |= nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c)) + c->y);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_and_imm(struct nsg6502_cpu *c) {
    c->a &= nsg6502_fetch_byte(c);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_and_zp(struct nsg6502_cpu *c) {
    c->a &= nsg6502_read_byte(c, nsg6502_fetch_byte(c));
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_and_zpx(struct nsg6502_cpu *c) {
    c->a &= nsg6502_read_byte(c, (nsg6502_fetch_byte(c) + c->x) & 0xFF);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_and_abs(struct nsg6502_cpu *c) {
    c->a &= nsg6502_read_byte(c, nsg6502_fetch_word(c));
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_and_abx(struct nsg6502_cpu *c) {
    c->a &= nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->x);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_and_aby(struct nsg6502_cpu *c) {
    c->a &= nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->y);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_and_inx(struct nsg6502_cpu *c) {
    c->a &= nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c)) + c->x);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_and_iny(struct nsg6502_cpu *c) {
    c->a &= nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c)) + c->y);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_eor_imm(struct nsg6502_cpu *c) {
    c->a ^= nsg6502_fetch_byte(c);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_eor_zp(struct nsg6502_cpu *c) {
    c->a ^= nsg6502_read_byte(c, nsg6502_fetch_byte(c));
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_eor_zpx(struct nsg6502_cpu *c) {
    c->a ^= nsg6502_read_byte(c, (nsg6502_fetch_byte(c) + c->x) & 0xFF);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_eor_abs(struct nsg6502_cpu *c) {
    c->a ^= nsg6502_read_byte(c, nsg6502_fetch_word(c));
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_eor_abx(struct nsg6502_cpu *c) {
    c->a ^= nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->x);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_eor_aby(struct nsg6502_cpu *c) {
    c->a ^= nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->y);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_eor_inx(struct nsg6502_cpu *c) {
    c->a ^= nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c)) + c->x);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_eor_iny(struct nsg6502_cpu *c) {
    c->a ^= nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c)) + c->y);
    nsg6502_evaluate_flags(c, c->a);
}

static void nsg6502_opcode_adc_imm(struct nsg6502_cpu *c) {
    nsg6502_adc(c, nsg6502_fetch_byte(c));
}

static void nsg6502_opcode_adc_zp(struct nsg6502_cpu *c) {
    nsg6502_adc(c, nsg6502_read_byte(c, nsg6502_fetch_byte(c)));
}

static void nsg6502_opcode_adc_zpx(struct nsg6502_cpu *c) {
    nsg6502_adc(c, nsg6502_read_byte(c, (nsg6502_fetch_byte(c)) + c->x) & 0xFF);
}

static void nsg6502_opcode_adc_abs(struct nsg6502_cpu *c) {
    nsg6502_adc(c, nsg6502_read_byte(c, nsg6502_fetch_word(c)));
}

static void nsg6502_opcode_adc_abx(struct nsg6502_cpu *c) {
    nsg6502_adc(c, nsg6502_read_byte(c, nsg6502_fetch_word(c)) + c->x);
}

static void nsg6502_opcode_adc_aby(struct nsg6502_cpu *c) {
    nsg6502_adc(c, nsg6502_read_byte(c, nsg6502_fetch_word(c)) + c->y);
}

static void nsg6502_opcode_adc_inx(struct nsg6502_cpu *c) {
    nsg6502_adc(c, nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c))) + c->x);
}

static void nsg6502_opcode_adc_iny(struct nsg6502_cpu *c) {
    nsg6502_adc(c, nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c))) + c->y);
}

static void nsg6502_opcode_sbc_imm(struct nsg6502_cpu *c) {
    nsg6502_sbc(c, nsg6502_fetch_byte(c));
}

static void nsg6502_opcode_sbc_zp(struct nsg6502_cpu *c) {
    nsg6502_sbc(c, nsg6502_read_byte(c, nsg6502_fetch_byte(c)));
}

static void nsg6502_opcode_sbc_zpx(struct nsg6502_cpu *c) {
    nsg6502_sbc(c, nsg6502_read_byte(c, (nsg6502_fetch_byte(c)) + c->x) & 0xFF);
}

static void nsg6502_opcode_sbc_abs(struct nsg6502_cpu *c) {
    nsg6502_sbc(c, nsg6502_read_byte(c, nsg6502_fetch_word(c)));
}

static void nsg6502_opcode_sbc_abx(struct nsg6502_cpu *c) {
    nsg6502_sbc(c, nsg6502_read_byte(c, nsg6502_fetch_word(c)) + c->x);
}

static void nsg6502_opcode_sbc_aby(struct nsg6502_cpu *c) {
    nsg6502_sbc(c, nsg6502_read_byte(c, nsg6502_fetch_word(c)) + c->y);
}

static void nsg6502_opcode_sbc_inx(struct nsg6502_cpu *c) {
    nsg6502_sbc(c, nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c))) + c->x);
}

static void nsg6502_opcode_sbc_iny(struct nsg6502_cpu *c) {
    nsg6502_sbc(c, nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c))) + c->y);
}

static void nsg6502_opcode_cmp_imm(struct nsg6502_cpu *c) {
    int32_t tmp = c->a - nsg6502_fetch_byte(c);
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_cmp_zp(struct nsg6502_cpu *c) {
    int32_t tmp = c->a - nsg6502_read_byte(c, nsg6502_fetch_byte(c));
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_cmp_zpx(struct nsg6502_cpu *c) {
    int32_t tmp = c->a - nsg6502_read_byte(c, (nsg6502_fetch_byte(c) + c->x) & 0xFF);
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_cmp_abs(struct nsg6502_cpu *c) {
    int32_t tmp = c->a - nsg6502_read_byte(c, nsg6502_fetch_word(c));
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_cmp_abx(struct nsg6502_cpu *c) {
    int32_t tmp = c->a - nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->x);
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_cmp_aby(struct nsg6502_cpu *c) {
    int32_t tmp = c->a - nsg6502_read_byte(c, nsg6502_fetch_word(c) + c->y);
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_cmp_inx(struct nsg6502_cpu *c) {
    int32_t tmp = c->a - nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c)) + c->x);
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_cmp_iny(struct nsg6502_cpu *c) {
    int32_t tmp = c->a - nsg6502_read_byte(c, nsg6502_read_word(c, nsg6502_fetch_byte(c)) + c->y);
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_cpy_imm(struct nsg6502_cpu *c) {
    int32_t tmp = c->y - nsg6502_fetch_byte(c);
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_cpy_zp(struct nsg6502_cpu *c) {
    int32_t tmp = c->y - nsg6502_read_byte(c, nsg6502_fetch_byte(c));
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_cpy_abs(struct nsg6502_cpu *c) {
    int32_t tmp = c->y - nsg6502_read_byte(c, nsg6502_fetch_word(c));
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_cpx_imm(struct nsg6502_cpu *c) {
    int32_t tmp = c->x - nsg6502_fetch_byte(c);
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_cpx_zp(struct nsg6502_cpu *c) {
    int32_t tmp = c->x - nsg6502_read_byte(c, nsg6502_fetch_byte(c));
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_cpx_abs(struct nsg6502_cpu *c) {
    int32_t tmp = c->x - nsg6502_read_byte(c, nsg6502_fetch_word(c));
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp >= 0) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_bit_zp(struct nsg6502_cpu *c) {
    uint16_t tmp = c->a & nsg6502_read_byte(c, nsg6502_fetch_byte(c));
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp & 0x40) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_OVERFLOW); }
}

static void nsg6502_opcode_bit_abs(struct nsg6502_cpu *c) {
    uint16_t tmp = c->a & nsg6502_read_byte(c, nsg6502_fetch_word(c));
    nsg6502_evaluate_flags(c, (uint8_t)(tmp & 0xFF));
    if (tmp & 0x40) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_OVERFLOW); }
}

static void nsg6502_opcode_asl_a(struct nsg6502_cpu *c) {
    c->a = c->a << 1;
    nsg6502_evaluate_flags(c, c->a);
    if(c->a & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_asl_zp(struct nsg6502_cpu *c) {
    uint8_t addr = nsg6502_fetch_byte(c);
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) << 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
    if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_asl_zpx(struct nsg6502_cpu *c) {
    uint8_t addr = (nsg6502_fetch_byte(c) + c->x) & 0xFF;
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) << 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d); if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_asl_abs(struct nsg6502_cpu *c) {
    uint16_t addr = nsg6502_fetch_word(c);
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) << 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
    if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_asl_abx(struct nsg6502_cpu *c) {
    uint16_t addr = nsg6502_fetch_word(c) + c->x;
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) << 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
    if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_lsr_a(struct nsg6502_cpu *c) {
    c->a = c->a >> 1;
    nsg6502_evaluate_flags(c, c->a);
    if(c->a & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_lsr_zp(struct nsg6502_cpu *c) {
    uint8_t addr = nsg6502_fetch_byte(c);
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) >> 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
    if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_lsr_zpx(struct nsg6502_cpu *c) {
    uint8_t addr = (nsg6502_fetch_byte(c) + c->x) & 0xFF;
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) >> 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d); if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_lsr_abs(struct nsg6502_cpu *c) {
    uint16_t addr = nsg6502_fetch_word(c);
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) >> 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
    if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_lsr_abx(struct nsg6502_cpu *c) {
    uint16_t addr = nsg6502_fetch_word(c) + c->x;
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) >> 1);

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
    if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_rol_a(struct nsg6502_cpu *c) {
    c->a = c->a << 1 | NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY);
    nsg6502_evaluate_flags(c, c->a);
    if(c->a & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_rol_zp(struct nsg6502_cpu *c) {
    uint8_t addr = nsg6502_fetch_byte(c);
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) << 1 | NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY));

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
    if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_rol_zpx(struct nsg6502_cpu *c) {
    uint8_t addr = (nsg6502_fetch_byte(c) + c->x) & 0xFF;
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) << 1 | NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY));

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d); if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_rol_abs(struct nsg6502_cpu *c) {
    uint16_t addr = nsg6502_fetch_word(c);
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) << 1 | NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY));

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
    if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_rol_abx(struct nsg6502_cpu *c) {
    uint16_t addr = nsg6502_fetch_word(c) + c->x;
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) << 1 | NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY));

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
    if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_ror_a(struct nsg6502_cpu *c) {
    c->a = c->a << 7 | NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY);
    nsg6502_evaluate_flags(c, c->a);
    if(c->a & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_ror_zp(struct nsg6502_cpu *c) {
    uint8_t addr = nsg6502_fetch_byte(c);
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) << 7 | NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY));

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
    if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_ror_zpx(struct nsg6502_cpu *c) {
    uint8_t addr = (nsg6502_fetch_byte(c) + c->x) & 0xFF;
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) << 7 | NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY));

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d); if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_ror_abs(struct nsg6502_cpu *c) {
    uint16_t addr = nsg6502_fetch_word(c);
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) << 7 | NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY));

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
    if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

static void nsg6502_opcode_ror_abx(struct nsg6502_cpu *c) {
    uint16_t addr = nsg6502_fetch_word(c) + c->x;
    nsg6502_write_byte(c, addr, nsg6502_read_byte(c, addr) << 7 | NSG6502_FLAG_IS_SET(c->status, NSG6502_STATUS_REGISTER_CARRY));

    uint8_t d = c->memory[addr];
    nsg6502_evaluate_flags(c, d);
    if(d & 0xFF00) { NSG6502_FLAG_SET(c->status, NSG6502_STATUS_REGISTER_CARRY); }
}

// Cycle count might be incorrect
// Not bothered to fix it
const struct nsg6502_opcode NSG6502_OPCODES[256] = {
        [0x6A] = {"ROR A", 1, nsg6502_opcode_ror_a},
        [0x66] = {"ROR ZP", 1, nsg6502_opcode_ror_zp},
        [0x76] = {"ROR ZP, X", 2, nsg6502_opcode_ror_zpx},
        [0x6E] = {"ROR ABS", 1, nsg6502_opcode_ror_abs},
        [0x7E] = {"ROR ABS, X", 1, nsg6502_opcode_ror_abx},

        [0x2A] = {"ROL A", 1, nsg6502_opcode_rol_a},
        [0x26] = {"ROL ZP", 1, nsg6502_opcode_rol_zp},
        [0x36] = {"ROL ZP, X", 2, nsg6502_opcode_rol_zpx},
        [0x2E] = {"ROL ABS", 1, nsg6502_opcode_rol_abs},
        [0x3E] = {"ROL ABS, X", 1, nsg6502_opcode_rol_abx},

        [0x4A] = {"LSR A", 1, nsg6502_opcode_lsr_a},
        [0x46] = {"LSR ZP", 1, nsg6502_opcode_lsr_zp},
        [0x56] = {"LSR ZP, X", 2, nsg6502_opcode_lsr_zpx},
        [0x4E] = {"LSR ABS", 1, nsg6502_opcode_lsr_abs},
        [0x5E] = {"LSR ABS, X", 1, nsg6502_opcode_lsr_abx},

        [0x0A] = {"ASL A", 1, nsg6502_opcode_asl_a},
        [0x06] = {"ASL ZP", 1, nsg6502_opcode_asl_zp},
        [0x16] = {"ASL ZP, X", 2, nsg6502_opcode_asl_zpx},
        [0x0E] = {"ASL ABS", 1, nsg6502_opcode_asl_abs},
        [0x1E] = {"ASL ABS, X", 1, nsg6502_opcode_asl_abx},

        [0x24] = {"BIT ZP", 1, nsg6502_opcode_bit_zp},
        [0x2C] = {"BIT ABS", 1, nsg6502_opcode_bit_abs},

        [0xC0] = {"CPY #", 1, nsg6502_opcode_cpy_imm},
        [0xC4] = {"CPY ZP", 1, nsg6502_opcode_cpy_zp},
        [0xCC] = {"CPY ABS", 1, nsg6502_opcode_cpy_abs},

        [0xE0] = {"CPX #", 1, nsg6502_opcode_cpx_imm},
        [0xE4] = {"CPX ZP", 2, nsg6502_opcode_cpx_zp},
        [0xEC] = {"CPX ABS", 1, nsg6502_opcode_cpx_abs},

        [0xC9] = {"CMP #", 1, nsg6502_opcode_cmp_imm},
        [0xC5] = {"CMP ZP", 1, nsg6502_opcode_cmp_zp},
        [0xD5] = {"CMP ZP, X", 2, nsg6502_opcode_cmp_zpx},
        [0xCD] = {"CMP ABS", 1, nsg6502_opcode_cmp_abs},
        [0xDD] = {"CMP ABS, X", 1, nsg6502_opcode_cmp_abx},
        [0xD9] = {"CMP ABS, Y", 1, nsg6502_opcode_cmp_aby},
        [0xC1] = {"CMP INX", 1, nsg6502_opcode_cmp_inx},
        [0xD1] = {"CMP INY", 1, nsg6502_opcode_cmp_iny},

        [0xE9] = {"SBC #", 1, nsg6502_opcode_sbc_imm},
        [0xE5] = {"SBC ZP", 1, nsg6502_opcode_sbc_zp},
        [0xF5] = {"SBC ZP, X", 2, nsg6502_opcode_sbc_zpx},
        [0xED] = {"SBC ABS", 1, nsg6502_opcode_sbc_abs},
        [0xFD] = {"SBC ABS, X", 1, nsg6502_opcode_sbc_abx},
        [0xF9] = {"SBC ABS, Y", 1, nsg6502_opcode_sbc_aby},
        [0xE1] = {"SBC INX", 1, nsg6502_opcode_sbc_inx},
        [0xF1] = {"SBC INY", 1, nsg6502_opcode_sbc_iny},

        [0x69] = {"ADC #", 1, nsg6502_opcode_adc_imm},
        [0x65] = {"ADC ZP", 1, nsg6502_opcode_adc_zp},
        [0x75] = {"ADC ZP, X", 2, nsg6502_opcode_adc_zpx},
        [0x6D] = {"ADC ABS", 1, nsg6502_opcode_adc_abs},
        [0x7D] = {"ADC ABS, X", 1, nsg6502_opcode_adc_abx},
        [0x79] = {"ADC ABS, Y", 1, nsg6502_opcode_adc_aby},
        [0x61] = {"ADC INX", 1, nsg6502_opcode_adc_inx},
        [0x71] = {"ADC INY", 1, nsg6502_opcode_adc_iny},

        [0x49] = {"EOR #", 1, nsg6502_opcode_eor_imm},
        [0x45] = {"EOR ZP", 1, nsg6502_opcode_eor_zp},
        [0x55] = {"EOR ZP, X", 2, nsg6502_opcode_eor_zpx},
        [0x4D] = {"EOR ABS", 1, nsg6502_opcode_eor_abs},
        [0x5D] = {"EOR ABS, X", 1, nsg6502_opcode_eor_abx},
        [0x59] = {"EOR ABS, Y", 1, nsg6502_opcode_eor_aby},
        [0x41] = {"EOR INX", 1, nsg6502_opcode_eor_inx},
        [0x51] = {"EOR INY", 1, nsg6502_opcode_eor_iny},

        [0x29] = {"AND #", 1, nsg6502_opcode_and_imm},
        [0x25] = {"AND ZP", 1, nsg6502_opcode_and_zp},
        [0x35] = {"AND ZP, X", 2, nsg6502_opcode_and_zpx},
        [0x2D] = {"AND ABS", 1, nsg6502_opcode_and_abs},
        [0x3D] = {"AND ABS, X", 1, nsg6502_opcode_and_abx},
        [0x39] = {"AND ABS, Y", 1, nsg6502_opcode_and_aby},
        [0x21] = {"AND INX", 1, nsg6502_opcode_and_inx},
        [0x31] = {"AND INY", 1, nsg6502_opcode_and_iny},

        [0x09] = {"ORA #", 1, nsg6502_opcode_ora_imm},
        [0x05] = {"ORA ZP", 1, nsg6502_opcode_ora_zp},
        [0x15] = {"ORA ZP, X", 2, nsg6502_opcode_ora_zpx},
        [0x0D] = {"ORA ABS", 1, nsg6502_opcode_ora_abs},
        [0x1D] = {"ORA ABS, X", 1, nsg6502_opcode_ora_abx},
        [0x19] = {"ORA ABS, Y", 1, nsg6502_opcode_ora_aby},
        [0x01] = {"ORA INX", 1, nsg6502_opcode_ora_inx},
        [0x11] = {"ORA INY", 1, nsg6502_opcode_ora_iny},

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
        [0xDE] = {"DEC ABS, X", 2, nsg6502_opcode_dec_abx},

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
