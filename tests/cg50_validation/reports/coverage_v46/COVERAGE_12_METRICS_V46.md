# CG50 v46 — Reporte de 12 Métricas Independientes de Cobertura

- **Generado (UTC):** `2026-11-24T15:54:24.996878+00:00`
- **Compilador e instrumentación:** `GCC 16.1.0 (MSYS2 UCRT64 g++ --coverage -O1 -DCG50_ENABLE_COVERAGE_COUNTERS=1)`

| # | Métrica Independiente | Numerador / Denominador Medido | Porcentaje | Notas |
|---|---|---|---|---|
| 1 | **Feature** | `682 / 922` ROM-confirmadas; `134` batería + `24` receipts | `73.97%` ROM / `17.14%` auto | Separación estricta estático vs dinámico |
| 2 | **Opcode** | `152 / 227` clases 16-bit (`16580 / 59919` encodings); DSP32 `0 / 140` | `66.96%` clases | `4,010,727,966` instrucciones dinámicas medidas |
| 3 | **MMIO** | `266 / 413` registros CG50 (`121` R+W) | `64.41%` | `321` direcciones MMIO únicas |
| 4 | **Bitfield** | `850 / 7266` bits alternados (`0<->1`) | `11.7%` | `132` registros con toggle real |
| 5 | **Branch (`gcov -b`)** | `6089 / 8658` ejecutadas (`4058` tomadas ≥1 vez); líneas `7338/11701` (`62.71%`) | `70.33%` exec / `46.87%` taken | `34` archivos `core/casio_cg50/` |
| 6 | **Function (`gcov -f`)** | `422 / 606` funciones `casio_cg50` | `69.64%` | Calls `1525/2634` (`57.9%`) |
| 7 | **State** | `20 / 25` estados (CPU `2/4`, Flash CFI `12/15`, DMAC `6/6`) | `80.0%` | FSMs hardware reales |
| 8 | **Transition** | `21 / 28` transiciones | `75.0%` | CPU `4` + Flash CFI `17` |
| 9 | **Error** | `4 / 56` códigos `EXPEVT` + `9` `INTEVT` | N/A | `DMAC AE=1`, `DelaySlotExc=0` |
| 10 | **Boundary** | `8 / 9` sondas de frontera | `88.89%` | MMU AT, Delay Slot, DSP Repeat, BSC Refresh, RTC Alarm/Periodic, DMAC AE, BCD ALU, Sleep Wake |
| 11 | **Combination** | `10 / 10` suites multi-subsistema | `100.0%` | Se amplía en Fase 5 con matriz Setup + 100K |
| 12 | **Mutation** | `29 / 30` mutantes eliminados (`PARTIAL`) | `96.67%` | Ejecutado en sandbox aislado `build-mutation-sandbox/` |
