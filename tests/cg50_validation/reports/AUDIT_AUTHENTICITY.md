# Auditoría de Autenticidad — Núcleo Nativo Casio fx-CG50 (`v47`)

## Resumen Ejecutivo

- **Estado de Autenticidad Matemática:** `CONFIRMED` — **Cero HLE matemático** en producción, Android y certificación (`host_math_fallback_allowed: false`).
- **Compilación de HLE legado:** `CG50_ENABLE_LEGACY_DEBUG_HLE` está **desactivado en tiempo de compilación** en todos los targets (`casio_cg50`, `casio_native`, `cg50_battery`, `cg50_validation_runner`, `cg50_cpu_dsp_cert_lab`, `cg50_register_cert_lab`, `cg50_combined_hw_stress`, `cg50_hw_torture_runner`, `cg50_fuzzer` y Android Release APK).
- **Arquitectura de CPU confirmada (`CPU73050.dll` en vivo + desensamblado):** **Renesas SH7305 (`SH4AL-DSP`) SIN FPU hardware**.
  - **Modo estricto por defecto (`H-003`):** `strictSh7305NoFpu_ = true` por defecto en `Cg50NativeCore` — cualquier opcode `0xFxxx` fuera del espacio SH4AL-DSP (`SR.DSP=0`) o `0x4n5A/4n52/4n56` genera `EXPEVT 0x180` (`General Illegal Instruction`).
  - **Perfil `StrictSilicon` por defecto (`H-004`):** `examDecouplingEnabled_ = false` por defecto (`examModePatchHits == 0` en arranque, 24 apps, batería `133/133` y estrés `120K`). El desacoplamiento de Exam Mode (`0x00382E84`) solo existe bajo perfil explícito `HardwareProfile::ManagerPlusCompat` o `CG50_EXAM_DECOUPLING=1`.
  - **Verificación en vivo sobre el firmware oficial Casio OS 3.80:** `fpuOpcodesExecuted == 0` en todas las ejecuciones.

## Evidencia de Certificación de Silicio y Autenticidad (`v47`)

| Dimensión | Evidencia Medida en `v47` | Resultado |
|---|---|---|
| **Oráculo vivo `CPU73050.dll` (x86 32-bit, R/O)** | `65,536` encodings · CPU16 `923/928` exactos + `5` quirk DLL (`0` bugs core) · DSP16 `16/16` · DSP32 `136/140` exactos (`140/140` spec Renesas) + `116/116` ilegales | **CONFIRMED** |
| **Vectores de operandos CPU + BCD ALU (`0x04CB0010`)** | `9,000,000` CPU ALU/Mul/Logic (`0` fallos) + `1,000,000` BCD ALU 6 modos + alias `0x0C` (`0` fallos) | **CONFIRMED** |
| **Registros y campos de bits SH7305** | `413/413` CG50 ejercitados · `413/413` save/restore (chunk 112 USB2 pipe banks) · `8,016/8,016` bits clasificados · `289/289` no-CG50 seguros | **CONFIRMED** |
| **Excepciones e interrupciones (`EXPEVT`/`INTEVT`)** | `52/56` ranuras verificadas (`4` HW-pin/reset declaradas en `UNVERIFIED_COMPONENTS.json`) | **CONFIRMED** |
| **Estrés multi-periférico 16 hilos (`CG50_MMIO_AUDIT=1`)** | `120,000` secuencias · `30,624` inyecciones de fallo OK · **`0` fallos** · `fpu=0` · `exam_patch=0` (`H-005` mutex thread-safe) | **CONFIRMED** |
| **Gráficos independientes RGB565 (`H-009`)** | `10/10` benchmarks píxel-a-píxel (`100.000%`, `MAE=0.00`) + `COMP-ULTRA-MATH` + `3/3` negativos rechazados | **CONFIRMED** |
| **Integridad ROM + Persistencia atómica (`H-006`/`H-007`)** | SHA-256 FIPS 180-4 en las 8 ROMs staged; escritura atómica `MoveFileExA`/`rename` en C++ y Dart | **CONFIRMED** |
| **Inventario empírico cero-heurísticas (`H-001`)** | `CG50_HARDWARE_MASTER.json` (`schema 2.0.0`) derivado exclusivamente de artefactos medidos | **CONFIRMED** |

## Inventario Transparente de Fronteras `Manager PLUS` vs. Silicio

| Mecanismo | Estado en `v47` | Naturaleza y Justificación |
|---|---|---|
| `CheckResultHlePending()` / `CG50_RESULT_FIELD_HLE` / `CG50_APP_KEY_HLE` / `CG50_WORK_HLE` | **NO COMPILADO** (`#ifdef CG50_ENABLE_LEGACY_DEBUG_HLE`, OFF) | Eliminado de producción/certificación; auditado en `AuditForbiddenEnvOverrides()`. |
| `examDecouplingEnabled_` (`0x00382E84`) | **DESACTIVADO POR DEFECTO** (`StrictSilicon`, `examModePatchHits == 0`) | Opt-in exclusivo bajo `HardwareProfile::ManagerPlusCompat` (`H-004`). |
| Traps IPC `0xF0010000..0xF00100FF` | **IPC de host ROM Manager PLUS** (`appIfTrapsExecuted`) | Buzón de eventos UI `GCalAppIFA.dll`; **cero cálculo matemático**. |
| `kScanConsistencyEnabled` (`0x8038B1D0`) | **Documentado abiertamente** | Alinea `c68 <- c48` en el montaje inicial de bloques FS de la imagen `Manager PLUS`. |

## Estado Honesto de Referencias de Comparación

- **Oráculo vivo `CPU73050.dll` + `fx-CG Manager PLUS` (local):** `CONFIRMED` (`54/54` casos UI + `1,456` vectores live CPU/DSP).
- **Oráculos matemáticos y gráficos independientes (`mpmath` 50 dígitos, `SymPy`, renderizador RGB565):** `CONFIRMED` (cero circularidad).
- **Calculadora física Casio fx-CG50:** **`NOT_YET_VERIFIED`** (`physical_verified: false` — nunca marcada como `N/A_DISCARDED_BY_DESIGN`).
