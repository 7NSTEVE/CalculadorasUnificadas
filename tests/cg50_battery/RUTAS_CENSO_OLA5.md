# RUTAS Y ORÁCULOS — CENSO v2 → OLA 5 (minería del manual soft.txt ES v3.30)

Fecha: 2026-11-06 · Fuente: `D:\Temp\opencode\manuales\soft.txt` (663 págs, marcadores `[PAGINA n]`) ·
Censo: `CENSO_MANUAL_SOFT_v2.csv` (545 ítems) · Exclusiones: `MATRIZ_INVENTARIO.md` + `MATRIZ-COMPATIBILIDAD.md` + OLA4.

## 0. Reglas de honestidad

- Marcas de oráculo: **V** = verbatim del manual (teclas y resultado en texto), **C** = construido a mano desde
  fórmula/ejemplo verbatim (el resultado del manual está en imagen, no en texto), **H** = hipótesis de ruta (slot F
  no confirmado), **IMG** = el manual muestra la pantalla como imagen (formato exacto por confirmar).
- Toda página se cita `[PAGINA n]`. Los números de Casio en secuencias de teclas: a=0, b=1, … j=9
  (verificado: `bbaaba`=110010₂, `b5-e`=1E−4, `bca`=120).
- No se incluye nada ya acreditado (97 APROBADAS) ni en vuelo (logab v14c, fracción kFrac v14c, llaves/coma v23,
  SIMUL 2×2 ancla ya propuesta en OLA4, Solve/SolveN v23, MOD_Exp/Rmdr/Int÷ y plan '+'/'=' de OLA4).

## 1. Conflictos resueltos (insumos verificados — usar como canónicos)

1. **° ya acreditada**: ruta `OPTN F6 F5 F1` confirmada por EJECUCIÓN REAL (probe v14a: hueco en F3 pág 2;
   v14b: tg01 = 0.5 PASO det 2/2). La nota "'°'=OPTN F6 F3 F1" era errónea (F3=PROB). **NO es candidato OLA5.**
2. **FINANCE resuelto por manual verbatim** [P297, L15844/15855]: Math I/O → `K6(g)6(g)2(FINANCE)`;
   Line I/O → `K6(g)6(g)6(g)1(FINANCE)`. El guest es Math I/O → **OPTN ▶▶ F2** (coincide con v21).
   Causa: en Math I/O el menú OPTN pág 3 es más corto (sin PICTURE/FUNCMEM/CAPTURE) y FINANCE cae en F2.
3. **CALC pág 3** [P74 verbatim `K4(CALC) 6(g) 6(g) 1(Int÷)`]: pág3 = F1=Int÷, F2=Rmdr, F3=Simp.
   Corrige la hipótesis F4/F5 de OLA4. (Int÷/Rmdr excluidos por OLA4; **Simp queda libre**.)
4. **MAT/VCT = OPTN F2** (probe v14a: `LIST|MAT/VCT|COMPLEX|CALC|STAT`). El texto de [P91] dice `3('MAT/VCT)`:
   discrepancia del manual; gana el probe de ejecución real. Slots pág 2 verbatim [P100]: `6(g) 2(Dim)` → F2=Dim,
   F3=Fill( (mismo submenú). Submenú de List: `6(g)1(List)` tras ▶ vuelve a pág 1 F1=List [P127-128].
5. **LOGIC**: guest RUN-MAT = OPTN ▶▶ F4 con F1=And,F2=Or,F3=Not,F4=Xor (v21). El manual [P89] lista el menú
   LOGIC **dentro de BASE**: Neg=F1, Not=F2, and=F3, or=F4, xor=F5, xnor=F6. El orden BASE en el guest está por verificar.
6. **Submenú FINANCE — VERIFICADO POR FRAMES (v26diag, 2026-11-12)**: `OPTN F6 F6 F2` abre FINANCE con submenú
   `SIMPLE | COMPND | CASH | AMORTZN | CONVERT | ▶` (la pág. 2 vía ▶ aloja TAX/COST-Sell-Margin/DAYS/BOND; fi05 ya
   acreditó `kF6,kF1,kF2` = submenú pág.2 → Cost → Sell). Los picks de submenú se hacen con TECLAS F, NO con
   dígitos (fin-dB: el '2' fue a la línea de entrada y NO pickió COMPND). Pendiente de frames (sonda v27eqn en
   vuelo): slots de COMPND (fin-dC), AMORTZN (fin-dD), pág. 2 ▶ (fin-dE), TAX (fin-dF). Evidencia: 13 frames
   `c_fin-dA_*`/`c_fin-dB_*` en `D:\Temp\opencode\battery-out-findiag\png\` y copia permanente en
   `E:\Descompilados\11_Otros\Emuladores\Evidencia_CG50\bateria-cg50-nativa\outdir-findiag\`.
7. **Cruce con MATRIZ_INVENTARIO**: fi01-08 APROBADAS (Smpl_SI/SFV, Cnvt_EFF/'APR, Sell, Days_Prd, Cmpd_PV) → excluidos;
   d/dx, d²/dx², ∫dx, Σ( en vuelo/acreditados → excluidos; editor rejilla MAT BLOQUEADA (multi-fila); STAT valtable
   BLOQUEADA; EQUATION app BLOQUEADA; x⁻¹ DIAGNOSTICADA (SHIFT+x² inserta √□) → **inversa de matriz inviable** hasta
   inventariar la tecla; CONVERT NO_APLICABLE (v19).
8. **Política OLA4 #12** [TOPCANDIDATAS L44]: PROHIBIDO ampliar la batería a aplicaciones hasta cerrar RUN-MAT e
   inventario MATH completo → toda la sección C queda **EN ESPERA** (se mapea, no se recomienda para OLA 5).
   Excepción: SETUP es interno de RUN-MAT (no es app de menú) → BASE y Fix/Sci/Norm/Eng no infringen la política.
9. **Mojibake recuperable del censo** (por contexto de glifo): `/g151`=µ, `/g433x`=σx, `/g433y`=σy, `/g922`=Σx²,
   `?List`=ΔList, `? d x`=∫dx, `S  (`=Σ(, `S  INT/PRN`=ΣINT/ΣPRN, `'r ?/g423`='r∠θ. **Corruptos reales** (no mapeables):
   líneas CSV 179, 378, 388, 390, 434, 445, 548, 549 (`) y !/( `, `") g,h,i!/( `, `") fg,ic,`, `") eb,gf,cc!/( `,
   `/H11623`, `") b,c,d`, `") {`, `) ) !/( `).

**Leyenda de riesgo**: `glifo` = glifo nuevo en entrada/eco/salida · `app` = navegación de aplicación (política #12) ·
`SETUP` = cambia estado global (save/restore ya verificado en infra) · `rejilla` = plantilla/editor de celdas ·
`det` = resultado iterativo/probabilístico — exigir determinismo 2/2 · `llaves` = depende de `{ }` (v23 en vuelo).

---

## 2. Categoría A — RUN-MAT, ruta corta, sin glifos nuevos (candidatos primarios OLA 5)

| ítem (censo) | ruta de teclas | pág. manual | oráculo (marca) | riesgo |
|---|---|---|---|---|
| FMin | OPTN F4(CALC) F6 F1(FMin) | P74 (menú), P83 (ej) | `FMin(x²−4x+9,0,3,6)` → mínimo en **x=2**, f(2)=5 (C, IMG) | det (búsqueda numérica); formato de pantalla por confirmar |
| FMax | OPTN F4 F6 F2(FMax) | P74, P83 | `FMax(x²−4x+9,4,7,6)` → **x=7** (f(7)=30) (C) | det; ídem |
| Rnd | OPTN F6 F4(NUMERIC) F4(Rnd) | P51 (ej, L2683), P63 (menú) | V: Fix 3 → `200÷7 w` → 28.571; `Rnd w` → 28.571; `×14 w` → **399.994** | SETUP Fix 3 (estado global) |
| Q( | OPTN F6 F3(PROB) F6 F2(Q( ) | P233 | `Q(0)` → **0.3989422804** = 1/√(2π) (C) | H slot F2 (P/R acreditados en F1/F3, st01-03) |
| Conjg | OPTN F3(COMPLEX) F4(Conjg) | P85-86 | V: `Conjg(2+4i)` → **2−4i** | — (i=SHIFT+0 verificado; salida con i ya calibrada) |
| Dim (MAT) | OPTN F2(MAT/VCT) F6 F2(Dim) | P100 | V: `Dim(Mat A)` con A=[[1,2,3][4,5,6]] → **2×3** (V texto: "dos filas y tres columnas") | requiere Mat A (asignación `→` por verificar) o `Dim(MatAns)` (H) |
| 'Bin | SETUP Mode→Dec, J, DISPLAY F3 | P90 | V ej: 22₁₀ → `'Bin` → **10110** (C valor) | SETUP; sufijo de base en display por confirmar |
| 'Oct | SETUP Dec + DISPLAY F4 | P90 | V ej: 22 → **26** (C) | ídem |
| 'Hex | SETUP Dec + DISPLAY F2 | P90 | C: 22 → **16** | ídem |
| 'Dec | SETUP Bin/Hex + DISPLAY F1 | P90 | C: 10110₂ → **22** | ídem |
| Neg | SETUP Mode→Bin + LOGIC F1 | P89-90 | V ej: `Neg(110010₂)` → complemento a 2 → **1111111111001110** (=−50) (C valor, IMG) | SETUP Bin; slots LOGIC-BASE por verificar (guest LOGIC difiere) |
| xnor | SETUP Hex + LOGIC F6 | P89 | C: `120₁₆ xnor AD₁₆` → NOT16(18D₁₆) = **FE72₁₆** | H slot F6; ancho de palabra 16 bits por confirmar |
| d~o | SETUP Hex + d~o F1(d)/F2(h)/F3(b)/F4(o) | P89 | V ej: Hex mode, `1(d~o) 1(d) bcd w` (123₁₀) → **7B₁₆** (C valor) | SETUP Hex |
| Cmpd_FV † | OPTN ▶▶ F2(FINANCE) ▶ CMPD (H slot) | P297 (ruta), P347 (sintaxis) | C: `Cmpd_FV(2,10,−1000,0,1,1)` → **1210** | H slot CMPD; P/Y,C/Y explícitos (default 12/12 verbatim P347) |
| Cmpd_n † | ídem | P347 | C: `Cmpd_n(100,−1000,0,4000,1,1)` → **2** | ídem |
| Cmpd_I% † | ídem | P347 | C: `Cmpd_I%(1,−1000,0,1100,1,1)` → **10** | ídem + det (Newton) |
| Cmpd_PMT † | ídem | P347 | C: `Cmpd_PMT(12,0,−1200,0,12,12)` → **100** | ídem |
| Amt_BAL † | OPTN ▶▶ F2(FINANCE) ▶ AMORT (H slot) | P347-348 | C: `Amt_BAL(1,1,0,−1000,100,12,12)` → **−900** (I%=0) | H slot AMORT |
| Amt_INT † | ídem | P347 | C: `Amt_INT(1,1,0,−1000,100,12,12)` → **0** | ídem |
| Amt_PRN † | ídem | P347 | C: `Amt_PRN(1,1,0,−1000,100,12,12)` → **100** | ídem |
| Cost † | OPTN ▶▶ F2(FINANCE) ▶ COST (H slot) | P290 (fórmulas), P348 (sintaxis) | C: `Cost(125,20)` → **100** | H slot; Sell ya acreditado (fi07) |
| Margin † | ídem | P290, P348 | C: `Margin(100,125)` → **20** | ídem |

† = función OPTN fuera del censo v2 (el censo solo tiene las pantallas de la app Financial, sección C);
cada una cubre la aritmética del ítem de app homónimo y permite acreditarla sin navegar apps.

## 3. Categoría B — RUN-MAT con glifo nuevo o dependencia de lista/rejilla

| ítem (censo) | ruta de teclas | pág. manual | oráculo (marca) | riesgo |
|---|---|---|---|---|
| 'DMS | OPTN F5(ANGLE) F6 F3('DMS) | P64 (menú), P66 (ej) | V: `2.255 'DMS w` → **2°15´18˝** | glifo °´˝ en salida |
| °'" | OPTN F5(ANGLE) F3(°'") | P64 | V: `2°20´30˝+0°39´30˝ w` → **3°00´00˝** | glifos °´˝ en entrada |
| r | OPTN F5 F2(r) | P64, P66 (ej) | V: `4.25 K6(g)5(ANGLE)2(r) w` → **243.5070629** | glifo r superíndice en eco |
| g | OPTN F5 F3(g) | P64 | C: `100 g` → **90** (DEG) | H slot F3 (F2=r verbatim); glifo g |
| Pol( | tecla física por verificar (CATALOG alternativa) | P70 | V: `Pol(14,20.7)` → **r=24.98979792, θ=55.92839019** | BLOQUEO matcher: glifo ∠/ángulo en salida (OLA4 #4) |
| Rec( | ídem | P70 | V: `Rec(25,56)` → **13.97982259, 20.72593931** | ídem |
| 'r∠θ | OPTN F3(COMPLEX) F6 F3 | P86-87 | V: `1+√3i 'r∠θ` → **2∠60** | glifo ∠ en eco/salida |
| 'a+bi | OPTN F3 F6 F4 | P86-87 | V: `2∠60 'a+bi` → **1+√3i** | glifo ∠ en entrada (encadena con 'r∠θ) |
| Seq | OPTN F1(LIST) F5(Seq) | P125-126 | V: `Seq(X²,X,1,11,5)` → **{1,36,121}** | llaves en salida (v23); **keystone**: crea ListAns sin llaves de entrada |
| Min | OPTN F1 F6 F1(Min) ▶ F1(List) n ) | P127 | V: `Min(List 1={36,16,58,46,56})` → **16** | requiere List n creada (llaves v23 / editor STAT / Seq→ListAns H) |
| Max | OPTN F1 F6 F2(Max) | P127 | V ej: `Max(List 1={75,16,98,46,56}, List 2={35,59,58,72,67})` → **1** (devuelve el Nº de lista) (C) | 2 listas; semántica distinta al Max escalar |
| Mean | OPTN F1 F6 F3(Mean) | P127 | V: `Mean(List 1={36,16,58,46,56})` → **42.4** | dependencia List n |
| Med | OPTN F1 F6 F4(Med) | P127-128 | V ej (datos+frecuencia); C: `Med({1,2,3},{1,1,1})` → **2** | 2 listas (datos+frecuencia) |
| Sum | OPTN F1 F6 F6 F1(Sum) | P128 | V: `Sum(List 1={36,16,58,46,56})` → **212** | dependencia List n |
| Prod | OPTN F1 F6 F6 F2(Prod) | P128 | V: `Prod(List 1={2,3,6,5,4})` → **720** | ídem |
| Cuml | OPTN F1 F6 F6 F3(Cuml) | P129 | V: `Cuml(List 1={2,3,6,5,4})` → **{2,5,11,16,20}** | llaves salida + List n |
| % | OPTN F1 F6 F6 F4(%) | P129 | V: `%(List 1={2,3,6,5,4})` → **{10,15,30,25,20}** | ídem |
| ΔList | OPTN F1 F6 F6 F5(ΔList) | P130 | V: `ΔList({1,3,8,5,4})` → **{2,5,−3,−1}** | ídem (ítem censo `?List` recuperado) |
| Lst→Mat | OPTN F1 F2(Lst→Mat) | P125-126 | C: `Lst→Mat({1,2},{3,4})` → **[[1,3][2,4]]** | llaves + multilínea |
| Mat→Lst | OPTN F2(MAT/VCT) F2(Mat→Lst) | P102 | C: `Mat→Lst([[1,2,3][4,5,6]],2)` → **{2,5}** | llaves salida |
| Trn | OPTN F2 F4(Trn) | P102 | C: `Trn([[1,2][3,4]])` → **[[1,3][2,4]]** | salida matricial multilínea (matcher por verificar) |
| Identity | OPTN F2 F6 F1(Identity) | P101-102 | C: `Identity(2)` → **[[1,0][0,1]]** | ídem |
| Fill( | OPTN F2 F6 F3(Fill() | P101 | V ej: `Fill(3,Mat A)` → todas las celdas=3 (C valor) | multilínea + requiere Mat A |
| Augment (MAT) | OPTN F2 F5(Augment) | P101 | C: `Augment([[1,2][3,4]],[[5][6]])` → **[[1,2,5][3,4,6]]** | ídem |
| Ref | OPTN F2 F6 F4(Ref) | P102 | C: `Ref([[1,2][3,4]])` → **[[1,2][0,−2]]** | ídem |
| Rref | OPTN F2 F6 F5(Rref) | P102 | C: `Rref([[1,2][3,4]])` → **[[1,0][0,1]]**; ej manual 3×4 [[2,−1,3,19][1,1,−5,−21][0,4,3,0]] → **[[1,0,0,2][0,1,0,−3][0,0,1,4]]** (resuelto a mano) | ídem; 3×4 requiere plantilla m×n (rejilla) |
| ENG-SYM (k/M/G/T/P/E + m/µ/f) | OPTN F6 F6 F1(ENG-SYM) ▶ F1(k)… | P64 (menú), P72 (ej, L3916) | V: SETUP Eng → `999k+25k w` → **1.024M**; `9÷10` → ENG (pág 3 F3) → **900m** (L3920) | glifo µ nuevo; SETUP Eng; ENG/↔ENG son post-resultado (censo cat-0) |
| Cash_NPV † | OPTN ▶▶ F2(FINANCE) ▶ CASH (H slot) | P347 | C: `Cash_NPV(0,{−100,60,60})` → **20** | llaves (Csh es lista) + H slot |
| Cash_IRR † | ídem | P347 | C: `Cash_IRR({−100,200})` → **100** | llaves + det (iterativo) |
| Cash_PBP † | ídem | P347 | C: `Cash_PBP(0,{−100,60,60})` → **1.666666667** | llaves |
| Cash_NFV † | ídem | P347 | C: `Cash_NFV(0,{−100,60,60})` → **20** (NFV=NPV·(1+i)ⁿ) | llaves |
| Amt_ΣINT † | OPTN ▶▶ F2(FINANCE) ▶ AMORT | P348 | C: `Amt_ΣINT(1,3,0,−1000,100,12,12)` → **0** | Σ en el eco del nombre (glifo) |
| Amt_ΣPRN † | ídem | P348 | C: `Amt_ΣPRN(1,3,0,−1000,100,12,12)` → **300** | ídem |

Notas B: `logab` (OPTN F4 F6 F4) está **EN_PRUEBA v14c** — no duplicar. La salida matricial multilínea de
Trn/Ref/Rref/Identity/Fill(/Augment comparte el bloqueo "multi-fila" del editor (item 28) — probar primero con 2×2.
La inversa de matriz sigue bloqueada por la tecla x⁻¹ (item 34d).

## 4. Categoría C — navegación de aplicación (**EN ESPERA por política OLA4 #12**)

| ítem (censo) | ruta | pág. | oráculo | riesgo |
|---|---|---|---|---|
| SIMUL | MENU EQUATION → F1 | P138-139 | V ej 3×3 (4x+y−2z=−1 / x+6y+3z=1 / −5x+4y+z=−7) → **x=1, y=−1, z=2** (resuelto a mano); ancla 2×2 ya propuesta en OLA4 ({X=3,Y=2}) | app + glifos (item 31 BLOQUEADA) |
| POLY | EQUATION F2 | P140-141 | V: x³−2x²−x+2=0 → **X1=−1, X2=1, X3=2** | app |
| SOLVER | EQUATION F3 | P141-142 | V: H=VT−½GT² (H=14,T=2,G=9.8) → **V=16.8** (resuelto a mano) | app; plantilla de ecuación |
| SimRes/SimCoef | VARS ▶ F3(EQUATION) | P41 | C sobre SIMUL 3×3: SimRes → **{1,−1,2}** → MatAns | app previa + llaves |
| PlyRes/PlyCoef | VARS ▶ F3 | P41 | C sobre POLY ej: PlyRes → **{−1,1,2}** | ídem |
| STAT 1-VAR (x̄, sx, σx, Σx, Σx², minX, maxX, Q1, Q3, Med, Mod, n) | STAT app → 1-VAR → results / VARS | P39, P212-216 | C dataset ancla {1,2,2,3}: n=4, x̄=2, σx=**0.7071067812**, sx=**0.8164965809**, Σx=8, Σx²=18, minX=1, maxX=3, Med=2, Q1=1.5, Q3=2.5, Mod=2 | app + rejilla (item 30 BLOQUEADA); Q1/Q3 dependen de SETUP Q1Q3 |
| STAT 2-VAR / regresión (a,b,c,r,r²,Σy,Σy²,Σxy,sy,minY,maxY) | STAT app → REG | P39, P221-229 | C {(1,2),(2,4),(3,6)} ax+b: a=**2**, b=**0**, r=**1**, r²=**1** | app + rejilla |
| Tipos de regresión ({ax+b}…{Logistic}) | STAT F-tab | P221-223 | sin ancla propia (menú de tipos) | app |
| TEST/INTR (z, t, p, df, Lower, Upper) | STAT F3(TEST) F1(Z) F1(1-SAMPLE) | P240, P40 | sin ancla aún (construir 1-Sample z con dataset ancla) | app + H |
| t( | OPTN F6 F3(PROB) F6 F4(t( ) | P233-235 | V: **t(160.5)=−1.633855948**, **t(175.5)=0.4963343361** | requiere datos 1-VAR cargados (app) |
| FINANCIAL: pantallas SIMPLE/COMPND/AMORTZN/CASH/CONVERT/COST-SELL-MARGIN/DAYS | MENU FINANCIAL → F-tab | P278-291 | aritmética ya cubierta por las funciones OPTN de A†/B (fi01-08 acreditadas + candidatos); la pantalla solo añade navegación | app (7 pantallas consolidadas) |
| DEPREC (SL/FP/SYD/DB) | FINANCIAL → DEPREC | P294 | C: SL (5000,300,6) → **783.3333333**/año; SYD año1 → **1342.857143** | app; sin sintaxis OPTN equivalente |
| BOND (PRC/YLD) | FINANCIAL → BOND | P296, P348 | Bond_PRC devuelve lista **{PRC,INT,CST}** — sin ancla | app + llaves |
| CONICS parábola (FOCUS/VERTEX/LENGTH/DIRECTX/SYMMETRY) | MENU CONICS → tipo → DRAW → G-SOLVE | P204-205 | C: X=(Y−2)²+3 → foco **(3.25, 2)**, LENGTH (latus rectum 4p) = **1** | app + rejilla de selección de tipo |
| CONICS círculo (CENTER/RADIUS/X-ICEPT/Y-ICEPT) | ídem | P205 | C: (X+2)²+(Y+1)²=4 → centro **(−2,−1)**, radio **2** | ídem |
| CONICS elipse/hipérbola (e, ASYMPT) | ídem | P204 | sin ancla | app |
| TABLE (Start/End/Pitch/Result) | MENU TABLE / VARS | P39-41 | C: Y1=X², Start=1, End=3, Pitch=1 → **{1,4,9}** | app (prohibida #12) |
| RECURSION (FORMULA/RANGE/Result/aₙ) | MENU RECURSION / VARS | P41, P187 | sin ancla | app |
| Spreadsheet (SEQ/FILL/SORTASC/SORTDES/CELL/GRAB) | MENU SS | P367-368 | C: SORTASC {3,1,2} → **{1,2,3}** | app + rejilla |

## 5. Categoría D — rejillas/plantillas (matcher multi-fila o dimensión)

| ítem (censo) | ruta | pág. | oráculo | riesgo |
|---|---|---|---|---|
| DIM (pantalla de dimensión) | plantilla → intro 2,3 | P92, P100 (ej 2: `!*( ) c,d!/( ) a`) | V ej: rejilla 2×3 → Dim(Mat B) → **2×3** | rejilla; matcher multi-fila BLOQUEADO (item 28) |
| ROW-OP (SWAP/Row/Row+) | OPTN F2 F6? (ROW-OP, H) | P93-94 | C: Swap filas 1↔2 de [[1,2][3,4]] → **[[3,4][1,2]]** | H slots; rejilla |
| Plantilla m×n | SHIFT+× → F-tab 2×2/3×3/m×n | P25 | entrada 2×2 ya verificada (mt01); m×n por verificar | rejilla |
| Editor de listas STAT | MENU STAT | P206 | ancla de rejilla que alimenta todo el grupo LIST de B | app + rejilla |
| Spreadsheet celdas | MENU SS | P367-368 | rejilla de celdas | app + rejilla |

## 6. Categoría E — no certificables por determinismo / ausentes / sistema

| ítem (censo) | pág. | motivo |
|---|---|---|
| RAND / Ran# / RanList# / Samp | P63, P67-70 | aleatorios: sin oráculo determinista (Ran# n siembra secuencias pero el manual no imprime valores) |
| fn (FUNCMEM) | P58 | ausente en guest (Math I/O / add-in); Str requiere modo Line → incompatible |
| PICTURE / CAPTURE | P38 | ausentes (add-in) |
| CONVERT métrico | P38 | NO_APLICABLE al guest (item 29d, sonda v19) |
| RECV/TRANSMIT/CABLE/WAKEUP; MEMORY/Catalog/SEARCH | P446, P456, P404-405, P299 | sistema/hardware, fuera de alcance aritmético |
| UI-chrome cat-0 (ENG, DISPLAY, etiquetas F-tab, colores) | — | reclasificación propuesta: los toggles SETUP (Fix/Sci/Norm/Eng/Deg/Rad/Gra/Real/a+bi/Math/Line/ab·c/d·c) son **compuertas de estado** prerrequisito de oráculos A/B (Rnd-Fix3, ENG-SYM-Eng, BASE-Mode, tg-RAD) |

---

## 7. Recuento por categoría

| Cat | Filas | Definición |
|---|---|---|
| A | **22** | RUN-MAT corto, sin glifos nuevos (incluye 10 † fuera de censo que cubren ítems de app) |
| B | **33** | RUN-MAT con glifo nuevo (∠, °´˝, µ, Σ-eco, llaves) o dependencia de lista/matriz multilínea |
| C | **19** (consolidadas) | navegación de app — EN ESPERA por política OLA4 #12 |
| D | **5** | rejillas/plantillas (matcher multi-fila) |
| E | **6** | no certificables por determinismo / ausentes / sistema |
| **Total** | **85 filas** | ~62 ítems de censo mapeados + 10 insumos † + consolidaciones |

## 8. TOP-10 recomendado para OLA 5 (todo RUN-MAT, oráculo cerrado, sin apps ni llaves)

1. **Cost(125,20)=100** — fórmula verbatim P290; ruta OPTN FINANCE; cierra el trío costo/venta/margen con Sell ya acreditado.
2. **Margin(100,125)=20** — ídem, simétrico de Cost; dos PASO con una sola ruta nueva.
3. **Cmpd_FV(2,10,−1000,0,1,1)=1210** — interés compuesto básico; oráculo cerrado verificado a mano (1000×1.1²).
4. **Cmpd_PMT(12,0,−1200,0,12,12)=100** — cuota con I%=0: aritmética trivial y robusta; completa la familia CMPD.
5. **Amt_PRN(1,1,0,−1000,100,12,12)=100** — amortización con I%=0: PRN=cuota exacta; abre la familia AMORT.
6. **Amt_BAL(1,1,0,−1000,100,12,12)=−900** — saldo tras 1 cuota; mismo input que Amt_PRN (un tecleo, dos vectores).
7. **Amt_INT(1,1,0,−1000,100,12,12)=0** — cero interés: discrimina que la ruta lee el campo correcto (no confunde INT/PRN).
8. **Conjg(2+4i)=2−4i** — verbatim del manual P85; cero glifos nuevos (i ya calibrada); completa la pág 1 de COMPLEX.
9. **Q(0)=0.3989422804** — construido cerrado; hermano de P/R acreditadas (st01-03, misma ruta OPTN F6 F3 F6) — solo falta confirmar slot F2.
10. **'Bin + 'Oct (22 → 10110 / 26)** — ejemplo verbatim P90; abre el dominio BASE-N (item 33 NO_INVENTARIADO) con una sola navegación SETUP→Dec.

Mención: **FMin** (ejemplo verbatim P83) y **Seq** (keystone que desbloquea el grupo LIST sin llaves de entrada) van
en la ola siguiente si el frame-dump del submenú FINANCE y la calibración de llaves (v23) avanzan.

---

## 9. Plan de ejecución post-corrida-completa v25 (orden mecánico)

Estado (2026-11-13 23:20): v25c (full-run frío, PID 40560, arrancó 2026-11-13 23:08:54, ETA ~13:00 del 14) en vuelo; v29 compilada (exit 0; commit `5d0ca5a`) con sonda de 17 casos (8 diags v28 + eqn-dA/dB + fin-dG/dH + q01/q02 + siml01/02/03) AUTO-AGENDADA tras v25c (waiter background, warm canónico v22, SHA `8AD6BD2E…B73B10`).

### 9.1 FINANCE (frame-dump primero, todo RUN-MAT)
Estado (2026-11-13): fin-dA..dF ATERRIZADOS (v27eqn): el submenú FINANCE = SIMPLE|COMPND|CASH|AMORTZN|CONVERT|▶ (pág. 2: TAX|COST|DAYS|BOND); los picks SOLO conmutan F-tabs de slots de variable sin insertar texto; fin-dG (F1 tras pick COMPND) y fin-dH (salto por letra kLn al ícono C) AGENDADOS en la sonda v29; el ensamblado real de la llamada (Smpl/Cmpd desde RUN-MAT) queda para v30 con frames.

Diagnóstico en dos etapas (vectores `volcar=true`, frames deciden; el slot F2 de
FINANCE ya verbatim [P297]: Math I/O → `OPTN F6 F6 F2`):

1. **fin-dA** `{kOptn, kF6, kF6, kF2}` → ¿qué abre F2? (menú numerado SIMPLE/CMPD/
   CASH/AMORT/CONVT/COST/DAYS/DEPRC/BOND, o fila de F). Dump de cada paso.
2. **fin-dB** `{kOptn, kF6, kF6, kF2, kDigDos}` → el formulario CMPD: campo inicial,
   orden de campos (n, I%, PV, PMT, P/Y, C/Y), etiquetas de F1..F6 (el SOLVE del
   objetivo). Frames deciden cómo navegar campos (flechas vs EXE).
3. Con la ruta confirmada: vectores TOP-10 (Cost 100, Margin 20, Cmpd_FV 1210,
   Cmpd_PMT 100, Amt_PRN 100, Amt_BAL −900, Amt_INT 0) + sonda + recibo + commit.
4. Q( (slot F2 de PROB, oráculo 0.3989422804) y Conjg(2+4i) (regresión de la ruta
   cplx05) caben en la misma sonda si no estorban.

Riesgo declarado: formularios FINANCE son plantillas multi-campo (categoría
«rejilla» del censo) — el frame-dump es OBLIGATORIO antes de codificar vectores.

### 9.2 EQUATION (ícono 10 — kMenu,kAba,kDer×4,kExe; ancla SIML calibra '=','X','Y')
Estado (2026-11-13): siml-a/eqn01-05 ATERRIZADOS (v27eqn): EQUATION abre directo en el prompt SIML; kF2 NO es POLY; ruta cursor frágil (eqn04 abrió Distribution) → ruta LETRA kMenu,kXVar; eqn-dA/dB (EXIT del prompt → ¿menú de modos con F1=SIML/F2=POLY/F3=SOLVER?) AGENDADOS en la sonda v29; POLY real + eqn05 complejo en v30; matcher parBandas v29 = últimas k bandas libres de '?'.

1. Diagnóstico eqn-dA: apertura del app con volcar (kMenu → navegación → kExe) +
   SIML 2×2 (`X+Y=5, X−Y=1`) → la pantalla de soluciones X=3, Y=2 calibra los
   glifos '=' / 'X' / 'Y' y documenta el formato de la tabla (¿bandas [X=3][Y=2]?).
2. Extensión del matcher si hace falta: «último PAR de bandas» para tablas de
   soluciones (análogo del par [num][den]; OLA4 §7).
3. EQN-01..05 (oráculos L131-136 del censo: cuadrática x²−7x+12 → 4,3; cúbica →
   3,2,1; 2×2 → X=2,Y=3; 3×3 → 1,2,3; compleja x²+4x+13 → −2±3i) + sonda + commit.

### 9.3 STAT (tecla `2` abre el editor de listas) → STAT-04/05; MAT rejillas AL FINAL
Estado (2026-11-13): stat-dA ATERRIZADA (v27d; List 1 persiste entre reps → limpiar la lista para los reales), stat-dB agendado en v29; mat-dA/mat-b/dim-dA agendados en v29; STAT/MAT reales tras frames (v30).
(multi-banda, lo más delicado); OLA5 §A restantes (Rnd con SETUP Fix 3, 'Bin/'Oct
con SETUP Mode-Dec — cambiar SETUP global exige save/restore, ya verificado).

Regla de oro de cada lote: vector + mini-sonda (warm para depurar, y fría si toca
despliegue nuevo) + recibo archivado + commit antes de pasar al siguiente dominio.


---

## 10. CIERRE v39 (2026-11-21): LOGIC sin pagina 2 y ruta real a Base-N por SETUP->Mode

Esta seccion CORRIGE y CIERRA dos puntos de arriba. Manda sobre ellos.

### 10.1 Punto 5 (LOGIC) - CERRADO: el submenu LOGIC tiene UNA sola pagina
Medido con frames reales (sonda v39, caso bas-f1; recibos\v39-microsonda_base_result.txt):

- OPTN pag1 = LIST | MAT | CPLX | CALC | STAT | >
- OPTN pag2 = HYPERBL | PROB | NUMERIC | ANGLE | >
- OPTN pag3 = ENG-SYM | FINANCE | LOGIC | >
- kF4 (pag3) abre LOGIC = **And | Or | Not | Xor** (kF1..kF4)
- kF6 y kF5 dentro de LOGIC **no paginan**: la pantalla queda identica (p6..p8).

Por tanto `'Dec | 'Hex | 'Bin | 'Oct` **NO existen** en OPTN->LOGIC en RUN-MAT.
Toda la via LOGIC usada en v34..v38 queda descartada como ruta a Base-N.
(El orden Neg=F1..xnor=F6 que cita el manual [P89] corresponde al modo BASE,
no al LOGIC de RUN-MAT en modo Comp; sigue sin verificar en el guest.)

### 10.2 Filas 'Bin / 'Oct / 'Hex / 'Dec de la seccion 2 - RUTA CORREGIDA
La ruta real no es "SETUP Mode->Dec + DISPLAY Fx": el selector de base vive
en la **fila 2 del SETUP ("Mode")** y su strip es

    [Comp] [Dec] [Hex] [Bin] [Oct]  =  kF1 .. kF5

Evidencia (sonda v39, caso bas-f2, frames p4..p15):

- SHIFT+MENU abre SETUP con el cursor en la fila 1 ("Input/Output : Mth/Mix")
  y **sin** SHIFT pegado (el indicador S no aparece).
- kAba #1 solo limpia un SHIFT pegado si lo hubiera; con SHIFT limpio, **un
  solo kAba baja a "Mode"** (fila 2).
- kAba #2.. = Frac Result (3), Func Type (4), Draw Type (5), Derivative (6),
  Angle (7), Complex Mode (8), Coord (9), Grid (10)...
- kExit devuelve a RUN-MAT con el historial intacto.

Secuencia canonica para los cuatro items de base:

    kMenu, kDigUno, <digitos>, kExe, kShift, kMenu, kAba, kF4|kF5, kExit

    kF4 = Bin    kF5 = Oct    (kF2 = Dec, kF3 = Hex, kF1 = Comp)

Queda por confirmar con frames de v40 si el cambio de Mode **convierte el
valor ya mostrado** y con que formato lo pinta (prefijo/sufijo de base).

### 10.3 Correccion de calibracion: setup-dD (v32) estaba mal navegado
setup-dD pretendia llegar a "Display" con kAba x7 desde la fila 1. Con el
orden medido, 7 bajadas desde la fila 1 caen en **Complex Mode (fila 8)**, no
en Display (fila 13). Por eso devolvio '1/3' en vez de '0.333'. No se toca en
este lote: queda anotado para el frente de SETUP (harían falta ~12 bajadas,
o mejor, salto por letra si existe).
