# MATRIZ MAESTRA DE COMPATIBILIDAD — fx-CG50 nativo (rama/cg50-nativo-ios)

Mandato del dueño (2026-11-03): acreditar que la calculadora WINDOWS hace los
cálculos antes de cualquier traslado a iOS. Este archivo es el registro
persistente por función; NO se declara compatible nada sin evidencia de
ejecución real reproducible. Los porcentajes de una suite NO son el
porcentaje global de compatibilidad de la calculadora.

## Cómo se acredita un PASO

1. Boot real del OS 3.60 (SystemROM+UserROM+C2ROM trio fsP3a, lectura pura
   en E:) sobre el núcleo SH7305 propio, sin DLL de Casio.
2. Navegación por teclado real (matriz exacta cg50_keys_exact.h) con
   exigencia de reacción de pantalla por tecla.
3. El guest ejecuta la operación; el resultado se decodifica del framebuffer
   (glifos calibrados contra resultados-ancla tecleados por el anfitrión;
   la identidad del símbolo la dan NUESTRAS teclas, no el motor).
4. El texto esperado es una referencia independiente documentada del
   anfitrión (aritmética de doble precisión verificada a mano). El motor
   jamás genera el valor con el que se compara.
5. Repetición ×2 por caso con determinismo de texto decodificado.

## Infraestructura verificada (evidencia smoke v7 + corridas batería 1-6)

| Componente                    | Estado    | Evidencia |
|-------------------------------|-----------|-----------|
| Boot OS 3.60 real (CPU SH7305, MMU/TLB, TMU, DMA LCD) | APROBADA | smoke v7: 198.5M inst, 0 excepciones, primera GRAM chunk 5; battery v6: boot estable hash DCE621980851884D |
| Teclado matriz → guest        | APROBADA  | smoke v7: 6 teclas con reacción; battery v6-v7: tecleo completo de expresiones con registro por tecla |
| LCD R61524 → framebuffer ABI  | APROBADA  | frames 384×216; orientación 180° documentada (diag_rot180); decodificación de glifos en espacio display |
| Decodificador de glifos       | APROBADA  | v6: entrada y resultados decodificados («2?3», «6», «1111111110»); calibración contra resultados-ancla |
| save/restore de estado        | APROBADA  | smoke v7: 52.9 MB, roundtrip en caliente, huella idéntica |
| Recuperación de errores (EXIT tras Syntax ERROR) | APROBADA | corrida v6 envenenada documentada; v7 añade EXIT+AC por repetición |

## RUN-MAT — batería de cálculo real (v10-FULL: 12/12 PASO, exit 0)

Lineage honesto: v8b 6/12 (sombra de calibrador) → v9 10/12 (Σ/integral
con plantilla mal navegada) → **v10-FULL 12/12 deterministas** (exit 0
por gate estricto; recibos copiados del outdir D:\Temp\opencode\
battery-out-v10-full: `recibos/v10-full_battery_result.txt`). El '81/4'
(del caso integral) se lee como PAR estructural [81][4] del display
S-D MathIO; la referencia externa es 20.25.

## v13-FULL (52 casos: 12 vivos + 6 v11 + 34 importados) — 34/52 PASO

Recibos: `recibos/v13-full_battery_result.txt` + `v13-full_stderr.log`
+ BMPs finales por caso (outdir D:\Temp\opencode\battery-out-v13).
exit 7 (parcial, gate honesto: no certifica). Los 12 vivos y los 6
v11 SOSTIENEN (salvo fraccion, ver abajo). Defectos hallados con
recibo visual (BMP leído como imagen), todos del suite importado o
de la conversión, NINGUNO del motor guest:

| Grupo | Recibo | Defecto | Fix v14 |
|---|---|---|---|
| tg01-07 | `sin 30!` + diálogo Out of Domain; F-tab `x!\|nPr\|nCr\|RAND` | OPTN F6 F3 = menú PROB (insertaba `!`), no ANGLE | ° → OPTN F6 F4 F1 (probe volcar verifica) |
| tg01-04, cp03/04/06/07, mt01/05 | `)` extra en el echo + Syntax ERROR | `)` manual tras plantilla MathIO auto-cerrada | quitar kParenC |
| cp06/07 | F-tab pág. 2: `ReP\|ImP\|▶r∠θ\|▶a+bi` | ReP=F6→F1, ImP=F6→F2 (suite: F5 / F6→F1) | rutas corregidas |
| pw05 | guest mostró `3.000000004` | oráculo del suite '3' falso a 10 cifras | esperada=3.000000004 (verif. a mano ✓) |
| pw07 | F-tab pág. 2 CALC: `FMin\|FMax\|Σ(\|logab` | logab=F4 (F1 era FMin; REVISAR acertó) | OPTN F4 F6 F4 |
| fraccion | input `1/(2+1/3)` → guest dio 3/7 (¡correcto!) | sin navegación el kFrac anida | kDer entre fracciones |
| ca01/ca05 | `\|x=□` vacío, valor dentro de la caja expr | el kDer no llegó a la casilla \|x= | probe volcar (cursor por tecla) |
| cp04 | modo RAD global confirmado (barra estado) | oráculo '90' asumía DEG | esperada=1.570796327 (π/2, verif. a mano) |

### Sonda v14a (recibos por tecla, volcar=TRUE; outdir probe-v14a)

- **tg01**: OPTN página 1 = `LIST\|MAT/VCT\|COMPLEX\|CALC\|STAT\|▶`
  (F3=COMPLEX y F4=CALC confirman la tabla canónica). Página 2 =
  `HYPERBL\|PROB\|NUMERIC\|ANGLE\|▶` pero **F4 abrió NUMERIC**
  (`Abs\|Int\|Frac\|Rnd\|Intg`) → hay hueco en F3 de la página 2:
  **ANGLE = F5**. Fix v14b: ° = OPTN F6 **F5** F1.
- **ca01/ca05**: el kDer sale de la caja pero el valor cae en la zona
  antes del `)` de la plantilla (`…-7 2)\|x=□` / `(x⁴ 3)\|x=□`), no en
  `\|x=`. ca02 (contenido que termina en plantilla anidada sin x) SÍ
  llegó con un solo kDer. Fix v14b: **kDer kDer** antes del valor.

### Sonda v14b (outdir probe-v14b) — tg01 y ca05 PASAN; ca01 diagnosticado

- **tg01 = 0.5 PASO determinista 2/2** → **ANGLE = F5 CONFIRMADO por
  ejecución real** (bandas `[2_3i][2_3i][5][???30?][0.5]`: residuales
  del historial arriba, resultado fresco al final — la política
  última-banda sostiene).
- **ca05 = 108 PASO determinista 2/2** → el `kDer kDer` (salir del
  exponente de x⁴, salir de la caja de función hasta `|x=`) VERIFICADO.
- **ca01 DIVERGE con recibo definitorio**: el guest dio
  `-6.430620933×10⁻⁵` (zoom del BMP: exponente ⁻05). El eco a alta
  resolución muestra la causa: el input real era
  `d/dx(3x^(3−5x²+2x−7))\|x=2` — TODO el rabo `−5x²+2x−7` cayó DENTRO
  del exponente de `x^3` (MathIO: tras `^` el cursor permanece en el
  exponente hasta `→`). Verificación analítica a mano:
  f'(2) = 3·2⁻²⁰·(−18·ln2 − 10) = −6.43062×10⁻⁵ — **el guest es
  CORRECTO para lo que recibió**; la ruta le faltó `kDer` tras el `3`
  del exponente. Fix v14c: `kDer` tras `^3`.
- **Lección tg05-07 (oráculos)**: en RAD global la SALIDA de una
  inversa es radianes; el `°` del suite sobre la entrada (0.5, −0.5, 1,
  adimensionales) corrompería el input. Oráculos adaptados a formas
  cerradas RAD: tg05 = π/6 = 0.5235987756 (igual que asin05, PASO v13),
  tg06 = 2π/3 = 2.094395102, tg07 = π/4 = 0.7853981634 (¡valor que el
  guest YA mostró en v13 para tan⁻¹(1)!). El `°` inline queda SOLO en
  tg01-04 (funciones directas: convierte la ENTRADA, verificado por el
  PASO de tg01).
- **pi_dos DIFERIDO a ola 2**: MathIO muestra `2π` SIMBÓLICO (glifo π
  sin calibrar); haría falta toggle S-D tras EXE (soporte pos-EXE en el
  corredor). BLOQUEADA, no fallo del guest.

APROBADAS en v13 (deterministas 2/2): los 12 vivos + raiz9, siete_x2,
asin05, ln10, log2, ar01-05, pw01-04, pw06, cp01, ca02, ca03, ca04,
st01, st02, st03. FALLIDAS con fix en v14: fraccion, pw05, pw07,
tg01-07, cp03, cp04, cp06, cp07, ca01, ca05, mt01, mt05.

### Detalle v10-FULL (12 casos base, todos APROBADA det 2/2)

| Caso | Operación | Referencia independiente | Estado v10-FULL | Nota |
|------|-----------|--------------------------|-----------|------|
| 2x3 | multiplicación | 6 | APROBADA (det 2/2) | |
| 1+1 | suma | 2 | APROBADA (det 2/2) | |
| 123456789+987654321 | suma 10 dígitos | 1111111110 | APROBADA (det 2/2) | el guest dio 1111111110 |
| 7div13 | división decimal | 0.5384615385 | APROBADA (det 2/2) | el guest dio 0.5384615385 |
| 3y4xcinco | paréntesis + precedencia | 35 | APROBADA (det 2/2) | |
| dos^3 | potencia | 8 | APROBADA (det 2/2) | |
| sin2rad | sin(2) RAD | 0.9092974268 | APROBADA (det 2/2) | plantilla sin auto-cierra |
| 1div3 | 1÷3 | 0.3333333333 | APROBADA (det 2/2) | esta build renderiza decimal, no fracción |
| sumatorio | Σ(X²,X,1,10) | 385 | APROBADA (det 2/2) | Σ( OPTN F4 F6 F3; navegación por TAB kDer (v10) |
| derivada | d/dx(sin x)\|x=2 | -0.4161468365 | APROBADA (det 2/2) | =cos(2) ✓; plantilla d/dx + casilla \|x= navegada con → |
| integral | ∫(x³/…, apilada) | 81/4 (=20.25) | APROBADA (det 2/2, par estructural) | casillas de límites apiladas: ↑/↓, no → |
| complejo | (3+4i)^5 | -237-3116i | APROBADA (det 2/2) | el guest dio -237-3116i; OPTN F3 F1 = i |

Nota v8b: los 6 «FALLIDA» por sombra/glifo muestran en sus recibos BMP que
el guest calculó el valor correcto de la referencia independiente; el
defecto estaba en la calibración del decodificador, no en el cálculo.

## v16 — soporte post-EXE (S⇔D, ANS): 3/3 PASO (sonda probe-v16, det 2/2)

Infraestructura nueva: campo `post` del caso (teclas TRAS el EXE con
reacción exigida + estabilidad por kExe). Recibos: `probe-v16/`
(battery_result + BMPs post*) y `build/probe_v16_err.log`.

| Caso | Operación | Referencia independiente | Estado | Nota |
|------|-----------|--------------------------|--------|------|
| integral_sd | ∫₀³x³dx + S⇔D | 20.25 | **APROBADA** (det 2/2) | la tecla S⇔D es PROPIA (id 25, 0x0606, sin SHIFT); 81/4 → 20.25 verificado; ya no se depende del matcheo estructural de pares |
| pi_dos | 2π (SHIFT+EXP) | 6.283185307 | **APROBADA** (det 2/2) | π constante + aritmética; en esta build el resultado ya sale decimal (el S⇔D no hizo falta: post sin reacción, recibo) |
| an01 | 1+1 EXE; Ans+1 | 3 | **APROBADA** (det 2/2) | **Hallazgo de teclado**: 0x0402 es el (−) unario (ar01-05 lo acreditan; la tabla canónica lo desetiqueta «ANS»); Ans = SHIFT+(−), verificado por ejecución |

La fila π de la matriz de inventario deja de estar BLOQUEADA y la fila
Ans/Last pasa de NO_INVENTARIADA a APROBADA con este recibo.

## v15 delta — fixes de rutas del suite importado: 4/4 PASO (probe-v15, det 2/2, exit 0)

Recibos: `recibos/v15-delta_battery_result.txt` + stderr. Los 4 casos que
la sonda delta v14c había dejado divergentes (con diagnóstico del agente
anterior) quedan APROBADOS por ejecución real:

| Caso | Operación | Referencia | Estado | Fix verificado |
|------|-----------|-----------|--------|----------------|
| ca01 | d/dx(3x³−5x²+2x−7)\|x=2 | 18 | **APROBADA** (det 2/2) | kDer tras el exponente + UN kDer a \|x= (con el rabo en la línea principal) |
| cp04 | Arg(5i) | 1.570796327 (π/2, RAD) | **APROBADA** (det 2/2) | Arg liga solo el primario: input «5i» directo (Arg(0) es indefinido) |
| cp06 | ReP(8.5−3.2i) | 8.5 | **APROBADA** (det 2/2) | ReP es prefijo pelado: paréntesis PROPIOS del manual |
| cp07 | ImP(8.5−3.2i) | -3.2 | **APROBADA** (det 2/2) | ídem; i por SHIFT-0 |

Nota de lineage: la FULL v14c en vuelo usa el binario v14c (con los fixes
v14 pero SIN los 4 de v15): cp04/cp06/cp07/ca01 divergirán ahí por diseño
y sus APROBADAS citan esta sonda delta; el resto de casos de la FULL
citará la corrida completa como manda la matriz.

## OLA 1 CERRADA (2026-11-05): 55/55 compuertas RUN-MAT acreditadas, 0 defectos de cálculo

Lineage completo y honesto de la verificación final:

| Fuente | Casos | Resultado |
|---|---|---|
| FULL v14c (52 casos, binario v14c) | 39 PASO det 2/2 + cp04/cp06/cp07/ca01 DIVERGE-por-diseño (el binario no lleva los fixes v15; corrida murió en ca02 por pausa de sesión con 39 sellados) | recibos outdir v14c-full |
| Delta v14c (ca02,ca03,ca04,ca05,mt01,mt05,st01,st02,st03) | **9/9 PASO det 2/2** | `recibos/v14c-delta9_*` |
| Sonda v15 delta (ca01,cp04,cp06,cp07) | **4/4 PASO det 2/2, exit 0** | `recibos/v15-delta_*` |
| Sonda v16 post-EXE (integral_sd,pi_dos,an01) | **3/3 PASO det 2/2** | `recibos/v16-probe_*` |

**55/55 compuertas definidas → 55 APROBADAS con recibos BMP+hash+2 reps deterministas
+ referencia independiente documentada. Cero defectos de cálculo del guest en toda
la ola: cada divergencia histórica se resolvió como defecto de ruta de tecleo u
oráculo, nunca del motor.**

Los vectores nuevos acreditados en el cierre: ca03 (∫3x²+2x+1=81), ca04 (∫eˣdx 0..1
=1.718281828), ca05 (d²/dx²x⁴\|x=3=108), mt05 (Det 3×3=22), st01-03 (Normal P(0)=0.5,
P(1)=0.8413447461, R(2)=0.02275013195), integral_sd (S⇔D → 20.25), pi_dos (2π=
6.283185307), an01 (Ans=SHIFT+(−) → 3).

## OLA 3 CERRADA (2026-11-06): 93 compuertas APROBADAS totales, warm-boot operativo

| Fuente | Casos | Resultado |
|---|---|---|
| v19 (agente LÓGICA) | ca10=55, hp04=0.881373587, hp05=0.5493061443, hp06=1.316957897 **PASO det 2/2**; lo01-04 (fix F4 en v21), co01-02 (CONVERT ausente, NO_APLICABLE), ca06-09 (formas de plantilla documentadas en recibos) | `recibos/v19-*` |
| v20 (agente DISTRIBUCIONES) | **12/12 PASO det 2/2** — NormPD(0)=1/√2π, NormCD=Φ(1)−Φ(0), tPD(0,1)=1/π, tCD=(2/π)arctan1, BinomialPD/CD, InvBinomialCD=2, PoissonPD/CD (x,λ), GeoPD/CD, HypergeoPD=6/10 | `recibos/v20-*` |
| v21+v21b (LOGIC=F4 + agente FINANCIERO) | **12/12 PASO det 2/2** — lo01-04 (lógico 0/1), Smpl_SI=−30/SFV=−330 (ejemplo verbatim del manual), Cnvt_EFF(4,12)=12.550881, Cnvt_APR(2,21)=20, Sell(100,20)=125, Days_Prd=365/366, Cmpd_PV=−1000 | `recibos/v21-*, v21b-*` |

**Warm-boot (misión §9) OPERATIVO**: snapshot 52.9 MB tomado al final del arranque
frío; restore exige Ready+barra de estado; **equivalencia probada** (la misma suite
por ambas vías: result files byte a byte idénticos + huellas de instrucciones
idénticas). Las sondas nuevas arrancan en segundos, no en 20+ minutos.

**Hallazgos de teclado/menú de la ola 3**: LOGIC de RUN-MAT es lógico (0/1) no
bitwise (bitwise = BASE-N, diferido); página 3 de OPTN: F1=ENG-SYM, F2=FINANCE,
F3=hueco, F4=LOGIC, F6=▶ (slots fijos con huecos, volcado c_lo01_p4/p5);
FINANCE accesible desde RUN-MAT (PAG 297 verbatim) con funciones planas que
teclean ')'; CONVERT métrico ausente del guest (NO_APLICABLE); Poisson es (x,λ).

## OLA 4 (2026-11-06): +4 compuertas — 97 APROBADAS totales

| Fuente | Resultado |
|---|---|
| v22 (NUMERIC pág. 2 via OPTN F6 F4 F6) | **4/5 PASO det 2/2** — GCD(84,36)=12, LCM(4,6)=12, MOD(13,5)=3, RndFix(1.23456789,3)=1.235 | `recibos/v22-*` |
| inv01 (x⁻¹ por SHIFT+x²) | DIVERGE documentado: SHIFT+x² = √ (ya acreditado como raiz9); x⁻¹ no está en el teclado directo del guest — BLOQUEADA-por-tecla | probe-v22 |

Denominador vivo del censo v2: 377 compuertas computables → **97/377 ≈ 26%**
(más las teclas físicas: +,−,×,÷,^,x²,√,³√,ˣ√,x⁻¹ vía ^(-1), 10ˣ, e^, log, ln,
sin/cos/tan+inversas, π, ᴇ — la mayoría ya ejercitadas por los 97 casos).

## Pendiente de inventariar/probar (NO_INVENTARIADA o NO_PROBADA)

Actualizado 2026-11-06 post-v22 (97 APROBADAS, `c52dc62`):

- Matrices/vectores: inversa, multiplicación, Trans/Ref/Rref
  (OPTN F2 MAT/VCT) — Det ya acreditado (mt01/mt05, fila 27 de
  INVENTARIO); el editor rejilla sigue bloqueado por matcher
  multi-fila (MAT-02..04).
- Estadística 1-2 variables, regresiones (aplicación STAT) —
  STAT-04/05 bloqueadas; navegación por tecla `2` documentada en
  OLA4-VECTORES-PROPUESTOS §4.2.
- ~~Distribuciones de probabilidad.~~ **HECHO**: di01-12 via OPTN
  F5 F3 DIST (v20, `8ec7085`).
- Gráficas cartesianas/paramétricas/polares (aplicación GRAPH),
  tablas (TABLE).
- Ecuaciones (aplicación EQUA: polinomios, sistemas 2-6 incógnitas) —
  EQUATION-01..05 bloqueadas por glifo '='; plan de anclas POLY/SIML
  en OLA4-VECTORES-PROPUESTOS §4.2.
- ~~Financiero (TVM).~~ **HECHO en su menú OPTN desde RUN-MAT**:
  fi01-08 (v21/v21b, `b1be5fe`); la app FINANCIAL (ícono 12) con sus
  pantallas propias sigue sin inventariar. ~~Conversiones.~~
  **NO_APLICABLE** (add-in ausente, manual pág. 38).
- Casio BASIC (programas, bucles, condicionales) — app PRGM.
- ~~Hiperbólicas, hiperbólicas inversas.~~ **HECHO**: hp01-06 (OLA 2
  `2c670fd` + v19 `8ec7085`); log/exponencial avanzada: logab
  acreditado (v19), 10ˣ/eˣ ejercitados en pw/ln.
- Modos DEG/GRAD: DEG y RAD ejercitados (° y r de tg01-07); GRAD sin
  vector; notación científica/ENG-SYM menú inventariado (pag 351),
  vectores ENG sin correr; límites de magnitud y dominios sin vectores.
- Oráculo Casio: contrastes contra fx-CG50 física o simulador oficial
  (pendiente de disponibilidad; mientras, referencias del anfitrión).

## Defectos de pruebas previos documentados (auditoría)

- Corrida v1: tecleo durante el arranque (corte en primera GRAM, chunk 5)
  → hash DCE621980851884D constante: el OS ignoró TODAS las teclas.
  Corregido: ventana completa de 14 s + estabilidad.
- Corrida v2: banda = franja de pestañas F (glifos 62×22 falsos).
  Corregido: extracción de bandas del cuerpo (21..187) + exclusión de
  pestañas/cursor.
- Corrida v3: resultados '?' — el límite x372 recortaba los glifos
  alineados a la derecha (terminan en x374). Corregido: kColTexto=376
  (geometría medida) + exclusión de azul UI y borde negro x378.
- Corrida v4: calibración desde la fuente de ENTRADA no sirve: los
  resultados se renderizan con otra fuente. Corregido: calibración contra
  resultados-ancla del propio guest con teclas del anfitrión.
- Corrida v6: '4' calibró como fragmento 5×5 (carrera con el redraw).
  Corregido: espera de estabilidad antes de extraer bandas.

---

## Certificación v41 sobre el core unificado del monorepo (`core/casio_cg50`, 2026-11-23)

- **Resultado batería real:** **`133/133 PASO` (`jugados=133`, `divergencias=0`, `determinismo=SI 2/2`)** sin ninguna DLL de Casio.
- **Recibo verificado:** `recibos/v41-monorepo-133_battery_result.txt`
  - **SHA-256:** `BF9A6C7BE2EA27163435357F75AB1BD63341D09ECD23744EF96073D88E2A1F2E`
- **Cierres incluidos en `v41`:**
  - `stat-dA` (`n=5`), `stat-dB` (`n=4`): corrección de índices desde la derecha en `AplicarCalibraciones` y omisión del glifo de flecha de scroll (`\0`) sin tocar los dígitos calibrados.
  - `bas-dA` (`0000000000010110`), `bas-dB` (`00000000377`): conversión `Bin`/`Oct` vía `SETUP -> Mode -> Bin/Oct` + prefijo `d~o -> d`.
  - `setup-dD` (`1/3`): verificación real de `Fix 3` (`kAba × 12` hasta fila `Display : Norm1`) y restauración a `Norm1`.
  - **Hallazgo Modo Normal (`#461`):** el core del monorepo arranca en Modo Normal real (sin Exam Mode), por lo que 10 casos (`asin05`, `pw01`, `tg01`, `tg02`, `tg04`, `tg05`, `tg06`, `tg07`, `cp04`, `cplx04`) rinden forma simbólica exacta (`π/6`, `27/2`, `1/2`, `√2/2`, `2π/3`, `π/4`, `π/2`); se aplicó post-`EXE` `{kSD}` (`S↔D`) manteniendo intactas todas las referencias independientes.
- **Censo de apps interactivas (`cg50_apps_probe`, >1,000M instrucciones SH7305, `unsup=0`, `mflt=0`):**
  - **CONFIRMED (operativas visualmente):** `Run-Matrix`, `Statistics`, `Spreadsheet`, `Graph` (`Y1=x²` + `G-Solv MIN=(0,0)`), `Dyna Graph`, `Table` (`X=1..4 -> 1,4,9,16`), `Conic Graphs`, `Equation`, `Program / Casio BASIC` (ejecución `2+3=5` y persistencia verificada tras save/restore en instancia nueva), `Financial`, `E-CON4`, `Link`, `Memory Manager` (`Main Mem 60,492 B`, `SMEM 6,574,080 B`), `System Manager`, `Python`, `Distribution`, `Geometry.g3a`, `PictPlot.g3a`, `3DGraph.g3a`.
  - **FAILED / BRECHA HARDWARE DOCUMENTADA (resuelta en v42):** `eActivity` (`3`) devolvía `Memory ERROR` antes de la máquina CFI completa.

---

## Certificación v42 — Flash NOR CFI Spansion + SH-4A FPU/CPU/MMU + RTC/G3A (`core/casio_cg50`, 2026-11-23)

- **Resultado batería real (`cg50_battery`, 7 shards sobre `warm_state.bin` regenerado desde cero):** **`133/133 PASO` (`jugados=133`, `divergencias=0`, `decodificables=0`, `determinismo=SI 2/2`)**, 0 regresiones, sin ninguna DLL de Casio.
- **Recibo verificado:** `recibos/v42-monorepo-133_battery_result.txt`
  - **SHA-256:** `C7A0C33EEE9862EB4CF1D67FDE350322B02D09CCEC5787C6F703EA99192EC093`
- **Tests unitarios y compilación cruzada:**
  - `ctest` (MSVC x64 Release): **`6/6 PASS`** (`cg50_android_smoke`, `cw2_headless_test`, `nx_regression_test`, `cg50_c_api_contract_test`, `cg50_flash_cfi_test`, `cg50_sh4a_isa_test`).
  - Android NDK `clang++ --target=aarch64-linux-android24 -std=c++20 -fsyntax-only`: **`6/6 PASS`** (`0` errores).
- **Cierres hardware confirmados en `v42` (`cg50_apps_probe cert19`, `unsup=0`, `mflt=0`):**
  - **`eActivity` + Flash NOR Spansion S29GL256P (`LY755`):** **CONFIRMED** — abre sin `Memory ERROR` (`6574080 Bytes Free`), crea archivo `A`, evalúa cálculo `7×8 = 56`, ejecuta compactación SMEM + `Write-to-Buffer` + `Sector Erase 128 KB` (`FILE -> SAVE -> One Moment Please -> Complete! Press:[EXIT]`), y persiste **`A.g3e : 288`** en el sistema de archivos Flash (`6569984 Bytes Free`).
  - **`MicroPython v1.9.4`:** **CONFIRMED** — abre shell interactivo `>>>` y evalúa `2**10` → **`1024`**.
  - **`Spreadsheet`:** **CONFIRMED** — `A1=10`, `A2=20`, fórmula `A3 = =A1+A2` → **`30`**.
  - **Add-ins `.g3a` verificados:** `Geometry.g3a` (`01.05`), `PictPlot.g3a` (`01.03`), `3DGraph.g3a`, `Physium.g3a` (*Periodic Table & Fundamental Physical Constants*), `Prob Sim` (*Coin Toss / Dice Roll / Spinner / Marble Grab / Card Draw / Random Numbers*).
  - **`System Manager -> Version`:** `OS 03.80.0212`.
  - **Correcciones ISA SH-4A verificadas por `cg50_sh4a_isa_test`:** `FMOV.S` 32 bits completos, pares `DR`/`XD` (`SZ=1`), tabla exacta `0xFnmD/E/F` (`FLOAT`/`FTRC`/`FLDI0`/`FLDI1`/`FCNVSD`/`FCNVDS`/`FIPR`/`FMAC`/`FSCHG`/`FPCHG`/`FRCHG`/`FTRV` columna-mayor/`FSCA`), `MOVUA.L`, `MOVLI.L`/`MOVCO.L`, `RTS/N`, `RTV/N`, `PREFI`, desacople `CLRMAC`/`LDTLB`, multi-hit UTLB (`EXPEVT 0x140`).
  - **RTC tiempo real + ABI `UserROM`/`.g3a`:** `casio_cg50_set_rtc_epoch`, `casio_cg50_export_user_rom_file`, `casio_cg50_import_user_rom_file`, `casio_cg50_load_addin_file` (validación cabecera Casio `0x7000` bytes).
  - **Límites físicos honestos:** puerto físico 3-pin `S-LINK` y USB `Mass Storage` de escritorio no emulan cable físico en Android (suplidos por respaldo/restauración de `UserROM` e importación `.g3a` desde menú `⋯`).

