# CG50 v47 — Reporte de Pruebas de Mutación en Sandbox Aislado (112 Mutantes No Equivalentes)

- **Generado (UTC):** `2026-11-24T23:24:02.842331+00:00`
- **Sandbox:** `D:\Proyectos\CalculadorasUnificadas\build-mutation-sandbox` (`live_workspace_modified=false`)
- **Resultado:** **`112 / 112` eliminados (`100.0%`)**, sobrevivientes: `0`, errores de compilación: `0`
- **Tiempo total:** `1593.72 s`

| ID Mutante | Subsistema | Estado | Test que lo mató | Tiempo | Descripción |
|---|---|---|---|---|---|
| `MUT-001-CPU-DT-FLAG` | SH-4A CPU | **KILLED** | `cg50_sh4a_isa_test` | `11.296s` | Inverts DT Rn zero-flag T semantics |
| `MUT-002-CPU-SHLL2` | SH-4A CPU | **KILLED** | `cg50_sh4a_isa_test` | `9.871s` | Shifts left by 1 instead of 2 on SHLL2 |
| `MUT-003-CPU-SHLR2` | SH-4A CPU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `11.923s` | Shifts right by 1 instead of 2 on SHLR2 |
| `MUT-004-CPU-SHLL8` | SH-4A CPU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `12.557s` | Shifts left by 4 instead of 8 on SHLL8 |
| `MUT-005-CPU-SHLR8` | SH-4A CPU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `13.261s` | Shifts right by 4 instead of 8 on SHLR8 |
| `MUT-006-CPU-SHLL16` | SH-4A CPU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `10.886s` | Shifts left by 15 instead of 16 on SHLL16 |
| `MUT-007-CPU-SHLR16` | SH-4A CPU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `9.876s` | Shifts right by 15 instead of 16 on SHLR16 |
| `MUT-008-CPU-CMP-PZ` | SH-4A CPU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `11.236s` | Changes CMP/PZ (>=0) to (>0) |
| `MUT-009-CPU-CMP-PL` | SH-4A CPU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `10.941s` | Changes CMP/PL (>0) to (>=0) |
| `MUT-010-CPU-DELAYSLOT-JMP` | SH-4A CPU | **KILLED** | `cg50_sh4a_isa_test` | `7.703s` | Removes slot-illegal exception (0x1A0) on JMP inside delay slot |
| `MUT-011-CPU-DELAYSLOT-JSR` | SH-4A CPU | **KILLED** | `cg50_sh4a_isa_test` | `4.594s` | Removes slot-illegal exception on JSR inside delay slot |
| `MUT-012-CPU-NOFPU-STRICT` | SH7305 No-FPU Gate | **KILLED** | `cg50_sh7305_hw_test` | `11.378s` | Disables SH7305 no-FPU illegal exception trap on 0xFxxx |
| `MUT-013-CPU-CLIPU-B` | SH-4A CPU | **KILLED** | `cg50_sh7305_hw_test` | `10.714s` | Clamps CLIPU.B to 0xFE instead of 0xFF |
| `MUT-014-CPU-CLIPS-B` | SH-4A CPU | **KILLED** | `cg50_sh4a_isa_test` | `4.327s` | Clamps CLIPS.B negative to -127 instead of -128 |
| `MUT-015-CPU-NOTT` | SH-4A CPU | **KILLED** | `cg50_sh7305_hw_test` | `12.93s` | Disables T-bit inversion in NOTT |
| `MUT-016-CPU-CLRT` | SH-4A CPU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `12.076s` | Sets T=1 instead of T=0 on CLRT |
| `MUT-017-CPU-SETT` | SH-4A CPU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `10.438s` | Sets T=0 instead of T=1 on SETT |
| `MUT-018-CPU-DIV0U` | SH-4A CPU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `11.924s` | Sets instead of clears M/Q/T in DIV0U |
| `MUT-019-ALU-ADD-OVF` | SH-4A Integer ALU | **KILLED** | `cg50_sh4a_isa_test` | `4.789s` | Corrupts signed addition overflow detection (ADDV) |
| `MUT-020-ALU-SUB-OVF` | SH-4A Integer ALU | **KILLED** | `cg50_sh4a_isa_test` | `5.02s` | Corrupts signed subtraction overflow detection (SUBV) |
| `MUT-021-ALU-ADDC-CARRY` | SH-4A Integer ALU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `10.301s` | Drops carry-out bit in ADDC |
| `MUT-022-ALU-SUBC-BORROW` | SH-4A Integer ALU | **KILLED** | `cg50_sh4a_isa_test` | `5.911s` | Off-by-one borrow flag in SUBC |
| `MUT-023-ALU-NEGC-BORROW` | SH-4A Integer ALU | **KILLED** | `cg50_sh4a_isa_test` | `5.356s` | Inverts borrow flag T in NEGC |
| `MUT-024-ALU-SHAD-SAT` | SH-4A Integer ALU | **KILLED** | `cg50_sh4a_isa_test` | `5.778s` | Zeros negative SHAD -32 result instead of 0xFFFFFFFF |
| `MUT-025-ALU-SHLD-RIGHT` | SH-4A Integer ALU | **KILLED** | `cg50_sh4a_isa_test` | `4.121s` | Returns 0xFFFFFFFF instead of 0 on SHLD -32 |
| `MUT-026-ALU-ROTL` | SH-4A Integer ALU | **KILLED** | `cg50_sh4a_isa_test` | `4.442s` | Drops wrap-around bit 0 in ROTL |
| `MUT-027-ALU-ROTR` | SH-4A Integer ALU | **KILLED** | `cg50_sh4a_isa_test` | `4.252s` | Drops wrap-around bit 31 in ROTR |
| `MUT-028-ALU-ROTCL` | SH-4A Integer ALU | **KILLED** | `cg50_sh4a_isa_test` | `4.412s` | Inverts incoming T bit in ROTCL |
| `MUT-029-ALU-ROTCR` | SH-4A Integer ALU | **KILLED** | `cg50_sh4a_isa_test` | `4.09s` | Inverts incoming T bit in ROTCR |
| `MUT-030-ALU-CMPSTR` | SH-4A Integer ALU | **KILLED** | `cg50_sh4a_isa_test` | `3.988s` | Ignores MSB byte match in CMP/STR |
| `MUT-031-ALU-XTRCT` | SH-4A Integer ALU | **KILLED** | `cg50_sh4a_isa_test` | `5.897s` | Corrupts upper 16 bits in XTRCT |
| `MUT-032-DIV0S-TFLAG` | SH-4A Divide | **KILLED** | `cg50_sh4a_isa_test` | `4.675s` | Inverts DIV0S T-bit initialization |
| `MUT-033-DIV1-TFLAG` | SH-4A Divide | **KILLED** | `cg50_sh4a_isa_test` | `5.349s` | Inverts DIV1 quotient bit T |
| `MUT-034-DIV1-SHIFTIN` | SH-4A Divide | **KILLED** | `cg50_sh4a_isa_test` | `7.084s` | Drops oldT shift-in bit in DIV1 |
| `MUT-035-DIV-APPLY-Q` | SH-4A Divide | **KILLED** | `cg50_sh4a_isa_test` | `4.96s` | Never sets SR.Q after DIV0S/DIV1 |
| `MUT-036-MUL-LONG` | SH-4A Multiply | **KILLED** | `cg50_sh4a_isa_test` | `4.952s` | Replaces MUL.L product with addition |
| `MUT-037-MULU-W` | SH-4A Multiply | **KILLED** | `cg50_sh4a_isa_test` | `6.664s` | Truncates lhs to 8 bits in MULU.W |
| `MUT-038-MULS-W` | SH-4A Multiply | **KILLED** | `cg50_sh4a_isa_test` | `9.942s` | Zero-extends instead of sign-extends lhs in MULS.W |
| `MUT-039-DMULU-L` | SH-4A Multiply | **KILLED** | `cg50_sh4a_isa_test` | `18.93s` | Zeros MACH in DMULU.L |
| `MUT-040-DMULS-L` | SH-4A Multiply | **KILLED** | `cg50_cpu_dsp_cert_lab` | `13.291s` | Corrupts MACH shift in DMULS.L |
| `MUT-041-DSP-LDS-SR-GATE` | SH4AL-DSP | **KILLED** | `cg50_sh7305_hw_test` | `10.211s` | Allows LDS Rm, A0 when SR.DSP=0 instead of 0x180 |
| `MUT-042-DSP-SETRC-MASK` | SH4AL-DSP | **KILLED** | `cg50_sh7305_hw_test` | `9.944s` | Truncates SETRC repeat counter to 1 bit |
| `MUT-043-DSP-SAT-POS` | SH4AL-DSP | **KILLED** | `cg50_sh7305_hw_test` | `10.29s` | Disables SR.S 32-bit saturation in FinalizeArith40 |
| `MUT-044-DSP-SIGNEXT40` | SH4AL-DSP | **KILLED** | `cg50_cpu_dsp_cert_lab` | `9.757s` | Truncates 40-bit accumulator guard bits in SignExtend40 |
| `MUT-045-DSP-DSR-GT` | SH4AL-DSP | **KILLED** | `cg50_cpu_dsp_cert_lab` | `10.273s` | Never sets DSR.GT flag on positive DSP result |
| `MUT-046-DSP-DSR-ZERO` | SH4AL-DSP | **KILLED** | `cg50_cpu_dsp_cert_lab` | `12.297s` | Inverts DSR.Z zero flag in FinalizeArith40 |
| `MUT-047-DSP-DSR-NEG` | SH4AL-DSP | **KILLED** | `cg50_cpu_dsp_cert_lab` | `20.552s` | Inverts DSR.N negative flag in FinalizeArith40 |
| `MUT-048-DSP-DSR-OVF` | SH4AL-DSP | **KILLED** | `cg50_cpu_dsp_cert_lab` | `14.876s` | Disables 32-bit overflow detection in DSP ALU |
| `MUT-049-DSP-PMULS-Q15` | SH4AL-DSP | **KILLED** | `cg50_cpu_dsp_cert_lab` | `12.348s` | Drops Q15*Q15 left-shift-by-1 fractional scaling in PMULS |
| `MUT-050-DSP-MODULO-WRAP` | SH4AL-DSP | **KILLED** | `cg50_sh7305_hw_test` | `18.954s` | Disables MOD circular buffer wraparound in MOVX/MOVY |
| `MUT-051-DSP-ILLEGAL-DST` | SH4AL-DSP | **KILLED** | `cg50_cpu_dsp_cert_lab` | `11.173s` | Rejects valid A0 (0x7) destination in DSP ALU |
| `MUT-052-BCD-ADD-CARRY` | SH7305 BCD ALU | **KILLED** | `cg50_sh7305_hw_test` | `13.666s` | Off-by-one decimal carry threshold in BCD ADD |
| `MUT-053-BCD-SUB-BORROW` | SH7305 BCD ALU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `12.679s` | Corrupts decimal borrow base (9 instead of 10) in BCD SUB |
| `MUT-054-BCD-LATCH-STORE` | SH7305 BCD ALU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `12.471s` | Clears multi-word BCD carry/borrow latch |
| `MUT-055-BCD-ALIAS-0C` | SH7305 BCD ALU | **KILLED** | `cg50_cpu_dsp_cert_lab` | `12.222s` | Breaks BCD ALU result mirror alias at 0xA4CB000C |
| `MUT-056-EXPEVT-TLBMISS-RD` | MMU / Exceptions | **KILLED** | `cg50_cpu_dsp_cert_lab` | `11.438s` | Swaps Data TLB Miss Read EXPEVT 0x040 -> 0x060 |
| `MUT-057-EXPEVT-TLBMISS-WR` | MMU / Exceptions | **KILLED** | `cg50_cpu_dsp_cert_lab` | `10.457s` | Swaps Data TLB Miss Write EXPEVT 0x060 -> 0x040 |
| `MUT-058-EXPEVT-INITPGWR` | MMU / Exceptions | **KILLED** | `cg50_cpu_dsp_cert_lab` | `7.596s` | Corrupts Initial Page Write EXPEVT 0x080 -> 0x0C0 |
| `MUT-059-EXPEVT-PROTRD` | MMU / Exceptions | **KILLED** | `cg50_cpu_dsp_cert_lab` | `7.472s` | Corrupts Data TLB Protection Read EXPEVT 0x0A0 |
| `MUT-060-EXPEVT-PROTWR` | MMU / Exceptions | **KILLED** | `cg50_cpu_dsp_cert_lab` | `7.268s` | Corrupts Data TLB Protection Write EXPEVT 0x0C0 |
| `MUT-061-EXPEVT-MULTIHIT` | MMU / Exceptions | **KILLED** | `cg50_cpu_dsp_cert_lab` | `7.167s` | Corrupts TLB Multi-Hit EXPEVT 0x140 |
| `MUT-062-CACHE-CCR-OCI` | SH-4A Cache | **KILLED** | `cg50_sh7305_hw_test` | `15.504s` | Disables CCR.OCI operand cache invalidation |
| `MUT-063-CACHE-CCR-ICI` | SH-4A Cache | **KILLED** | `cg50_sh7305_hw_test` | `7.267s` | Disables CCR.ICI instruction cache invalidation |
| `MUT-064-CACHE-OCRAM-GATE` | SH-4A Cache | **KILLED** | `cg50_sh7305_hw_test` | `7.127s` | Exposes OCRAM even when CCR.ORA=0 |
| `MUT-065-CACHE-OCBP-WB` | SH-4A Cache | **KILLED** | `cg50_sh7305_hw_test` | `7.954s` | Drops dirty-line writeback counter on OCBP |
| `MUT-066-CACHE-OCBWB-CLEAN` | SH-4A Cache | **KILLED** | `cg50_sh7305_hw_test` | `8.221s` | Fails to flush dirty line in OCBWB |
| `MUT-067-RAMCR-MASK` | Cache/OCRAM RAMCR | **KILLED** | `cg50_sh7305_hw_test` | `10.663s` | Removes exact CPU73050.dll 0x3E0 mask on RAMCR |
| `MUT-068-UBC-FETCH-CH0` | UBC Breakpoints | **KILLED** | `cg50_sh7305_hw_test` | `9.355s` | Disables UBC ch0 instruction-fetch breakpoint detection |
| `MUT-069-UBC-CCMFR-W0C` | UBC Breakpoints | **KILLED** | `cg50_sh7305_hw_test` | `7.955s` | Breaks W0C semantics on UBC CCMFR |
| `MUT-070-UBC-CETR1-MASK` | UBC Breakpoints | **KILLED** | `cg50_sh7305_hw_test` | `10.459s` | Removes 12-bit mask on UBC CETR1 |
| `MUT-071-UBC-CBCR-MASK` | UBC Breakpoints | **KILLED** | `cg50_sh7305_hw_test` | `9.552s` | Removes bit-0 mask on UBC CBCR |
| `MUT-072-UBC-DATA-CH1` | UBC Breakpoints | **KILLED** | `cg50_sh7305_hw_test` | `8.702s` | Ignores CDR1/CDMR1 data comparison on UBC ch1 watchpoint |
| `MUT-073-INTC2-USERIMASK-KEY` | INTC2 | **KILLED** | `cg50_sh7305_hw_test` | `16.163s` | Bypasses 0xA5000000 write-key protection on USERIMASK |
| `MUT-074-INTC2-INTREQ-W0C` | INTC2 | **KILLED** | `cg50_sh7305_hw_test` | `8.585s` | Breaks W0C semantics on INTC2 INTREQ00 |
| `MUT-075-INTC2-INTMSKCLR` | INTC2 | **KILLED** | `cg50_sh7305_hw_test` | `11.787s` | Sets instead of clears mask bits on INTC2 INTMSKCLR00 |
| `MUT-076-INTC2-PRIO-SHIFT` | INTC2 | **KILLED** | `cg50_sh7305_hw_test` | `11.389s` | Reverses IRQ0..7 nibble order in INTPRI00 |
| `MUT-077-ETMU-UNF-FLAG` | ETMU | **KILLED** | `cg50_sh7305_hw_test` | `11.895s` | Never asserts ETMU underflow flag UNF |
| `MUT-078-ETMU-DECREMENT` | ETMU | **KILLED** | `cg50_sh7305_hw_test` | `7.51s` | Counts ETMU up instead of down |
| `MUT-079-ADC-ADST-ADF` | ADC | **KILLED** | `cg50_sh7305_hw_test` | `10.964s` | Fails to set ADF conversion-complete flag on ADST |
| `MUT-080-ADC-BAT-SCALE` | ADC | **KILLED** | `cg50_combined_hw_stress` | `10.714s` | Zeros battery ADC channel result |
| `MUT-081-DD-CLKR0-WRITE` | DD Bus | **KILLED** | `cg50_sh7305_hw_test` | `10.699s` | Drops writes to DD Bus DDCLKR0 |
| `MUT-082-SDHI-CARD-DETECT` | SDHI | **KILLED** | `cg50_hw_silicon_100_test` | `11.781s` | Forces SD_INFO1 card-detect bit ON with no card |
| `MUT-083-SDHI-CMD8-RSP` | SDHI | **KILLED** | `cg50_hw_silicon_100_test` | `12.716s` | Breaks SDHI CMD8 voltage-check response 0x01AA |
| `MUT-084-SDHI-NOCARD-TO` | SDHI | **KILLED** | `cg50_hw_silicon_100_test` | `12.833s` | Fails to set RSP_TO timeout when SD card is absent |
| `MUT-085-HUDI-SDINT-W0C` | HUDI | **KILLED** | `cg50_hw_silicon_100_test` | `9.949s` | Breaks W0C semantics on HUDI SDINT |
| `MUT-086-PRLCK-LOCK-ENFORCE` | PRLCK | **KILLED** | `cg50_hw_silicon_100_test` | `13.948s` | Disables PRLCKCR lock enforcement on PRPRICR0..5 |
| `MUT-087-PRLCK-MASK3F` | PRLCK | **KILLED** | `cg50_hw_silicon_100_test` | `11.354s` | Removes 0x3F mask on PRPRICR0..5 |
| `MUT-088-SCIF0-LOOPBACK` | SCIF0 | **KILLED** | `cg50_sh7305_hw_test` | `11.56s` | Disables SCIF0 SCFCR.LOOP hardware loopback |
| `MUT-089-SCIF0-OVERRUN` | SCIF0 | **KILLED** | `cg50_sh7305_hw_test` | `7.844s` | Fails to latch ORER overrun flag on 17th RX byte |
| `MUT-090-SCIF0-RDF-FLAG` | SCIF0 | **KILLED** | `cg50_sh7305_hw_test` | `10.18s` | Never sets SCFSR RDF|DR when RX FIFO has data |
| `MUT-091-USB2-VBUS-ATTACH` | USB2 | **KILLED** | `cg50_sh7305_hw_test` | `9.676s` | Fails to set VBSTS/VBINT/DVST on USB VBUS attach |
| `MUT-092-USB2-SETUP-VALID` | USB2 | **KILLED** | `cg50_sh7305_hw_test` | `5.797s` | Drops VALID/CTRT interrupt bits on USB SETUP packet |
| `MUT-093-USB2-PIPE-BANK` | USB2 | **KILLED** | `cg50_combined_hw_stress` | `10.013s` | Ignores PIPESEL bank index on PIPECFG read |
| `MUT-094-BSC-RTCSR-KEY` | BSC | **KILLED** | `cg50_hw_silicon_100_test` | `9.818s` | Removes 0xA55A0000 key guard on BSC RTCSR writes |
| `MUT-095-BSC-CSWCR-MASK` | BSC | **KILLED** | `cg50_hw_silicon_100_test` | `10.49s` | Removes CPU73050 type-dependent CS*WCR mask |
| `MUT-096-BSC-REFRESH-CMF` | BSC | **KILLED** | `cg50_hw_silicon_100_test` | `9.749s` | Prevents BSC SDRAM refresh compare-match flag CMF |
| `MUT-097-CPG-MSTPCR0-TMU-GATE` | CPG | **KILLED** | `cg50_hw_silicon_100_test` | `9.962s` | Disables CPG MSTPCR0 clock gating of TMU0/1 |
| `MUT-098-DMAC0-ADDR-ERR` | DMAC0 | **KILLED** | `cg50_hw_silicon_100_test` | `9.877s` | Prevents DMAOR.AE assertion on misaligned DMA |
| `MUT-099-DMAC0-CHCR-MASK` | DMAC0 | **KILLED** | `cg50_hw_silicon_100_test` | `10.998s` | Removes CPU73050 CHCR0..5 writable bitmask |
| `MUT-100-RTC-ALARM-FLAG` | RTC | **KILLED** | `cg50_hw_silicon_100_test` | `9.288s` | Prevents RTC Alarm Flag (RCR1.AF) from latching |
| `MUT-101-RTC-PERIODIC-FLAG` | RTC | **KILLED** | `cg50_hw_silicon_100_test` | `9.481s` | Prevents RTC Periodic Event Flag (RCR2.PEF) |
| `MUT-102-LCD-GRAM-WRITE` | R61524 LCD | **KILLED** | `cg50_sh7305_hw_test` | `12.412s` | Corrupts RGB565 pixel values written to R61524 GRAM |
| `MUT-103-LCD-GRAM-DUMMYRD` | R61524 LCD | **KILLED** | `cg50_sh7305_hw_test` | `9.303s` | Skips mandatory R61524 first dummy GRAM read |
| `MUT-104-LCD-SCROLL-Y` | R61524 LCD | **KILLED** | `cg50_sh7305_hw_test` | `4.229s` | Disables R61524 hardware vertical scroll rendering |
| `MUT-105-FLASH-CFI-UNLOCK` | NOR Flash CFI | **KILLED** | `cg50_flash_cfi_test` | `7.123s` | Breaks Spansion S29GL256P CFI unlock sequence |
| `MUT-106-FLASH-CFI-QUERY` | NOR Flash CFI | **KILLED** | `cg50_flash_cfi_test` | `7.202s` | Disables CFI Query (0x98@0xAA) QRY identification |
| `MUT-107-FLASH-AUTOSELECT-ID` | NOR Flash CFI | **KILLED** | `cg50_flash_cfi_test` | `7.495s` | Corrupts S29GL256P Autoselect Device ID1 0x227E |
| `MUT-108-USB-PROTO7-CHK` | USB Protocol 7.00 | **KILLED** | `cg50_usb_smem_qr_test` | `8.859s` | Corrupts Casio Protocol 7.00 packet checksum |
| `MUT-109-USB-PROTO7-CHK-VERIFY` | USB Protocol 7.00 | **KILLED** | `cg50_usb_smem_qr_test` | `9.717s` | Accepts corrupted Protocol 7.00 packet checksums |
| `MUT-110-FUGUE-CRC16` | SMEM Fugue FTL | **KILLED** | `cg50_usb_smem_qr_test` | `5.863s` | Uses wrong polynomial in Fugue FTL CRC16 |
| `MUT-111-QR-GF256-POLY` | LCD QR Detector | **KILLED** | `cg50_usb_smem_qr_test (TIMEOUT)` | `34.61s` | Uses AES polynomial 0x11B instead of QR GF(256) 0x11D |
| `MUT-112-QR-BCH-FORMAT` | LCD QR Detector | **KILLED** | `cg50_usb_smem_qr_test (TIMEOUT)` | `34.906s` | Corrupts ISO-18004 QR BCH(15,5) format mask 0x5412 |
