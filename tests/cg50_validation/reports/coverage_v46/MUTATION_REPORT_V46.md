# CG50 v46 — Reporte de Pruebas de Mutación en Sandbox Aislado

- **Generado (UTC):** `2026-11-24T16:12:39.665090+00:00`
- **Sandbox:** `D:\Proyectos\CalculadorasUnificadas\build-mutation-sandbox` (`live_workspace_modified=false`)
- **Resultado:** **`29 / 30` eliminados (`96.67%`)**, sobrevivientes: `1`, errores de compilación: `0`
- **Tiempo total:** `358.49 s`

| ID Mutante | Subsistema | Estado | Test que lo mató | Tiempo | Descripción |
|---|---|---|---|---|---|
| `MUT-01-CPU-DT-FLAG` | SH-4A CPU | **KILLED** | `cg50_sh4a_isa_test` | `10.86s` | Inverts DT Rn zero-flag T semantics |
| `MUT-02-CPU-SHLL2` | SH-4A CPU | **KILLED** | `cg50_sh4a_isa_test` | `9.471s` | Shifts left by 1 instead of 2 on SHLL2 |
| `MUT-03-CPU-DELAYSLOT-JMP` | SH-4A CPU | **KILLED** | `cg50_sh4a_isa_test` | `8.87s` | Removes slot-illegal exception (0x1A0) on JMP inside delay slot |
| `MUT-04-CPU-NOFPU-STRICT` | SH7305 No-FPU Gate | **KILLED** | `cg50_sh7305_hw_test` | `9.028s` | Disables SH7305 no-FPU illegal exception trap on 0xFxxx |
| `MUT-05-DSP-LDS-SR-GATE` | SH4AL-DSP | **KILLED** | `cg50_sh7305_hw_test` | `8.636s` | Allows LDS Rm, A0 when SR.DSP=0 instead of raising 0x180 |
| `MUT-06-DSP-SATURATE32` | SH4AL-DSP | **KILLED** | `cg50_sh7305_hw_test` | `10.016s` | Disables positive 32-bit DSP saturation |
| `MUT-07-DSP-SETRC-MASK` | SH4AL-DSP | **KILLED** | `cg50_sh7305_hw_test` | `10.942s` | Truncates SETRC repeat counter to 1 bit |
| `MUT-08-BCD-ADD-CARRY` | SH7305 BCD ALU | **KILLED** | `cg50_sh7305_hw_test` | `12.418s` | Off-by-one decimal carry threshold in hardware BCD ALU |
| `MUT-09-CACHE-CCR-OCI` | SH-4A Cache | **KILLED** | `cg50_sh7305_hw_test` | `9.228s` | Disables CCR.OCI operand cache invalidation |
| `MUT-10-RAMCR-MASK` | Cache/OCRAM RAMCR | **KILLED** | `cg50_sh7305_hw_test` | `9.241s` | Removes exact CPU73050.dll 0x3E0 mask on RAMCR |
| `MUT-11-UBC-FETCH-MATCH` | UBC Breakpoints | **KILLED** | `cg50_sh7305_hw_test` | `17.76s` | Disables UBC Channel 0 instruction-fetch breakpoint detection |
| `MUT-12-INTC2-USERIMASK-KEY` | INTC2 | **KILLED** | `cg50_sh7305_hw_test` | `12.576s` | Bypasses 0xA5000000 write-key protection on USERIMASK |
| `MUT-13-INTC2-MASK-FILTER` | INTC2 | **SURVIVED** | `-` | `11.96s` | Ignores INTMSK00 masking on external IRQ delivery |
| `MUT-14-HUDI-SDINT-W0C` | HUDI | **KILLED** | `cg50_hw_silicon_100_test` | `9.13s` | Breaks W0C semantics on HUDI SDINT |
| `MUT-15-PRLCK-LOCK-ENFORCE` | PRLCK | **KILLED** | `cg50_hw_silicon_100_test` | `9.738s` | Disables PRLCKCR lock enforcement on PRPRICR0..5 |
| `MUT-16-BSC-RTCSR-KEY` | BSC | **KILLED** | `cg50_hw_silicon_100_test` | `12.962s` | Removes 0xA55A0000 key guard on BSC RTCSR writes |
| `MUT-17-BSC-CSWCR-MASK` | BSC | **KILLED** | `cg50_hw_silicon_100_test` | `12.969s` | Removes CPU73050 type-dependent CS*WCR mask |
| `MUT-18-BSC-REFRESH-CMF` | BSC | **KILLED** | `cg50_hw_silicon_100_test` | `13.736s` | Prevents BSC SDRAM refresh compare-match flag CMF from asserting |
| `MUT-19-CPG-MSTPCR0-TMU-GATE` | CPG | **KILLED** | `cg50_hw_silicon_100_test` | `13.288s` | Disables CPG MSTPCR0 clock gating of TMU0/1 |
| `MUT-20-DMAC0-ADDR-ERR` | DMAC0 | **KILLED** | `cg50_hw_silicon_100_test` | `13.594s` | Prevents DMAOR.AE assertion on misaligned DMA transfer |
| `MUT-21-DMAC0-CHCR-MASK` | DMAC0 | **KILLED** | `cg50_hw_silicon_100_test` | `10.599s` | Removes CPU73050 CHCR0..5 writable bitmask enforcement |
| `MUT-22-RTC-ALARM-FLAG` | RTC | **KILLED** | `cg50_hw_silicon_100_test` | `11.962s` | Prevents RTC Alarm Flag (RCR1.AF) from latching on alarm match |
| `MUT-23-RTC-PERIODIC-FLAG` | RTC | **KILLED** | `cg50_hw_silicon_100_test` | `12.759s` | Prevents RTC Periodic Event Flag (RCR2.PEF) from asserting |
| `MUT-24-SDHI-CARD-DETECT` | SDHI | **KILLED** | `cg50_hw_silicon_100_test` | `9.66s` | Forces SD_INFO1 card-detect bit ON even with no card inserted |
| `MUT-25-LCD-GRAM-WRITE` | R61524 LCD | **KILLED** | `cg50_sh7305_hw_test` | `15.905s` | Corrupts RGB565 pixel values written to R61524 GRAM 0x0202 |
| `MUT-26-FLASH-CFI-UNLOCK` | NOR Flash CFI | **KILLED** | `cg50_flash_cfi_test` | `20.121s` | Breaks Spansion S29GL256P CFI unlock command sequence |
| `MUT-27-FLASH-CFI-QUERY` | NOR Flash CFI | **KILLED** | `cg50_flash_cfi_test` | `12.651s` | Disables CFI Query (0x98@0xAA) QRY identification |
| `MUT-28-USB-PROTO7-CHK` | USB Protocol 7.00 | **KILLED** | `cg50_usb_smem_qr_test` | `14.649s` | Corrupts Casio Protocol 7.00 two's-complement packet checksum |
| `MUT-29-FUGUE-CRC16` | SMEM Fugue FTL | **KILLED** | `cg50_usb_smem_qr_test` | `13.911s` | Uses wrong polynomial (0x8005 instead of CCITT 0x1021) in Fugue FTL CRC16 |
| `MUT-30-QR-BCH-FORMAT` | LCD QR Detector | **KILLED** | `cg50_usb_smem_qr_test` | `9.812s` | Corrupts ISO-18004 QR BCH(15,5) format mask 0x5412 |
