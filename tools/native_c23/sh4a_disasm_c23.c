/*
 * tools/native_c23/sh4a_disasm_c23.c
 * Strict ISO C23 SH-4/SH-4A disassembler (MSVC /std:clatest, Clang 21 -std=c23, GCC 16 -std=c23).
 */

#include "sh4a_disasm_c23.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(__clang__)
#ifndef nullptr
#define nullptr ((void*)0)
#endif
#endif

static_assert(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");

static int sext8(uint16_t v) {
    return (v & 0x80u) ? ((int)(v & 0xFFu) - 0x100) : (int)(v & 0xFFu);
}

static int sext12(uint16_t v) {
    return (v & 0x800u) ? ((int)(v & 0xFFFu) - 0x1000) : (int)(v & 0xFFFu);
}

static Sh4aDecodedInsn make_insn(uint16_t op, Sh4aOpcodeKind kind, const char* mn, const char* ops) {
    Sh4aDecodedInsn out;
    memset(&out, 0, sizeof(out));
    out.opcode = op;
    out.kind = kind;
    if (mn != nullptr) snprintf(out.mnemonic, sizeof(out.mnemonic), "%s", mn);
    if (ops != nullptr) snprintf(out.operands, sizeof(out.operands), "%s", ops);
    return out;
}

Sh4aDecodedInsn sh4a_decode_opcode(uint16_t op) {
    const unsigned n = (op >> 8u) & 0xFu;
    const unsigned m = (op >> 4u) & 0xFu;
    const unsigned low8 = op & 0xFFu;
    const unsigned sk = op & 0xFu;
    const unsigned high = op & 0xFF00u;
    char buf[48];

    /* Exact opcodes */
    switch (op) {
        case 0x0009: return make_insn(op, SH4A_KIND_NOP, "nop", "");
        case 0x0008: return make_insn(op, SH4A_KIND_SYSTEM, "clrt", "");
        case 0x0018: return make_insn(op, SH4A_KIND_SYSTEM, "sett", "");
        case 0x0048: return make_insn(op, SH4A_KIND_SYSTEM, "clrs", "");
        case 0x0058: return make_insn(op, SH4A_KIND_SYSTEM, "sets", "");
        case 0x0019: return make_insn(op, SH4A_KIND_SYSTEM, "div0u", "");
        case 0x0028: return make_insn(op, SH4A_KIND_SYSTEM, "clrmac", "");
        case 0x000B: return make_insn(op, SH4A_KIND_JUMP, "rts", "");
        case 0x002B: return make_insn(op, SH4A_KIND_JUMP, "rte", "");
        case 0x001B: return make_insn(op, SH4A_KIND_SYSTEM, "sleep", "");
        default: break;
    }

    if ((op & 0xF0FFu) == 0x00E3u) { snprintf(buf, sizeof(buf), "@R%u", n); return make_insn(op, SH4A_KIND_MEM, "icbi", buf); }
    if (high == 0xC300u) { snprintf(buf, sizeof(buf), "#0x%02X", low8); return make_insn(op, SH4A_KIND_SYSTEM, "trapa", buf); }

    /* STC/STS/LDC/LDS & system stack */
    switch (op & 0xF0FFu) {
        case 0x0002: snprintf(buf, sizeof(buf), "sr,R%u", n); return make_insn(op, SH4A_KIND_SYSTEM, "stc", buf);
        case 0x0012: snprintf(buf, sizeof(buf), "gbr,R%u", n); return make_insn(op, SH4A_KIND_SYSTEM, "stc", buf);
        case 0x0022: snprintf(buf, sizeof(buf), "vbr,R%u", n); return make_insn(op, SH4A_KIND_SYSTEM, "stc", buf);
        case 0x0032: snprintf(buf, sizeof(buf), "ssr,R%u", n); return make_insn(op, SH4A_KIND_SYSTEM, "stc", buf);
        case 0x0042: snprintf(buf, sizeof(buf), "spc,R%u", n); return make_insn(op, SH4A_KIND_SYSTEM, "stc", buf);
        case 0x003A: snprintf(buf, sizeof(buf), "sgr,R%u", n); return make_insn(op, SH4A_KIND_SYSTEM, "stc", buf);
        case 0x00FA: snprintf(buf, sizeof(buf), "dbr,R%u", n); return make_insn(op, SH4A_KIND_SYSTEM, "stc", buf);
        case 0x000A: snprintf(buf, sizeof(buf), "mach,R%u", n); return make_insn(op, SH4A_KIND_SYSTEM, "sts", buf);
        case 0x001A: snprintf(buf, sizeof(buf), "macl,R%u", n); return make_insn(op, SH4A_KIND_SYSTEM, "sts", buf);
        case 0x002A: snprintf(buf, sizeof(buf), "pr,R%u", n); return make_insn(op, SH4A_KIND_SYSTEM, "sts", buf);
        case 0x0029: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "movt", buf);
        case 0x400E: snprintf(buf, sizeof(buf), "R%u,sr", n); return make_insn(op, SH4A_KIND_SYSTEM, "ldc", buf);
        case 0x401E: snprintf(buf, sizeof(buf), "R%u,gbr", n); return make_insn(op, SH4A_KIND_SYSTEM, "ldc", buf);
        case 0x402E: snprintf(buf, sizeof(buf), "R%u,vbr", n); return make_insn(op, SH4A_KIND_SYSTEM, "ldc", buf);
        case 0x403E: snprintf(buf, sizeof(buf), "R%u,ssr", n); return make_insn(op, SH4A_KIND_SYSTEM, "ldc", buf);
        case 0x404E: snprintf(buf, sizeof(buf), "R%u,spc", n); return make_insn(op, SH4A_KIND_SYSTEM, "ldc", buf);
        case 0x40FA: snprintf(buf, sizeof(buf), "R%u,dbr", n); return make_insn(op, SH4A_KIND_SYSTEM, "ldc", buf);
        case 0x400A: snprintf(buf, sizeof(buf), "R%u,mach", n); return make_insn(op, SH4A_KIND_SYSTEM, "lds", buf);
        case 0x401A: snprintf(buf, sizeof(buf), "R%u,macl", n); return make_insn(op, SH4A_KIND_SYSTEM, "lds", buf);
        case 0x402A: snprintf(buf, sizeof(buf), "R%u,pr", n); return make_insn(op, SH4A_KIND_SYSTEM, "lds", buf);
        case 0x4002: snprintf(buf, sizeof(buf), "mach,@-R%u", n); return make_insn(op, SH4A_KIND_MEM, "sts.l", buf);
        case 0x4012: snprintf(buf, sizeof(buf), "macl,@-R%u", n); return make_insn(op, SH4A_KIND_MEM, "sts.l", buf);
        case 0x4022: snprintf(buf, sizeof(buf), "pr,@-R%u", n); return make_insn(op, SH4A_KIND_MEM, "sts.l", buf);
        case 0x4006: snprintf(buf, sizeof(buf), "@R%u+,mach", n); return make_insn(op, SH4A_KIND_MEM, "lds.l", buf);
        case 0x4016: snprintf(buf, sizeof(buf), "@R%u+,macl", n); return make_insn(op, SH4A_KIND_MEM, "lds.l", buf);
        case 0x4026: snprintf(buf, sizeof(buf), "@R%u+,pr", n); return make_insn(op, SH4A_KIND_MEM, "lds.l", buf);
        case 0x4003: snprintf(buf, sizeof(buf), "sr,@-R%u", n); return make_insn(op, SH4A_KIND_MEM, "stc.l", buf);
        case 0x4013: snprintf(buf, sizeof(buf), "gbr,@-R%u", n); return make_insn(op, SH4A_KIND_MEM, "stc.l", buf);
        case 0x4023: snprintf(buf, sizeof(buf), "vbr,@-R%u", n); return make_insn(op, SH4A_KIND_MEM, "stc.l", buf);
        case 0x4033: snprintf(buf, sizeof(buf), "ssr,@-R%u", n); return make_insn(op, SH4A_KIND_MEM, "stc.l", buf);
        case 0x4043: snprintf(buf, sizeof(buf), "spc,@-R%u", n); return make_insn(op, SH4A_KIND_MEM, "stc.l", buf);
        case 0x4032: snprintf(buf, sizeof(buf), "sgr,@-R%u", n); return make_insn(op, SH4A_KIND_MEM, "stc.l", buf);
        case 0x40F2: snprintf(buf, sizeof(buf), "dbr,@-R%u", n); return make_insn(op, SH4A_KIND_MEM, "stc.l", buf);
        case 0x4007: snprintf(buf, sizeof(buf), "@R%u+,sr", n); return make_insn(op, SH4A_KIND_MEM, "ldc.l", buf);
        case 0x4017: snprintf(buf, sizeof(buf), "@R%u+,gbr", n); return make_insn(op, SH4A_KIND_MEM, "ldc.l", buf);
        case 0x4027: snprintf(buf, sizeof(buf), "@R%u+,vbr", n); return make_insn(op, SH4A_KIND_MEM, "ldc.l", buf);
        case 0x4037: snprintf(buf, sizeof(buf), "@R%u+,ssr", n); return make_insn(op, SH4A_KIND_MEM, "ldc.l", buf);
        case 0x4047: snprintf(buf, sizeof(buf), "@R%u+,spc", n); return make_insn(op, SH4A_KIND_MEM, "ldc.l", buf);
        case 0x40F6: snprintf(buf, sizeof(buf), "@R%u+,dbr", n); return make_insn(op, SH4A_KIND_MEM, "ldc.l", buf);
        case 0x4011: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_SYSTEM, "cmp/pz", buf);
        case 0x4015: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_SYSTEM, "cmp/pl", buf);
        case 0x4010: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "dt", buf);
        case 0x4000: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "shll", buf);
        case 0x4001: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "shlr", buf);
        case 0x4021: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "shar", buf);
        case 0x4004: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "rotl", buf);
        case 0x4005: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "rotr", buf);
        case 0x4024: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "rotcl", buf);
        case 0x4025: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "rotcr", buf);
        case 0x4008: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "shll2", buf);
        case 0x4009: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "shlr2", buf);
        case 0x4018: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "shll8", buf);
        case 0x4019: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "shlr8", buf);
        case 0x4028: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "shll16", buf);
        case 0x4029: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_REG, "shlr16", buf);
        case 0x401B: snprintf(buf, sizeof(buf), "@R%u", n); return make_insn(op, SH4A_KIND_MEM, "tas.b", buf);
        case 0x402B: snprintf(buf, sizeof(buf), "@R%u", n); return make_insn(op, SH4A_KIND_JUMP, "jmp", buf);
        case 0x400B: snprintf(buf, sizeof(buf), "@R%u", n); return make_insn(op, SH4A_KIND_JUMP, "jsr", buf);
        case 0x0023: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_JUMP, "braf", buf);
        case 0x0003: snprintf(buf, sizeof(buf), "R%u", n); return make_insn(op, SH4A_KIND_JUMP, "bsrf", buf);
        default: break;
    }

    /* Banked control */
    if ((op & 0xF08Fu) == 0x0082u) { snprintf(buf, sizeof(buf), "r%u_bank,R%u", (op >> 4u) & 7u, n); return make_insn(op, SH4A_KIND_SYSTEM, "stc", buf); }
    if ((op & 0xF08Fu) == 0x408Eu) { snprintf(buf, sizeof(buf), "R%u,r%u_bank", n, (op >> 4u) & 7u); return make_insn(op, SH4A_KIND_SYSTEM, "ldc", buf); }
    if ((op & 0xF08Fu) == 0x4083u) { snprintf(buf, sizeof(buf), "r%u_bank,@-R%u", (op >> 4u) & 7u, n); return make_insn(op, SH4A_KIND_MEM, "stc.l", buf); }
    if ((op & 0xF08Fu) == 0x4087u) { snprintf(buf, sizeof(buf), "@R%u+,r%u_bank", n, (op >> 4u) & 7u); return make_insn(op, SH4A_KIND_MEM, "ldc.l", buf); }

    /* Compact immediates */
    if ((op & 0xF000u) == 0xE000u) { snprintf(buf, sizeof(buf), "#0x%02X,R%u", low8, n); return make_insn(op, SH4A_KIND_REG, "mov", buf); }
    if ((op & 0xF000u) == 0x7000u) { snprintf(buf, sizeof(buf), "#%d,R%u", sext8(low8), n); return make_insn(op, SH4A_KIND_REG, "add", buf); }
    if (high == 0xC800u) { snprintf(buf, sizeof(buf), "#0x%02X,R0", low8); return make_insn(op, SH4A_KIND_SYSTEM, "tst", buf); }
    if (high == 0xC900u) { snprintf(buf, sizeof(buf), "#0x%02X,R0", low8); return make_insn(op, SH4A_KIND_REG, "and", buf); }
    if (high == 0xCA00u) { snprintf(buf, sizeof(buf), "#0x%02X,R0", low8); return make_insn(op, SH4A_KIND_REG, "xor", buf); }
    if (high == 0xCB00u) { snprintf(buf, sizeof(buf), "#0x%02X,R0", low8); return make_insn(op, SH4A_KIND_REG, "or", buf); }
    if (high == 0x8800u) { snprintf(buf, sizeof(buf), "#%d,R0", sext8(low8)); return make_insn(op, SH4A_KIND_SYSTEM, "cmp/eq", buf); }

    /* GBR / disp8 memory ops */
    if (high == 0xC000u) { snprintf(buf, sizeof(buf), "R0,@(0x%02X,GBR)", low8); return make_insn(op, SH4A_KIND_MEM, "mov.b", buf); }
    if (high == 0xC100u) { snprintf(buf, sizeof(buf), "R0,@(0x%02X,GBR)", low8 * 2u); return make_insn(op, SH4A_KIND_MEM, "mov.w", buf); }
    if (high == 0xC200u) { snprintf(buf, sizeof(buf), "R0,@(0x%02X,GBR)", low8 * 4u); return make_insn(op, SH4A_KIND_MEM, "mov.l", buf); }
    if (high == 0xC400u) { snprintf(buf, sizeof(buf), "@(0x%02X,GBR),R0", low8); return make_insn(op, SH4A_KIND_MEM, "mov.b", buf); }
    if (high == 0xC500u) { snprintf(buf, sizeof(buf), "@(0x%02X,GBR),R0", low8 * 2u); return make_insn(op, SH4A_KIND_MEM, "mov.w", buf); }
    if (high == 0xC600u) { snprintf(buf, sizeof(buf), "@(0x%02X,GBR),R0", low8 * 4u); return make_insn(op, SH4A_KIND_MEM, "mov.l", buf); }
    if (high == 0xCC00u) { snprintf(buf, sizeof(buf), "#0x%02X,@(R0,GBR)", low8); return make_insn(op, SH4A_KIND_MEM, "tst.b", buf); }
    if (high == 0xCD00u) { snprintf(buf, sizeof(buf), "#0x%02X,@(R0,GBR)", low8); return make_insn(op, SH4A_KIND_MEM, "and.b", buf); }
    if (high == 0xCE00u) { snprintf(buf, sizeof(buf), "#0x%02X,@(R0,GBR)", low8); return make_insn(op, SH4A_KIND_MEM, "xor.b", buf); }
    if (high == 0xCF00u) { snprintf(buf, sizeof(buf), "#0x%02X,@(R0,GBR)", low8); return make_insn(op, SH4A_KIND_MEM, "or.b", buf); }
    if (high == 0x8000u) { snprintf(buf, sizeof(buf), "R0,@(0x%X,R%u)", sk, m); return make_insn(op, SH4A_KIND_MEM, "mov.b", buf); }
    if (high == 0x8100u) { snprintf(buf, sizeof(buf), "R0,@(0x%X,R%u)", sk * 2u, m); return make_insn(op, SH4A_KIND_MEM, "mov.w", buf); }
    if (high == 0x8400u) { snprintf(buf, sizeof(buf), "@(0x%X,R%u),R0", sk, m); return make_insn(op, SH4A_KIND_MEM, "mov.b", buf); }
    if (high == 0x8500u) { snprintf(buf, sizeof(buf), "@(0x%X,R%u),R0", sk * 2u, m); return make_insn(op, SH4A_KIND_MEM, "mov.w", buf); }

    /* PC-relative & branches */
    if ((op & 0xF000u) == 0x9000u) { snprintf(buf, sizeof(buf), "@(0x%X,PC),R%u", low8 * 2u, n); return make_insn(op, SH4A_KIND_MEM, "mov.w", buf); }
    if ((op & 0xF000u) == 0xD000u) { snprintf(buf, sizeof(buf), "@(0x%X,PC),R%u", low8 * 4u, n); return make_insn(op, SH4A_KIND_MEM, "mov.l", buf); }
    if (high == 0xC700u) { snprintf(buf, sizeof(buf), "@(0x%X,PC),R0", low8 * 4u); return make_insn(op, SH4A_KIND_REG, "mova", buf); }
    if ((op & 0xF000u) == 0x1000u) { snprintf(buf, sizeof(buf), "R%u,@(0x%X,R%u)", m, sk * 4u, n); return make_insn(op, SH4A_KIND_MEM, "mov.l", buf); }
    if ((op & 0xF000u) == 0x5000u) { snprintf(buf, sizeof(buf), "@(0x%X,R%u),R%u", sk * 4u, m, n); return make_insn(op, SH4A_KIND_MEM, "mov.l", buf); }
    if ((op & 0xF000u) == 0xA000u) { snprintf(buf, sizeof(buf), "%+d", sext12(op & 0xFFFu) * 2); return make_insn(op, SH4A_KIND_BRANCH, "bra", buf); }
    if ((op & 0xF000u) == 0xB000u) { snprintf(buf, sizeof(buf), "%+d", sext12(op & 0xFFFu) * 2); return make_insn(op, SH4A_KIND_BRANCH, "bsr", buf); }
    if (high == 0x8900u) { snprintf(buf, sizeof(buf), "%+d", sext8(low8) * 2); return make_insn(op, SH4A_KIND_BRANCH_ND, "bt", buf); }
    if (high == 0x8B00u) { snprintf(buf, sizeof(buf), "%+d", sext8(low8) * 2); return make_insn(op, SH4A_KIND_BRANCH_ND, "bf", buf); }
    if (high == 0x8D00u) { snprintf(buf, sizeof(buf), "%+d", sext8(low8) * 2); return make_insn(op, SH4A_KIND_BRANCH, "bt/s", buf); }
    if (high == 0x8F00u) { snprintf(buf, sizeof(buf), "%+d", sext8(low8) * 2); return make_insn(op, SH4A_KIND_BRANCH, "bf/s", buf); }

    /* Two-register groups 0x0, 0x2, 0x3, 0x4, 0x6 */
    switch (op & 0xF00Fu) {
        case 0x0000: snprintf(buf, sizeof(buf), "R%u,@(R0,R%u)", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.b", buf);
        case 0x0001: snprintf(buf, sizeof(buf), "R%u,@(R0,R%u)", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.w", buf);
        case 0x0002: snprintf(buf, sizeof(buf), "R%u,@(R0,R%u)", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.l", buf);
        case 0x0004: snprintf(buf, sizeof(buf), "@(R0,R%u),R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.b", buf);
        case 0x0005: snprintf(buf, sizeof(buf), "@(R0,R%u),R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.w", buf);
        case 0x0006: snprintf(buf, sizeof(buf), "@(R0,R%u),R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.l", buf);
        case 0x0007: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "mul.l", buf);
        case 0x000F: snprintf(buf, sizeof(buf), "@R%u+,@R%u+", m, n); return make_insn(op, SH4A_KIND_MEM, "mac.l", buf);
        case 0x2000: snprintf(buf, sizeof(buf), "R%u,@R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.b", buf);
        case 0x2001: snprintf(buf, sizeof(buf), "R%u,@R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.w", buf);
        case 0x2002: snprintf(buf, sizeof(buf), "R%u,@R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.l", buf);
        case 0x2004: snprintf(buf, sizeof(buf), "R%u,@-R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.b", buf);
        case 0x2005: snprintf(buf, sizeof(buf), "R%u,@-R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.w", buf);
        case 0x2006: snprintf(buf, sizeof(buf), "R%u,@-R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.l", buf);
        case 0x2007: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "div0s", buf);
        case 0x2008: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "tst", buf);
        case 0x2009: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "and", buf);
        case 0x200A: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "xor", buf);
        case 0x200B: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "or", buf);
        case 0x200C: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "cmp/str", buf);
        case 0x200D: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "xtrct", buf);
        case 0x200E: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "mulu.w", buf);
        case 0x200F: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "muls.w", buf);
        case 0x3000: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "cmp/eq", buf);
        case 0x3002: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "cmp/hs", buf);
        case 0x3003: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "cmp/ge", buf);
        case 0x3004: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "div1", buf);
        case 0x3005: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "dmulu.l", buf);
        case 0x3006: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "cmp/hi", buf);
        case 0x3007: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "cmp/gt", buf);
        case 0x3008: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "sub", buf);
        case 0x300A: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "subc", buf);
        case 0x300B: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "subv", buf);
        case 0x300C: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "add", buf);
        case 0x300D: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "dmuls.l", buf);
        case 0x300E: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "addc", buf);
        case 0x300F: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "addv", buf);
        case 0x400C: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "shad", buf);
        case 0x400D: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "shld", buf);
        case 0x400F: snprintf(buf, sizeof(buf), "@R%u+,@R%u+", m, n); return make_insn(op, SH4A_KIND_MEM, "mac.w", buf);
        case 0x6000: snprintf(buf, sizeof(buf), "@R%u,R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.b", buf);
        case 0x6001: snprintf(buf, sizeof(buf), "@R%u,R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.w", buf);
        case 0x6002: snprintf(buf, sizeof(buf), "@R%u,R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.l", buf);
        case 0x6003: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "mov", buf);
        case 0x6004: snprintf(buf, sizeof(buf), "@R%u+,R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.b", buf);
        case 0x6005: snprintf(buf, sizeof(buf), "@R%u+,R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.w", buf);
        case 0x6006: snprintf(buf, sizeof(buf), "@R%u+,R%u", m, n); return make_insn(op, SH4A_KIND_MEM, "mov.l", buf);
        case 0x6007: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "not", buf);
        case 0x6008: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "swap.b", buf);
        case 0x6009: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "swap.w", buf);
        case 0x600A: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "negc", buf);
        case 0x600B: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "neg", buf);
        case 0x600C: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "extu.b", buf);
        case 0x600D: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "extu.w", buf);
        case 0x600E: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "exts.b", buf);
        case 0x600F: snprintf(buf, sizeof(buf), "R%u,R%u", m, n); return make_insn(op, SH4A_KIND_REG, "exts.w", buf);
        default: break;
    }

    snprintf(buf, sizeof(buf), "0x%04X", (unsigned)op);
    return make_insn(op, SH4A_KIND_ILLEGAL, ".word", buf);
}

bool sh4a_map_rom_offset(uint32_t vaddr, size_t rom_size, size_t* out_offset) {
    if (out_offset == nullptr) return false;
    uint32_t phys = vaddr;
    if ((vaddr & 0xE0000000u) == 0x80000000u || (vaddr & 0xE0000000u) == 0xA0000000u) {
        phys = vaddr & 0x1FFFFFFFu;
    }
    if ((size_t)phys >= rom_size) return false;
    *out_offset = (size_t)phys;
    return true;
}

uint32_t sh4a_crc32(const uint8_t* data, size_t len) {
    if (data == nullptr && len > 0) return 0u;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint32_t)data[i];
        for (int b = 0; b < 8; ++b) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}
