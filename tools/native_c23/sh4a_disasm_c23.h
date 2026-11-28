#pragma once
/*
 * tools/native_c23/sh4a_disasm_c23.h
 * Native ISO C23 SH-4/SH-4A disassembler & ROM diagnostic library ported 1:1 from
 * E:\Proyectos\Calculadoras (blanca y gris)\Investigacion\github-cg50-native-rom-core\
 * Calculadora Unificada\tools\disasm_sh4a.py aligned with Cg50NativeCore::ExecuteOpcode().
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Sh4aOpcodeKind {
    SH4A_KIND_REG = 0,
    SH4A_KIND_MEM = 1,
    SH4A_KIND_BRANCH = 2,
    SH4A_KIND_BRANCH_ND = 3,
    SH4A_KIND_JUMP = 4,
    SH4A_KIND_SYSTEM = 5,
    SH4A_KIND_NOP = 6,
    SH4A_KIND_ILLEGAL = 7
} Sh4aOpcodeKind;

typedef struct Sh4aDecodedInsn {
    uint16_t opcode;
    Sh4aOpcodeKind kind;
    char mnemonic[16];
    char operands[48];
} Sh4aDecodedInsn;

/* Decodes a single 16-bit big-endian SH-4/SH-4A instruction word. */
Sh4aDecodedInsn sh4a_decode_opcode(uint16_t op);

/* Maps virtual P1 (0x8000xxxx) / P2 (0xA000xxxx) address into physical ROM offset.
 * Returns true if within [0, rom_size). */
bool sh4a_map_rom_offset(uint32_t vaddr, size_t rom_size, size_t* out_offset);

/* Computes IEEE 802.3 CRC32 over buffer in pure ISO C23 (ROM/SMEM diagnostic). */
uint32_t sh4a_crc32(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif
