# Limitaciones Conocidas y Fronteras de Fidelidad — Casio fx-CG50 Native Core (`v47`)

- **Generado (UTC):** `2026-11-24T23:24:06.656777+00:00`
- **Commit base:** `acb7468b87675fec226fce73eef5ef8f28e1c5eb`
- **Referencia física:** **`NOT_YET_VERIFIED`** (`physical_verified = false` — no se ha ejecutado sonda física sobre una unidad Casio fx-CG50 real en esta sesión).

## 1. Frontera de Hardware Físico vs. Oráculos de Software
- Toda la certificación de bajo nivel (`65,536` codificaciones, `227` clases CPU, `256` clases DSP2, `702` registros SH7305, `8,016` bits CG50, `56` ranuras EXPEVT/INTEVT, `10,000,000` vectores de operandos) fue ejecutada contra:
  1. Las bibliotecas oficiales locales de Casio (`CPU73050.dll`, `LCDR61524A.dll`, `ExtLY755A.dll`, `CGM.dlm`) invocadas en vivo (solo lectura, cero parches `.text`).
  2. La especificación arquitectónica oficial Renesas SH-4A / SH4AL-DSP (`REJ09B0003-0400`).
  3. Oráculos independientes externos (`mpmath` 50 dígitos, `SymPy`, renderizador puro Python RGB565 y `fx-CG Manager PLUS` vivo).
- **No se afirma paridad eléctrica con silicio físico** hasta correr sondas `.g3a` sobre una fx-CG50 física real.

## 2. Temporización Funcional-Precisa (No Ciclo-a-Ciclo Eléctrico)
- El núcleo es **funcional-preciso y 100 % determinista por instrucción**, no un simulador RTL/eléctrico ciclo-a-ciclo.
- `BSC` (`CS*WCR`), `CPG` (`FRQCR` PLL relock), `DMAC0` (transferencias atómicas por canal), `ShCacheUnit` (tags/dirty/OCRAM funcionales sin stalls SDRAM) y `SpansionNorFlashCfi` (ventanas `DQ6/DQ7` configurables) están documentados en detalle en `TIMING_DIVERGENCES.json`.

## 3. Política de Opcodes No Asignados y Perfiles de Hardware (`H-003` / `H-004`)
- **Sin FPU hardware por defecto (`H-003`):** `strictSh7305NoFpu_ = true` por defecto; cualquier opcode `0xFxxx` fuera del espacio SH4AL-DSP o `0x4n5A/4n52/4n56` genera `EXPEVT 0x180`.
- **Perfil `StrictSilicon` por defecto (`H-004`):** el desacoplamiento de Exam Mode de la ROM `Manager PLUS` (`0x00382E84`) está **desactivado por defecto** (`examDecouplingEnabled_ = false`, `examModePatchHits == 0`). Solo se habilita explícitamente vía `HardwareProfile::ManagerPlusCompat` o `CG50_EXAM_DECOUPLING=1`.
- **Opcodes no asignados (`0x3001` y ranuras sin decodificador fuera del espacio `0xFxxx`/DSP):** transicionan honestamente a `CG50CORE_MACHINE_FAULTED` (`HaltUnsupported`) para preservar el contrato C ABI probado en `tests/core/cg50_core_c_api_contract_test.cpp` (sección 12). Todo opcode `0xFxxx` (incluyendo `0xFFFF`) dispara `EXPEVT 0x180` bajo `strictSh7305NoFpu_ = true`.
- **Traps IPC `0xF0010000..0xF00100FF` y `kScanConsistencyEnabled` (`0x8038B1D0`):** son la frontera de interfaz host propia de la imagen ROM `fx-CG Manager PLUS` (`GCalAppIFA.dll`); ejecutan cero cálculo matemático.

## 4. Bloques SoC SH7305 Ausentes en Placa fx-CG50 (11 módulos / 289 registros)
- `I2C`, `SHOC`, `SIOF0/1`, `MMCIF`, `FLCTL`, `BLITTER`, `ECC`, `SPU2-DSP0/1` y `FSI` no están cableados en la placa fx-CG50 (sin altavoz ni NAND).
- La auditoría forense de `SystemROM.raw` demostró que el OS solo referencia `13` de esos `289` registros durante arranque/standby para mantenerlos en reset; los `289/289` cuentan con respaldo R/W aislado verificado.

## 5. Periféricos Externos y Soak Multi-Día
- Sensores físicos externos (`EA-200` / `C-Lab`), cable físico 3-pin y proyector físico están fuera de alcance de software (sí funcionan `USB Mass Storage`, `Protocol 7.00`, `ScreenRecv` y `SCIF0` link virtual por red local).
- Soak continuo `24h–72h` permanece `PENDING` (se ejecutaron soak de 15 min con `4.87B` ops y estrés adversarial de `120,000` secuencias / 16 hilos con `0` fallos).

## 6. Límites Matemáticos Nativos del Firmware Casio OS 3.80
- Sin CAS simbólico general; rango `±9.999999999e99`; precisión interna BCD de 15 dígitos (`10` visibles); `Equation` hasta grado 6 / 6 incógnitas; MicroPython ejecuta punto flotante IEEE-754 por software (sin FPU hardware SH7305).
