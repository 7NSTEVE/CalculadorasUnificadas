# Inventario Técnico Ampliado Casio fx-CG50 OS `3.80.0212` (Fase 3 `v46`)

- **Fecha UTC:** `2026-11-24T08:15:10.143063+00:00` | **Git HEAD:** `ab66ee1923afd3ff413ab5f33df917e9d9a9aa59`
- **SystemROM.raw SHA-256:** `8CF1E1F2AD3FB742B557D00535F8AEA7874C7F8D138B56B4C3938E26C0F82E23`

## 1. Denominadores Técnicos Auditados (Sin Cifra Comercial Forzada)

| Dimensión técnica | Denominador real extraído | Fuente directa |
|---|---|---|
| Syscalls Casio OS 3.80 (`0x0000..0x1F67`) | **`8,040`** (`8,030` manejadores únicos) | `SystemROM.raw @ 0x006B01DC` |
| Syscalls cruzadas con `gint`/PrizmSDK | **`80`** | Tabla canónica `gint`/fxSDK |
| Mensajes de error/excepción internos | **`141`** | `SystemROM.raw` |
| Cadenas de diagnóstico / modos internos | **`34`** | `SystemROM.raw` |
| Add-ins `.g3a` embebidos detectados (`~USBPower`) | **`61`** | `C2ROM.raw` / `UserROM.raw` |
| Combinaciones ortogonales de `Setup` | **`1152`** | Matriz `Angle×IO×Complex×Disp×Frac×Func×Deriv` |

## 2. Diff Binario Entre Variantes Regionales Oficiales de Casio (`31,429,936 B` c/u)

| Comparativa | Bytes diferentes | Bloques 4 KB diferentes | Identidad (%) |
|---|---|---|---|
| `fx-CG50_International_vs_fx-CG50_AU` | `0` / `31,429,936` | `0` | `100.000000%` |
| `fx-CG50_International_vs_GRAPH90+E_France` | `26,885,729` / `31,429,936` | `7,308` | `14.458213%` |
| `fx-CG50_AU_vs_GRAPH90+E_France` | `26,885,729` / `31,429,936` | `7,308` | `14.458213%` |

## 3. Matriz Comparativa vs. Ecosistema Comunitario SH7305 (`hexbinoct/casio-cg50`, `gint`, `Simon Lothar`, `redoste`)

| Componente | `hexbinoct/casio-cg50` | `gint` | Nuestro núcleo `v46` |
|---|---|---|---|
| SH4AL-DSP (16b MOVX/MOVY/MOVS + 32b Parallel + SR.DSP) | Pending (omitted per Open Questions) | Supported on HW | IMPLEMENTED + UNIT_VERIFIED (65,536 opcodes & 256 DSP2 audited vs CPU73050.dll) |
| ITLB + Instruction/Data Cache (IC/OC + OCRAM + RAMCR 0x3E0) | Pending (ITLB & IC omitted) | Used on HW | IMPLEMENTED + UNIT_VERIFIED (IC/OC/ITLB/UTLB/RAMCR) |
| Spansion S29GL256P NOR Flash CFI (Autoselect/CFI/WriteBuffer/Suspend) | Partial CFI ID reads | Bfile/Fugue | IMPLEMENTED + UNIT_VERIFIED + LIVE SMEM FUGUE FTL |
| USB 2.0 Function Controller (57 regs + Virtual Host BOT/SCSI/P7/ScreenRecv) | No full USB emulation | fxlink USB driver | IMPLEMENTED + LIVE TESTED |
| SCIF0 3-pin Serial + Interrupts + Virtual Link | No serial IRQs | Serial driver | IMPLEMENTED + PEER-TO-PEER VERIFIED |
| DMAC0 (6 ch + DMAOR round-robin + DMARS0..2 + AE error detection) | Partial LCD DMA issues | gint DMA driver | IMPLEMENTED + UNIT_VERIFIED (including AE negative test) |
| CPG (14 regs + FRQCR/PLL/FLL Hz + MSTPCR0/2 active gating) | Clock frequency discrepancies | Ptune/CPG driver | IMPLEMENTED + UNIT_VERIFIED vs CPU73050.dll |
| PFC (86 regs) + ExtLY755A (POWERON/DDPOWER/USB_DET/_AppIF) | Incomplete keyboard/power pins | KEYSC/PFC driver | IMPLEMENTED + UNIT_VERIFIED vs CGM.dlm & ExtLY755A.dll |
| BCD Hardware ALU (0xA4CB0010..1C) | Physically tested (202 cases) | OS-internal | IMPLEMENTED + 1M TORTURE + 133/133 BATTERY + 50+ MANAGER PLUS DIFF |
| BSC SDRAM Refresh (RTCSR 0xA55A key + RTCNT/RTCOR) + HUDI + PRLCK + SDHI | Passive / omitted | Partial | IMPLEMENTED + UNIT_VERIFIED vs CPU73050.dll dataflow |
