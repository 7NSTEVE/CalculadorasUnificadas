#pragma once

#include <cstdint>

// 16-bit host codes for DLSim `_DLSIM_GCAIKeyPress@8`.
// Encoding: (inputLine << 8) | outputLine, inputLine = KI (1..7),
// outputLine = KO (1..10). El keycode gint equivalente es
// ((outputLine - 1) << 4) | (8 - inputLine)  [keysc: (row << 4) + (7 - col)].
// F1 = (0x07, 0x0A) from the fx-CG syscall notes (GetKeyWait / PRGM_GetKey).
// Tabla corregida 2026-10-17 contra gint keycodes.h/keysc.c: la tabla anterior
// era una permutacion (solo F1/LN/DEL/AC coincidian; kd4 envio UP/VARS/RIGHT/
// LEFT/F-D donde se pretendian digitos y MENU -> corrida funcional no valida).
struct Cg50KeyDef {
    int id;
    int cx, cy, w, h;
    std::uint16_t matrixCode;
    const wchar_t* name;
};

static constexpr Cg50KeyDef g_cg50ExactKeys[] = {
    { 0,  112,  559, 54, 48, 0x070A, L"F1" },
    { 1,  200,  559, 54, 48, 0x060A, L"F2" },
    { 2,  288,  559, 54, 48, 0x050A, L"F3" },
    { 3,  376,  559, 54, 48, 0x040A, L"F4" },
    { 4,  464,  559, 54, 48, 0x030A, L"F5" },
    { 5,  552,  559, 54, 48, 0x020A, L"F6" },
    { 6,  115,  642, 64, 42, 0x0709, L"SHIFT" },
    { 7,  202,  642, 64, 42, 0x0609, L"OPTN" },
    { 8,  289,  642, 64, 42, 0x0509, L"VARS" },
    { 9,  376,  642, 64, 42, 0x0409, L"MENU" },
    { 10, 512,  628, 52, 42, 0x0209, L"ARRIBA" },
    { 11, 460,  676, 42, 52, 0x0309, L"IZQUIERDA" },
    { 12, 564,  676, 42, 52, 0x0208, L"DERECHA" },
    { 13, 512,  724, 52, 42, 0x0308, L"ABAJO" },
    { 14, 115,  711, 64, 42, 0x0708, L"ALPHA" },
    { 15, 202,  711, 64, 42, 0x0608, L"X^2" },
    { 16, 289,  711, 64, 42, 0x0508, L"POT" },
    { 17, 376,  711, 64, 42, 0x0408, L"EXIT" },
    { 18, 112,  789, 64, 42, 0x0707, L"X_THETA" },
    { 19, 200,  789, 64, 42, 0x0607, L"LOG" },
    { 20, 288,  789, 64, 42, 0x0507, L"LN" },
    { 21, 376,  789, 64, 42, 0x0407, L"SIN" },
    { 22, 464,  789, 64, 42, 0x0307, L"COS" },
    { 23, 552,  789, 64, 42, 0x0207, L"TAN" },
    { 24, 112,  858, 64, 42, 0x0706, L"FRAC" },
    { 25, 200,  858, 64, 42, 0x0606, L"SD" },
    { 26, 288,  858, 64, 42, 0x0506, L"PAREN_A" },
    { 27, 376,  858, 64, 42, 0x0406, L"PAREN_C" },
    { 28, 464,  858, 64, 42, 0x0306, L"COMA" },
    { 29, 552,  858, 64, 42, 0x0206, L"FLECHA" },
    { 30, 121,  956, 88, 56, 0x0705, L"7" },
    { 31, 226,  956, 88, 56, 0x0605, L"8" },
    { 32, 332,  956, 88, 56, 0x0505, L"9" },
    { 33, 437,  956, 88, 56, 0x0405, L"DEL" },
    { 34, 542,  956, 88, 56, 0x0101, L"AC" },
    { 35, 121, 1038, 88, 56, 0x0704, L"4" },
    { 36, 226, 1038, 88, 56, 0x0604, L"5" },
    { 37, 332, 1038, 88, 56, 0x0504, L"6" },
    { 38, 437, 1038, 88, 56, 0x0404, L"POR" },
    { 39, 542, 1038, 88, 56, 0x0304, L"ENTRE" },
    { 40, 121, 1120, 88, 56, 0x0703, L"1" },
    { 41, 226, 1120, 88, 56, 0x0603, L"2" },
    { 42, 332, 1120, 88, 56, 0x0503, L"3" },
    { 43, 437, 1120, 88, 56, 0x0403, L"MAS" },
    { 44, 542, 1120, 88, 56, 0x0303, L"MENOS" },
    { 45, 121, 1202, 88, 56, 0x0702, L"0" },
    { 46, 226, 1202, 88, 56, 0x0602, L"PUNTO" },
    { 47, 332, 1202, 88, 56, 0x0502, L"EXP" },
    { 48, 437, 1202, 88, 56, 0x0402, L"ANS" },
    { 49, 542, 1202, 88, 56, 0x0302, L"EXE" },
};

static constexpr int NUM_CG50_KEYS =
    static_cast<int>(sizeof(g_cg50ExactKeys) / sizeof(g_cg50ExactKeys[0]));

constexpr bool Cg50KeyCodesAreUnique()
{
    for (int i = 0; i < NUM_CG50_KEYS; ++i) {
        for (int j = i + 1; j < NUM_CG50_KEYS; ++j) {
            if (g_cg50ExactKeys[i].matrixCode == g_cg50ExactKeys[j].matrixCode) {
                return false;
            }
        }
    }
    return true;
}

static_assert(NUM_CG50_KEYS == 50, "CG50 faceplate expects 50 keys");
static_assert(Cg50KeyCodesAreUnique(), "CG50 matrix codes must be unique 16-bit codes");
static_assert(g_cg50ExactKeys[0].matrixCode == 0x070A, "F1 is (input=7, output=10)");
static_assert(g_cg50ExactKeys[34].matrixCode == 0x0101, "AC/ON is 0x0101");
