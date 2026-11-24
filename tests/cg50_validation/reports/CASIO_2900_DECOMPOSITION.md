# Descomposición Honesta del Inventario fx-CG50 y la Cifra Comercial «2900+ Funciones»

Generado automáticamente por `build_feature_inventory.py` sobre los manuales oficiales completos presentes en `assets/manuals/` (`fx-CG50_Soft_v370_EN.pdf` — 677 págs, `fx-CG50_Soft_v370_ES.pdf` — 678 págs, `fx-CG50_Hard_EN.pdf` — 17 págs, `fx-CG_Manager_PLUS_Software_Users_Guide_EN.pdf` — 45 págs) y contrastado contra cadenas en `SystemROM.raw` + `UserROM.raw`.

## 1. Denominador Real Extraído (Sin Duplicación Artificial)

| Fuente / Estrato | Entradas Únicas Extraídas |
|---|---:|
| Pilares funcionales base (42 áreas del manual + add-ins) | 49 |
| Comandos de `Ch. 8 §7 Program Mode Command List` (pp. 348–355) | 531 |
| Tokens adicionales de `Ch. 8 §8 Special Commands Text Table` (pp. 356–362) | 235 |
| APIs documentadas de `Ch. 17 MicroPython / casioplot` (pp. 560–591) | 76 |
| Condiciones de error de `Appendix 1 Error Message Table` (pp. 603–615) | 31 |
| **Total único en `CG50_FEATURES_MASTER.json`** | **922** |
| Símbolos con coincidencia directa en `SystemROM.raw`/`UserROM.raw` | 682 |
| Meta de casos de prueba (niveles 1–10) | 12779 |

## 2. Cómo Casio Alcanza la Cifra Comercial «2900+ / 3000+ Funciones»

Casio no publica una lista numerada del 1 al 2900. Al auditar las 677 páginas del manual de software se observa que la cifra comercial incluye la suma multiplicativa de:
1. **Comandos matemáticos y de programación únicos** (~450–650 tokens canónicos en Run-Matrix, OPTN, VARS, PRGM, CATALOG).
2. **Conversiones métricas y constantes físicas** (`CONVERT` y `Physium.g3a`: 118 elementos de la tabla periódica + 40 constantes fundamentales CODATA + unidades de longitud/área/volumen/tiempo/masa/presión/energía/potencia/temperatura).
3. **Permutaciones de modalidad y menú por aplicación** (por ejemplo, 10 tipos de gráfico × 5 estilos × colores × G-Solv; 12 modelos de regresión en STAT + Spreadsheet + Picture Plot; 8 distribuciones × PD/CD/Inv × gráficas; 10 tipos de cálculo financiero × variables; 40+ construcciones/mediciones en Geometry; 6 modos en Prob Sim; 100+ funciones/métodos en MicroPython).
4. **Opciones de interfaz, formato y configuración (`SETUP`, `eActivity`, `Memory`, `System`, `Link`, `Exam Mode`, `E-CON4`)** — más de 168 ítems de menú UI censados ya en `CENSO_RESUMEN.md`.

## 3. Regla Estricta de Contabilización para Certificación
- **Ninguna entrada** se marca `PASS` por el solo hecho de abrir una aplicación o haber aparecido en `cert2900 v2`.
- Los `133` casos deterministas de `cg50_battery.cpp` se vinculan únicamente a sus pilares correspondientes sin marcar como certificada toda el área mientras `test_cases_passed < test_cases_target`.
- Todas las entradas fijan `host_math_fallback_allowed: false` y mantienen `physical_reference_verified: false` mientras no exista una unidad física medida.
