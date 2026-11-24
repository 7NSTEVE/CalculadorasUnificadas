# Auditoría de Autenticidad — Núcleo Nativo Casio fx-CG50 (`v44`)

## Resumen Ejecutivo

- **Estado de Autenticidad Matemática:** `CONFIRMED` — **Cero HLE matemático** en producción, Android y certificación (`host_math_fallback_allowed: false`).
- **Compilación de HLE legado:** `CG50_ENABLE_LEGACY_DEBUG_HLE` está **desactivado en tiempo de compilación** en todos los targets (`casio_cg50`, `casio_native`, `cg50_battery`, `cg50_validation_runner`, `cg50_hw_torture_runner`, `cg50_fuzzer` y Android Release APK).
- **Arquitectura de CPU confirmada (`CPU73050.dll`):** **Renesas SH7305 (`SH4AL-DSP`) SIN FPU hardware**.
  - Evidencia en `CPU73050.dll` (`715,568 B`): **`0`** mnemónicos/registros FPU (`fmov`, `fadd`, `fpscr`, `fpul`, `fsca`, `ftrv`, `fpu`); **`40+`** símbolos SH4AL-DSP (`padd`, `psub`, `pmuls`, `movx`, `movy`, `ldrs`, `ldre`, `DSR`, `MOD`, `SPU2DSP0/1`).
  - Modo estricto disponible: `Cg50NativeCore::SetStrictSh7305NoFpu(true)` genera `EXPEVT 0x180` (`General Illegal Instruction`) ante cualquier opcode `0xFxxx` fuera del espacio SH4AL-DSP (`SR.DSP=0`).
  - Verificación en vivo sobre el firmware oficial Casio OS 3.80 (arranque en frío + batería `133/133` + 24 apps `cert2900` + suites extremas): **`fpuOpcodesExecuted == 0`**.
  - `sh_fpu.h` conserva soporte opt-in para código SH-4A legado con corrección verificada de redondeo IEEE-754 `RM=01` (`TowardZero`), eliminando el bug histórico de truncación entera `floor()`/`ceil()`.

## Evidencia de Ejecución Real del Firmware (BCD ALU `0x04CB0000`)

Todos los cálculos numéricos de la calculadora provienen del firmware oficial de Casio ejecutando instrucciones SH7305 reales y la unidad hardware decimal **BCD ALU** (`FASCR` `0x04CB0010`, `FASSRA` `0x04CB0014`, `FASSRB` `0x04CB0018`, `FASDR` `0x04CB001C`, alias `0x04CB000C`):

| Corrida Real en `v44` | Instrucciones SH7305 | Ops Hardware BCD ALU (`0x04CB0010`) | Opcodes FPU (`0xFxxx`) | Comandos LCD Desconocidos | Resultado |
|---|---:|---:|---:|---:|---|
| Arranque en frío (`gen_cg50_warm_state`) | `164,466,762` | Medido en arranque | `0` | `0` | `CONFIRMED` (`8.82 s`, `PC=0xA00206FC`) |
| Batería oficial (`cg50_battery`, `CG50_CERT_STRICT_NO_HLE=1`) | `> 10^9` | Activo en 133/133 | `0` | `0` | `CONFIRMED` (`133/133 PASO`, `0` divergencias, `2/2` determinista) |
| Matriz 24 apps + add-ins (`cert2900`, `CG50_MMIO_AUDIT=1`) | `2,103,862,284` | **`3,804,147`** | `0` | `0` | `CONFIRMED` (`24/24`, `unsup=0`, `mflt=0`) |

## Inventario Completo de Hooks, Overrides y Frontera Manager PLUS vs. Hardware Físico

Para garantizar transparencia total, cada punto especial en `core/casio_cg50/cg50_native_core.cpp` fue auditado y clasificado:

| Mecanismo | Ubicación | Estado en `v44` | Naturaleza y Justificación |
|---|---|---|---|
| `CheckResultHlePending()` / `CG50_RESULT_FIELD_HLE` | `cg50_native_core.cpp` | **BLOQUEADO** (`#ifdef CG50_ENABLE_LEGACY_DEBUG_HLE`, OFF) | Antiguo prototipo de depuración (2026-10). No se compila ni existe en binarios de producción o certificación. |
| `CG50_APP_KEY_HLE`, `CG50_WORK_HLE`, `CG50_1D40_LEGACY` | `cg50_native_core.cpp` | **BLOQUEADO** (`#ifdef CG50_ENABLE_LEGACY_DEBUG_HLE`, OFF) | Banderas experimentales históricas eliminadas de compilación estándar y auditadas en `AuditForbiddenEnvOverrides()`. |
| `AuditForbiddenEnvOverrides()` | `cg50_native_core.cpp` | **ACTIVO** (`0` variables prohibidas permitidas) | Valida 12 variables de entorno de override (`CG50_RESULT_FIELD_HLE`, `CG50_APP_KEY_HLE`, `CG50_WORK_HLE`, `CG50_1D40_LEGACY`, `CG50_KEY_TASK`, `CG50_KEY_DIRECT`, `CG50_UTLB_AUTODIRTY`, `CG50_HOST_HEARTBEAT`, `CG50_FORCE_RB0`, `CG50_TRAP_F0000_RES`, `CG50_TRAP_F000C_PENDING`, `CG50_PBDR_HOST`). |
| Traps `0xF0010000..0xF00100FF` | `cg50_native_core.cpp` | **IPC de ROM Manager PLUS** (contado en `appIfTrapsExecuted`) | Interfaz `GCalAppIFA.dll` propia de la ROM `fx-CG Manager PLUS` para eventos UI/buzón de host. **Cero cálculo matemático**; no calcula ni inyecta resultados. |
| `kExamDecouplingEnabled` (`0x00382E84` tras `146M` inst) | `cg50_native_core.cpp` | **Documentado abiertamente** (desactivable con `CG50_EXAM_DECOUPLING=0`) | Desacopla la comprobación de Exam Mode específica de la imagen Manager PLUS tras el arranque anti-tamper. No modifica ningún cálculo ni periférico matemático. |
| `kScanConsistencyEnabled` (`0x8038B1D0`) | `cg50_native_core.cpp` | **Documentado abiertamente** | Alinea `c68 <- c48` en escaneo inicial de bloques de memoria en la imagen Manager PLUS para evitar error de montaje FS en el arranque. |

## Cierre de Cobertura de Hardware SH7305 (`CG50_HARDWARE_MASTER.json` v44)

Tras cerrar Fase 3 (SH4AL-DSP, caché IC/OC de 4 vías + OCRAM 16 KB, ITLB real de 4 entradas, INTC2/NMI/USERIMASK, CMT IRQ, WDT watchdog-reset, ADC corregido a `0x04610080..8E`, DD Bus `0x044C0000`, SDHI `0x04CF001C..22`, SCIF0 con cable virtual peer-to-peer e IRQs, USB 2.0 completo con VBUS/SETUP/Pipes/FIFOs/IRQ, UBC breakpoints hardware, R61524 scroll/mirror/CABC/blanking/FMARK y Flash CFI DQ6/DQ7):

- **Componentes inventariados:** `39` totales (`28` presentes en placa fx-CG50 según `CGM.dlm`, `11` presentes en silicio SH7305 pero no cableados en fx-CG50).
- **Registros SH7305 extraídos de `CPU73050.dll`:** `702` totales (`413` aplicables a fx-CG50, `289` `NOT_PRESENT_ON_CG50`).
  - `FULL_SILICON`: **`222`** registros (subió desde `97` en `v43`).
  - `FUNCTIONAL_PARTIAL`: **`78`** registros.
  - `PASSIVE_BACKING`: **`113`** registros (bancos PFC/BSC no usados, alarmas RTC, HUDI/PRLCK).
  - `UNMAPPED`: **`0`** registros.
- **Periféricos sin mapear observados en 2.1 mil millones de instrucciones (`cert2900`):** **`0`** (SDHI `0x04CF001C..22` completamente absorbido por `SdhiUnit`).

## Referencia de Comparación

- **Emulador oficial Casio `fx-CG Manager PLUS` (local):** `DISPONIBLE` (utilizado para certificación diferencial y verificación de fidelidad BCD/errores/gráficos).
- **Oráculo matemático independiente (`mpmath` 50 dígitos + `SymPy`):** `DISPONIBLE` (externo al emulador, cero circularidad).
- **Calculadora física fx-CG50:** `PENDING` (`physical_reference_verified: false` — declarado con honestidad al no contar con unidad física en esta sesión).
