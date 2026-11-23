# CENSO_MANUAL_SOFT v2 — Resumen de clasificación

**Fuente:** `CENSO_MANUAL_SOFT_v1.csv` (545 ítems de menú del manual Soft fx-CG50, ES, 663 págs).
**Desambiguación:** texto completo `soft_manual_texto.txt` (marcadores `=== PAG N ===`).
**Salida:** `CENSO_MANUAL_SOFT_v2.csv` (columnas: `item_menu, hits, paginas, categoria`).

Método: reglas por palabra clave (GRAPH→18, Mat→14, List→15, PMT/PV→21, JUMP/If→22, colores→0-UI…) y, para los ambiguos, el capítulo de la página donde aparece el ítem (cap 1 págs 38-46 = menús OPTN/VARS/PRGM y SETUP; cap 2 = menús de funciones; cap 3 = listas; cap 4 = ecuaciones; cap 5 = gráficas/cónicas; cap 6 = estadística; cap 7 = financiero; cap 8 = PRGM; cap 9 = hoja de cálculo; cap 10 = eActivity; cap 13 = Link; cap 15 = Picture Plot; cap 17 = Python). Los ítems de SETUP (Fix/Sci/Norm/Deg/Rad/Gra/Eng, Coord/Grid/Axes/Label, d/c vs ab/c, BEGIN/END, 365/360, Annual/Semi, colores, Intsect/Union, Std/OnData, Form/Val, Low/Right…) se clasifican como **0-UI-chrome** salvo que sean funciones computables. Ítems de composición tipográfica (glifos vacíos `/H11623`, `/H18554`, `|`, ítem sin nombre) → 0-UI-chrome.

## Totales por categoría

| Categoría | Ítems |
|---|---|
| 0-UI-chrome | 168 |
| 1-Aritmetica | 41 |
| 2-Potencias-raices | 0 |
| 3-Trigonometria | 7 |
| 4-Hiperbolicas | 7 |
| 5-LogExp | 1 |
| 6-Complejos | 8 |
| 7-Combinatoria | 3 |
| 8-Derivada | 6 |
| 9-Integral | 1 |
| 10-Sumatorio | 1 |
| 11-Ecuaciones | 4 |
| 12-Sistemas | 3 |
| 13-Polinomios | 3 |
| 14-Matrices | 21 |
| 15-Listas | 41 |
| 16-Probabilidad | 65 |
| 17-Regresiones | 30 |
| 18-Graficas | 66 |
| 19-Tablas | 7 |
| 20-Recurrencias | 4 |
| 21-Financiero | 38 |
| 22-PRGM-BASIC | 15 |
| 23-Sistema-apps | 5 |
| ?-dudoso | 0 |
| **Total** | **545** |

## Compuertas computables

- **Número FINAL de compuertas-computables (todas menos 0-UI-chrome): 377** de 545.
- ?-dudoso: 0 (todos los ítems quedaron clasificados con evidencia de página).

Notas sobre categorías casi vacías (no es pérdida: son teclas físicas del teclado, no ítems de menú):
- **2-Potencias-raices = 0**: `^`, `√`, `x²`, `x⁻¹`, `³√` son teclas/dengo del panel, no aparecen como ítem de menú en el censo. (`Power`, `X²`, `X³`, `X⁴` del censo son *modelos de regresión*, asignados a 17.)
- **5-LogExp = 1** (`log a b`): `log` y `ln` son teclas físicas. El `Log` del censo es regresión logarítmica (17).
- **9-Integral = 1** (`∫ d x` del menú MATH) y **10-Sumatorio = 1** (`Σ (`): `∫` y `Σ` como plantillas naturales ya están en la batería.

## 30 compuertas computables más importantes SIN vector en la batería actual

(La batería ya cubre: sin/cos/tan+inversas, log/ln, √, x², ^, ∫, d/dx, Σ, complejos i/Abs/Arg/Conjg/ReP/ImP, Det, Mat literales, nPr/nCr/!, sinh/cosh/tanh, Abs/Int/Frac/Intg, Normal P/Q/R/t, π, Ans, S-D, fracciones, hiperbólicas.)

| # | Ítem (censo) | Cat. | Página | Qué verifica |
|---|---|---|---|---|
| 1 | Solve | 11-Ecuaciones | 74 | solve(f,g) por Newton |
| 2 | SolveN | 11-Ecuaciones | 74 | soluciones de f(x)=0 |
| 3 | FMin | 8-Derivada | 74 | mínimo numérico de función |
| 4 | FMax | 8-Derivada | 74 | máximo numérico de función |
| 5 | d 2 / d x 2 | 8-Derivada | 74/25 | segunda derivada numérica |
| 6 | log a b | 5-LogExp | 25/74 | logaritmo en base arbitraria |
| 7 | GCD | 1-Aritmetica | 63 | máximo común divisor |
| 8 | LCM | 1-Aritmetica | 63 | mínimo común múltiplo |
| 9 | MOD | 1-Aritmetica | 63 | resto de n÷m |
| 10 | Int÷ | 1-Aritmetica | 74 | cociente de división entera |
| 11 | Rnd | 1-Aritmetica | 63 | redondeo al formato Fix/Sci/Norm |
| 12 | Ran# | 16-Probabilidad | 63 | aleatorio [0,1) |
| 13 | Pol( | 3-Trigonometria | 64 | rectangulares→polares |
| 14 | Rec( | 3-Trigonometria | 64 | polares→rectangulares |
| 15 | ° ’ ” | 3-Trigonometria | 64 | entrada/retorno sexagesimal (DMS) |
| 16 | Ref | 14-Matrices | 102 | forma escalonada |
| 17 | Rref | 14-Matrices | 102 | forma escalonada reducida |
| 18 | Trn | 14-Matrices | 102 | matriz traspuesta |
| 19 | Identity | 14-Matrices | 98 | matriz identidad n×n |
| 20 | Seq | 15-Listas | 125 | generar lista f(x), a..b, paso |
| 21 | ΔList | 15-Listas | 125 | diferencias consecutivas |
| 22 | Cuml | 15-Listas | 125 | suma acumulada de lista |
| 23 | ¯ x (media) | 15-Listas | 39 | media de datos 1-VAR |
| 24 | s x (desv. muestral) | 15-Listas | 39 | desviación estándar muestral |
| 25 | 2-VAR | 17-Regresiones | 221 | estadística de dos variables (Σxy, r…) |
| 26 | ax + b | 17-Regresiones | 221/229 | regresión lineal (a, b, r) |
| 27 | SimRes | 12-Sistemas | 41 | matriz de soluciones de sistemas 2-6 incógnitas |
| 28 | PlyRes | 13-Polinomios | 41 | raíces de polinomio grado 2-6 |
| 29 | NPV | 21-Financiero | 285 | valor presente neto (flujo de caja) |
| 30 | BAL | 21-Financiero | 288 | saldo pendiente tras la cuota PM2 |

Menciones adicionales valiosas (fuera del top-30): `Rmdr`, `MOD_Exp`, `RndFix`, `Samp`/RanList, `z`/`Chi`/`F` (resultados de test), `Lower`/`Upper` (intervalos de confianza), `zLow`/`zUp`/`tLow`/`tUp`/`xInv` (distribución inversa), `IRR`/`PBP`/`NFV`/`'EFF`/`'APR`/`SL`/`SYD`/`DB`/`PRC`/`YLD` (financiero), `FOCUS`/`VERTEX`/`CENTER`/`ASYMPT` (G-SOLVE cónicas), `a n` (recursión), `If`/CellIf (PRGM), prefijos ENG `k`/`M`/`µ`…, `'Dec`/`'Hex`/`'Bin`/`'Oct` (Base-N), `Mean`/`Med`(mediana)/`Q 1`/`Q 3`/`Min`/`Max`/`Sum`/`Prod`/`%`.

## Criterios aplicados en los casos ambiguos (traza)

- **Ítem compartido entre contextos** → gana el contexto con más páginas: `Med` (2 págs mediana VARS/LIST vs 3 págs Med-Med regresión) → 17; `Norm`/`Bin` (mayoría SETUP) → 0-UI; `Int` (Int( numérico + RanInt en la misma pág 63) → 1; `List` (mayoría menús LIST) → 15; `X`/`Y`/`r` (V-WIN + función gráfica) → 18/18/17; `b`/`d` (coeficiente de regresión pág 39 vs símbolo Base-N pág 89) → 17; `p` (valor-p pág 40 vs prefijo pico pág 64) → 16; `n` (recuento 1-VAR; también periodos financieros y prefijo nano) → 15.
- **Menús OPTN/VARS/PRGM de la pág 38-42** → categoría de su contenido: LIST→15, MAT/VCT→14, COMPLEX→6, CALC→0-UI (cabecera mixta), STAT→16, CONVERT→0-UI, HYPERBL→4, PROB→16, NUMERIC→1, ANGLE→3, ENG-SYM→0-UI, PICTURE/FUNCMEM/CAPTURE→0-UI, FINANCE→21, LOGIC→1, V-WIN/FACTOR/DYNA→18, TABLE→19, RECURSION→20, EQUATION→11, Str→22, COMMAND/CONTROL/JUMP/RELATNL/I/O/CLEAR/DISPLAY/?/:/STR/Prog→22.
- **Variables VARS STAT (pág 39-40)**: lado x y cuantiles → 15 (n, x̄, sx, σx, Σx, Σx², minX/maxX, Q1/Q3, Mod); lado y apareado → 17 (ȳ, Σy, Σy², Σxy, σy, sy, minY/maxY, a-e, r, r², MSe); resultados de test/intervalos/distribuciones y ANOVA → 16 (z, t, Chi, F, p̂, df, se, SS*/MS*/F*/p*, Lower/Upper, xInv, zLow/zUp, tLow/tUp, PTS, INPUT, RESULT, TEST, INTR, DIST).
- **Modelos de regresión** (ax+b, a+bx, Med-Med, X², X³, X⁴, Log, ae^bx, ab^x, Power, Sin, Logistic, 2-VAR, REG) → 17. Tipos de gráfico estadístico descriptivo (Scatter, xyLine, Pie, Hist, MedBox, Bar, Broken) → 15; NPPlot/N-Dist → 16; GRAPH1/2/3/DRAW → 18.
- **Cónicas G-SOLVE (pág 204)**: FOCUS, VERTEX, CENTER, LENGTH, DIRECTX, SYMMETRY, X/Y-ICEPT, RADIUS, ASYMPT → 18 (cap 5); `e` (excentricidad/coef. cuártico) → 17.
- **SETUP y sus comandos equivalentes en PRGM (pág 300)** → 0-UI-chrome (Coord, Grid, Axes, Label, Skt/Lin, Deriv, Func, Simul como ajuste, Back, SGV-WIN, Locus, Tbl-Var, ΣDisp, Resid, Frac, Y=Speed, Date, Periods, Ineq, Simp-modo, Q1Q3, P/L-Clr); `PMT` (mayoría financiera) y `SIMUL` (solucionador pág 138) → 21 y 12; `DRAW` → 18.
- **Hoja de cálculo (pág 367-368)**: Cell( (CellMin/Max/Mean/Median/Sum/Prod), SEQ, SORTASC/DES, 1-VAR → 15; REG/2-VAR → 17; If/CellIf y RELATNL → 22; FILE/EDIT/CUT/PASTE/COPY/FILL/GRAB/$/CONDIT/CONTENT/FORMAT/VAR/RECALCS → 0-UI.
- **eActivity (pág 403-405/421)**: 'MAT/VCT → 14, 'LIST → 15; STRIP/TEXT/STOP/CALC/MATH/COLOR/MARKER/MEMO/PageUp/PageDown/DEL-LINE/Catalog/VIEW/SIZE/OPT/CAPACITY → 0-UI.
- **Link (pág 446/456)**: RECV/TRANSMIT/CABLE/WAKEUP → 23-Sistema-apps; SELECT/CURRENT/ALL → 0-UI.
- **Picture Plot (pág 525-527)**: DefG/MODIFY → 18, REG → 17; AXTRNS/T-Y/T-X/PLAY/PAN/FadeI/O/UNDO/Switch/Cutout/P-LINK/DEL•BTM/1~20/Plot → 0-UI.
- **Python (pág 566)**: llaves `{ }` de dict/set → 23-Sistema-apps; `|` (tabla ASCII de compatibilidad py, pág 582) → 0-UI-chrome.
- **Literales de lista con llaves** (`{g,h,i}`, `{56,82,64}`, `{41,65,22}`, `{1,2,3}`, `{ }` pág 24) → 15-Listas (sintaxis de ingreso de listas).
