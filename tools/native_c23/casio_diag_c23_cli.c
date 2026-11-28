/*
 * tools/native_c23/casio_diag_c23_cli.c
 * Standalone ISO C23 SH-4A disassembler, ROM CRC32 diagnostic CLI, and self-test.
 */

#include "sh4a_disasm_c23.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_self_test(void) {
    int pass = 0, fail = 0;
    #define CHK(cond, name) do { \
        if (cond) { ++pass; printf("[PASS] %s\n", name); } \
        else { ++fail; fprintf(stderr, "[FAIL] %s\n", name); } \
    } while (0)

    Sh4aDecodedInsn i1 = sh4a_decode_opcode(0x0009u);
    CHK(i1.kind == SH4A_KIND_NOP && strcmp(i1.mnemonic, "nop") == 0, "sh4a_nop");

    Sh4aDecodedInsn i2 = sh4a_decode_opcode(0x62E1u);
    CHK(i2.kind == SH4A_KIND_MEM && strcmp(i2.mnemonic, "mov.w") == 0 && strcmp(i2.operands, "@R14,R2") == 0, "sh4a_mov_w_r14_r2");

    Sh4aDecodedInsn i3 = sh4a_decode_opcode(0x89FBu);
    CHK(i3.kind == SH4A_KIND_BRANCH_ND && strcmp(i3.mnemonic, "bt") == 0 && strcmp(i3.operands, "-10") == 0, "sh4a_bt_neg10");

    Sh4aDecodedInsn i4 = sh4a_decode_opcode(0x4F22u);
    CHK(i4.kind == SH4A_KIND_MEM && strcmp(i4.mnemonic, "sts.l") == 0 && strcmp(i4.operands, "pr,@-R15") == 0, "sh4a_sts_l_pr");

    Sh4aDecodedInsn i5 = sh4a_decode_opcode(0xC32Au);
    CHK(i5.kind == SH4A_KIND_SYSTEM && strcmp(i5.mnemonic, "trapa") == 0 && strcmp(i5.operands, "#0x2A") == 0, "sh4a_trapa");

    size_t off = 0;
    CHK(sh4a_map_rom_offset(0x80002D1Au, 0x2000000u, &off) && off == 0x2D1Au, "sh4a_map_p1_rom");
    CHK(!sh4a_map_rom_offset(0xA4000000u, 0x2000000u, &off), "sh4a_map_oob_rejected");

    const uint8_t msg[9] = { '1','2','3','4','5','6','7','8','9' };
    CHK(sh4a_crc32(msg, 9u) == 0xCBF43926u, "sh4a_crc32_standard_vector");

    printf("SUMMARY: passed=%d failed=%d\n", pass, fail);
    if (fail == 0) {
        printf("SH4A_DISASM_C23_TESTS_PASS\n");
        return 0;
    }
    return 1;
}

int main(int argc, char** argv) {
    if (argc < 2 || strcmp(argv[1], "--self-test") == 0) {
        return run_self_test();
    }
    if (strcmp(argv[1], "--hex") == 0) {
        for (int i = 2; i < argc; ++i) {
            unsigned long v = strtoul(argv[i], NULL, 16);
            Sh4aDecodedInsn insn = sh4a_decode_opcode((uint16_t)(v & 0xFFFFu));
            printf("0x%04X  %-8s %s\n", (unsigned)insn.opcode, insn.mnemonic, insn.operands);
        }
        return 0;
    }
    fprintf(stderr, "Uso: casio_diag_c23_cli [--self-test | --hex <op1> ...]\n");
    return 2;
}
