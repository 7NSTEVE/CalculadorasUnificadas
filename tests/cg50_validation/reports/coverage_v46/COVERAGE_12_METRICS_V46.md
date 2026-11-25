# CG50 v46 — Reporte de 12 Métricas Independientes de Cobertura

- **Generado (UTC):** `2026-11-24T23:08:08.329060+00:00`
- **Compilador e instrumentación:** `GCC 16.1.0 (MSYS2 UCRT64 g++ --coverage -O1 -DCG50_ENABLE_COVERAGE_COUNTERS=1)`

| # | Métrica Independiente | Numerador / Denominador Medido | Porcentaje | Notas |
|---|---|---|---|---|
| 1 | **Feature** | `682 / 922` ROM-confirmadas; unión deduplicada PASS-only `23` (`23` bat ∪ `0` val, overlap `0`) | `73.97%` ROM / `2.49%` auto | H-010: unión canónica sin doble conteo y filtro `status==PASS` |
| 2 | **Opcode** | `227 / 227` clases 16-bit (`59919 / 59919` encodings); DSP32 `140 / 140` | `100.0%` clases | `4,305,124,677` instrucciones dinámicas medidas |
| 3 | **MMIO** | `413 / 413` registros CG50 (`413` R+W) | `100.0%` | `595` direcciones MMIO únicas |
| 4 | **Bitfield** | `7262 / 7266` bits alternados (`0<->1`) | `99.94%` | `413` registros con toggle real |
| 5 | **Branch (`gcov -b`)** | `6997 / 9273` ejecutadas (`4973` tomadas ≥1 vez); líneas `8350/12237` (`68.24%`) | `75.46%` exec / `53.63%` taken | `34` archivos `core/casio_cg50/` |
| 6 | **Function (`gcov -f`)** | `443 / 619` funciones `casio_cg50` | `71.57%` | Calls `1674/2718` (`61.59%`) |
| 7 | **State** | `20 / 25` estados (CPU `2/4`, Flash CFI `12/15`, DMAC `6/6`) | `80.0%` | FSMs hardware reales |
| 8 | **Transition** | `22 / 28` transiciones | `78.57%` | CPU `4` + Flash CFI `18` |
| 9 | **Error** | `13 / 56` códigos `EXPEVT` + `38` `INTEVT` | N/A | `DMAC AE=65`, `DelaySlotExc=3` |
| 10 | **Boundary** | `9 / 9` sondas de frontera | `100.0%` | MMU AT, Delay Slot, DSP Repeat, BSC Refresh, RTC Alarm/Periodic, DMAC AE, BCD ALU, Sleep Wake |
| 11 | **Combination** | `14 / 14` suites multi-subsistema | `100.0%` | Se amplía en Fase 5 con matriz Setup + 100K |
| 12 | **Mutation** | `112 / 112` mutantes eliminados (`CONFIRMED`) | `100.0%` | Ejecutado en sandbox aislado `build-mutation-sandbox/` |
