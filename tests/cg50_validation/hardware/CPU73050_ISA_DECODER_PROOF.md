# Prueba Forense por Desensamblado Real de `CPU73050.dll`, `LCDR61524A.dll` y `ExtLY755A.dll` (v2)

- **Fecha UTC:** `2026-11-24T07:55:16.440382+00:00` | **Git HEAD:** `ab66ee1923afd3ff413ab5f33df917e9d9a9aa59`
- **Alcance (rigor metodológico):** Static decoder classification (65,536/65,536 16-bit opcodes + 256/256 DSP 2nd-word classes + 64/64 EXPEVT slots) proves decoder table architecture and dispatch targets in Casio CPU73050.dll. Runtime semantic certification is proven separately by CTest, ASan/UBSan, 12-metric coverage, mutation testing, and 50+ live Manager PLUS differential runs.

## 1. Binarios Oficiales Auditados (SHA-256)

| Binario | Tamaño (bytes) | SHA-256 |
|---|---|---|
| `CPU73050.dll` | `715,568` | `CC0C58E78E9BBB42242FFF7653EF46BF9DE1E332058C0CFFD611936355C2098B` |
| `LCDR61524A.dll` | `95,536` | `36F4CA9F21AA46CC4D7D3FD6B5EE3E545F1B078249ACBDF2FF033CEF5D6BD570` |
| `ExtLY755A.dll` | `66,352` | `77F7A73A2B2DE67CEE05B3DB6D57805A1916D4B3BDEAAE6EB4431FECC4B535A7` |
| `NOR29GLN2A.dll` | `55,600` | `95FB812405BB60A28ABD161D0DB7863AA9F03471D55B2940F5A3E75FD2917B2F` |
| `SerLY755A.dll` | `50,992` | `0A1EF405490F213EED9793C06FB37D7DF72732265A01B107F997AE340CD7FC02` |
| `FileSDSimC.dll` | `54,064` | `85E93A6ED76FB81A90B5A9B8D395C5762723B18A924D5342DB10105982D8ACCA` |

## 2. Clasificación Exhaustiva del Decodificador (65,536 + 256 DSP + 64 EXPEVT)

- **16-bit opcodes (`0x1001605C`):** `65,536` clasificados (`59,919` activos en `227` clases + `5,617` ilegales; inventario completo en `CPU73050_OPCODES_65536.json`).
- **32-bit DSP 2do word (`0x10092470`):** `256` clases (`140` activas + `116` ilegales → `0x10010270`).
- **Tabla `EXPEVT` (`0x10093990`):** **`64 / 64` ranuras preservadas** (`56` válidas + `8` marcadas explícitamente `INVALID_SLOT_0xFFFF`).
- **FPU IEEE-754:** `0 / 65,536` opcodes y `0` códigos `EXPEVT` (`0x120=False`, `0x800=False`, `0x820=False`).

## 3. Verificación Cruzada Automatizada contra `cg50_native_core.cpp`

| ID | Requisito extraído de `CPU73050.dll` | Estado en `cg50_native_core.cpp` |
|---|---|---|
| `AUTO-CHK-01` | 0xF000..0xF3FF routed to 16-bit SH4AL-DSP MOVX/MOVY when SR.DSP=1 | **`CONFIRMED`** |
| `AUTO-CHK-02` | 0xF800..0xFBFF raises EXPEVT 0x1A0 inside delay slot (executingDelaySlot_) | **`CONFIRMED`** |
| `AUTO-CHK-03` | RAMCR (0xFF000074) masks writes with 0x000003E0u per CPU73050.dll 0x1003F34D | **`CONFIRMED`** |
| `AUTO-CHK-04` | 0x4n5A / 0x0n5A (FPUL) raises Illegal Instruction under strictSh7305NoFpu_ | **`CONFIRMED`** |
| `AUTO-CHK-05` | 0x4n6A..0x4nBA & 0x0n6A..0x0nBA map to DSR/A0/X0/X1/Y0/Y1 gated on SR.DSP | **`CONFIRMED`** |
| `AUTO-CHK-06` | BSC RTCSR/RTCNT/RTCOR enforce 0xA55A0000 write-protection key + CMF W0C + active refresh counter | **`CONFIRMED`** |
