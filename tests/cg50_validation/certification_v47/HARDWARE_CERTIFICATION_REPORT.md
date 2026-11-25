# Reporte de Certificación Exhaustiva de Hardware SH7305 / Casio fx-CG50 (`v47`)

- **Fecha (UTC):** `2026-11-24T23:24:06.656777+00:00`
- **Commit base:** `acb7468b87675fec226fce73eef5ef8f28e1c5eb`
- **Referencia física:** **`NOT_YET_VERIFIED`** (`physical_verified = false` — política estricta sin `N/A_DISCARDED_BY_DESIGN`)
- **Oráculos utilizados:** `CPU73050.dll` en vivo (x86 32-bit, solo lectura, cero parches `.text`), `LCDR61524A.dll`, `ExtLY755A.dll`, `CGM.dlm`, especificación Renesas SH-4A/SH4AL-DSP (`REJ09B0003-0400`), `fx-CG Manager PLUS` vivo (`54/54`), `mpmath`/`SymPy` 50 dígitos y renderizador independiente RGB565 (`10/10` exactos + `3/3` negativos rechazados).

## 1. Resumen de Verificación Empírica (`CONFIRMED / PENDING`)

| Dimensión Certificada | Resultado Medido | Estado | Evidencia Directa |
|---|---:|---|---|
| **Censo 16-bit `CPU73050.dll`** | `65,536` (`59,919` activos / `5,617` ilegales / `0` FPU) | **CONFIRMED** | `cpu73050_live_census.json` |
| **Diff en vivo CPU 16-bit vs `CPU73050.dll`** | `923/928` exactos + `5` quirk DLL (`0` bugs core) | **CONFIRMED** | `CPU_OPCODE_DIFFERENTIAL_RESULTS.json` |
| **Diff en vivo DSP16 `MOVS`** | `16/16` exactos (`0` div) | **CONFIRMED** | `DSP_DIFFERENTIAL_RESULTS.json` |
| **Diff en vivo DSP32 (256 clases `op2`)** | `136/140` exactos DLL (`140/140` spec Renesas) + `116/116` ilegales (`0` bugs core) | **CONFIRMED** | `DSP_DIFFERENTIAL_RESULTS.json` |
| **Vectores de operandos sembrados** | **`10,000,000`** (`9,000,000` CPU + `1,000,000` BCD ALU, **`0` fallos**) | **CONFIRMED** | `CPU_OPCODE_DIFFERENTIAL_RESULTS.json` |
| **Ranuras `EXPEVT` / `INTEVT`** | **`52 / 56`** (`100%` no-pin; `4` HW-pin/reset declaradas honestamente) | **CONFIRMED** | `EXCEPTION_CERTIFICATION_MATRIX.json` |
| **Registros CG50 (702 SoC totales)** | **`413 / 413`** ejercitados · **`413 / 413`** save/restore · **`289 / 289`** no-CG50 seguros | **CONFIRMED** | `REGISTER_BY_REGISTER_MATRIX.csv` |
| **Campos de bits CG50** | **`8,016 / 8,016`** bits clasificados empíricamente (`RW=5710`, `RO0=2109`, `RO1=60`, `SPECIAL=137`) | **CONFIRMED** | `BITFIELD_CERTIFICATION_MATRIX.csv` |
| **Estrés multi-periférico + inyección de fallos (16H)** | **`120,000`** secuencias · **`30,624`** fallos inyectados OK · **`0` fallos** · `fpu=0` · `exam_patch=0` | **CONFIRMED** | `combined_hw_stress_report.json` |
| **Campaña de mutación en sandbox aislado** | **`112 / 112`** (`100.0%`, sobrevivientes=`0`, build_err=`0`) | **CONFIRMED** | `mutation_report_v47.json` |
| **Auditoría independiente píxel-a-píxel gráficos (`H-009`)** | **`10/10`** benchmarks gráficos exactos (`100.000%`, `MAE=0.00`, `PSNR=99.99 dB`) + `COMP-ULTRA-MATH` + **`3/3` negativos rechazados** | **CONFIRMED** | `graphics_pixel_reference_audit.json` |
| **Unidad física Casio fx-CG50** | `physical_verified = false` (`NOT_YET_VERIFIED`) | **PENDING** | `UNVERIFIED_COMPONENTS.json` |

## 2. Divergencias Reales Encontradas y Corregidas en el Núcleo (`4`) + Defectos Demostrados en `CPU73050.dll` (`2`)

1. **`HW-DIV-001` (`CONFIRMED_FIXED`):** `Usb2Unit` no serializaba los bancos por pipe (`PIPECFG/PIPEBUF/PIPEMAXP/PIPEPERI`) -> corregido con chunk opcional `112` (`409/413 -> 413/413`).
2. **`HW-DIV-002` (`CONFIRMED_FIXED`):** Reescritura integral de `sh_dsp.h` a la ISA Renesas SH4AL-DSP real (`16/16` `MOVS`, `140/140` clases DSP2 activas, `116/116` ilegales -> `EXPEVT 0x180`).
3. **`HW-DIV-003` (`CONFIRMED_FIXED`):** Corrección de opcodes desplazados en `sh_stack.h` (`0x4n56/66/57/67`) y soporte completo de `STC.L/LDC.L MOD/RS/RE`, `STS.L/LDS.L DSR/A0/X0/X1/Y0/Y1`, `LDC/LDC.L SGR`, `SETRC 0x4n34` y bloqueo estricto sin FPU en `0x4n52/0x4n56`.
4. **`HW-DIV-004` (`CONFIRMED_FIXED`):** Corrección de códigos `EXPEVT` TLB invertidos en `sh_exception.h` (`0x040/0x060/0x080/0x0A0/0x0C0`) y vectorización de `TLB Protection` a `VBR+0x100` (`52/56` ranuras verificadas).
5. **`ORACLE-DLL-001` y `ORACLE-DLL-002` (documentados, NO copiados):** Desensamblado x86 de `CPU73050.dll` probó que Casio trunca `MACH` a 10 bits en `LDS Rn, MACH` (`RVA 0x18A83`) y salta al epílogo `0x12227` con `eax` sin inicializar en `PCOPY 0xD9/F9/FA/FB` (`RVA 0x117FC..0x11956`). Nuestro núcleo preserva fielmente la especificación Renesas SH4AL-DSP.

## 3. Cierre de los 11 Hallazgos de la Auditoría Externa (`H-001`..`H-011`)

- **`H-001`:** `build_hardware_inventory.py` reescrito sin heurísticas `.exists()` ni búsquedas de texto; 100 % guiado por artefactos empíricos.
- **`H-002`:** `diff_comparator.py` con 12 etiquetas taxonómicas estrictas, verificación `fpu_opcodes != 0` antes de `VERSION_DIFF`, `10/10` pruebas unitarias PASS y `physical_reference_status = "NOT_YET_VERIFIED"`.
- **`H-003`:** `strictSh7305NoFpu_ = true` por defecto en `Cg50NativeCore`.
- **`H-004`:** `HardwareProfile::StrictSilicon` por defecto (`examDecouplingEnabled_ = false`, `examModePatchHits == 0`).
- **`H-005`:** `MmioAuditTable` protegido con `std::mutex` + `std::once_flag` y verificado bajo 16 hilos concurrentes con `CG50_MMIO_AUDIT=1`.
- **`H-006`:** Verificación criptográfica SHA-256 FIPS 180-4 en `casio_ffi_bindings.dart` sobre las 8 ROMs staged.
- **`H-007`:** Persistencia atómica (`MoveFileExA`/`rename` en `casio_ffi.cpp` y `writeAtomic()` en Dart) sin borrado previo del original.
- **`H-008`:** Endurecimiento opt-in en `cg50_local_bridge_server.dart` (`readOnlyMode`, `loopbackOnly`, `optionalPairingToken`, confirmación de borrado recursivo, CORS restringido a LAN, manteniendo el flujo directo sin PIN por defecto).
- **`H-009`:** `verify_graphics_references.py` verifica píxel-a-píxel contra renderizador matemático independiente RGB565 (`10/10` exactos `MAE=0.00` + `3/3` negativos rechazados).
- **`H-010`:** Deduplicación y filtro `status == "PASS"` en `run_coverage_12_metrics.py`.
- **`H-011`:** `cw2_headless_test`, `nx_regression_test` (ClassWiz + ES PLUS) y `cg50_android_smoke` con verificación numérica estricta por glifos calibrados y comprobaciones negativas en directorios temporales aislados.

## 4. Entregables de Certificación (`12 / 12`)

1. `HARDWARE_CERTIFICATION_MASTER.json`
2. `HARDWARE_CERTIFICATION_REPORT.md`
3. `REGISTER_BY_REGISTER_MATRIX.csv`
4. `BITFIELD_CERTIFICATION_MATRIX.csv`
5. `CPU_OPCODE_DIFFERENTIAL_RESULTS.json`
6. `DSP_DIFFERENTIAL_RESULTS.json`
7. `EXCEPTION_CERTIFICATION_MATRIX.json`
8. `HARDWARE_DIVERGENCES.json`
9. `UNVERIFIED_COMPONENTS.json`
10. `COVERAGE_GAPS.json`
11. `TIMING_DIVERGENCES.json`
12. `KNOWN_LIMITATIONS.md`
