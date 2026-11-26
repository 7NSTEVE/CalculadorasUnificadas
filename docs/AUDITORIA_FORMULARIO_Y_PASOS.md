# Auditoría del tutor: formulario II-2026 y pasos completos

Fecha: 25 de noviembre de 2026 · Rama `feat/tutor-audiovisual` (sin subir, `main` sin tocar).

## Reglas que se auditaron

1. **Todo parte de la hoja** `Ecuaciones_Tercer_Parcial_FG3.pdf` (42 casillas). Una ecuación que no está
   en la hoja solo se usa si antes se deriva desde las de la hoja (igualar, despejar, integrar).
2. **Ningún paso saltado**: en cada cálculo se escribe la ecuación de la hoja, el despeje, la sustitución
   con números, la cuenta intermedia y el resultado. Las soluciones oficiales solo sirven para comparar
   el número final.
3. **Alcance del III parcial: hasta la sección 20.5** (segunda ley). Carnot (20.6) y entropía (20.7) no entran.

## Cómo se ve en la pizarra

- Cuando el profesor toma una ecuación de la hoja aparece el **recorte real de esa casilla**, encerrado en
  un círculo del mismo color y número que en la pestaña *Fórmulas*. Tocarlo abre la hoja en esa casilla.
- Los renglones que salen de ella (igualar, despejar, integrar, sustituir, resultado) llevan **una barra
  de ese color**; si combinan varias casillas, llevan sus números al margen.
- El color sigue a la cadena aunque continúe en el paso siguiente, hasta que entra otra ecuación de la hoja.

## Resultado

| | Lecciones | Estado |
|---|---:|---|
| Desarrollo que entra al parcial | 87 | Sin saltos detectables. Quedan 5 marcas del detector que son conclusiones sin cuenta (Q029 a, Q076 C-D, Q100 C) o incisos fuera del parcial (E17 c, Q115 b) |
| Selección única que entra | 61 | 26 con cuentas completas. 35 son conceptuales (ver abajo) |
| Teoría | 8 | Reescrita antes de esta auditoría, con ejemplo y pregunta rápida |
| Fuera del parcial | 11 | Q118, Q125 (Carnot) · Q130–Q137 y T-ENT (entropía). Marcadas en la biblioteca |

Comparación con las soluciones publicadas: **120 coinciden**, **6 difieren** porque la solución oficial
tiene un error que la lección explica (Q045, Q050, Q060, Q061, Q099, Q116) y 40 no tienen número publicado.

### Lecciones reescritas en esta pasada

- **Ciclos del folleto Q112–Q116**: antes todo iba en una tabla. Ahora cada estado se calcula con
  `pV = nRT` o con la adiabática de la hoja, cada proceso tiene su paso y el cierre del ciclo suma
  trabajos y calores con la revisión de la primera ley.
- **Ciclos de examen Q092, Q100, Q109, Q122, Q128**: mismo tratamiento. Q122 muestra también la ruta
  del solucionario (con presiones) como comprobación.
- **Conducción Q037, Q041, Q043, Q044, Q052, Q053, Q055, Q058**: cambio de variable, integrales y cuentas
  intermedias escritas renglón por renglón desde `H = −kA dT/dx`.
- **Selección con cuentas Q066, Q068, Q102**, y detalles en Q012, Q029, Q038, E17.
- Carnot y entropía, donde un problema los pide, se derivan desde la hoja en un paso aparte
  (`e = 1 − |Q_C/Q_H|`, isotermas, `TV^(γ−1) = cte`; `dS = dQ/T` con el `dQ` de la hoja) y se marcan
  como fuera del parcial.

### Selección conceptual (35)

Q007, Q027, Q040, Q062, Q065, Q067, Q071–Q073, Q075, Q077–Q079, Q081–Q086, Q088–Q091, Q093, Q095,
Q101, Q104, Q117, Q119, Q121, Q123, Q124, Q126, Q127, Q129.

Se resuelven razonando con la ley que corresponde y descartando cada opción con su motivo. No tienen
cuentas largas, por eso el detector las marca como "sin desarrollo". Pendiente: revisarlas una por una
y agregar la ecuación de la hoja donde ayude (por ejemplo Q078 se puede derivar de `K = (3/2)nRT` y
`pV = nRT`).

## Problemas de los 21 PDF

| Fuente | Problemas | Abren lección verificada |
|---|---:|---:|
| Superexamen | 137 | 137 |
| Ejercicios Recomendados 3 | 21 | 21 |
| III Parciales 2023–2026 (ordinarios y extraordinarios) | 92 | 92 |
| Reposiciones 2023–2026 | 22 | 8 |
| Folleto de Física III | 97 | 37 |
| **Total** | **369** | **295** |

Los 74 que no abren lección verificada son **todos de otros parciales** (oscilaciones, ondas y fluidos:
folleto 1.x y 2.x y parte de las reposiciones). Esos los resuelve la IA a partir del recorte del
problema, con la misma regla de cinco renglones; sus números no están verificados por código.

## Cómo repetir la auditoría

```
D:\DevTools\Python311\python.exe tools/tutor_runtime/auditoria_pasos.py
```

Es heurística: marca candidatos para revisar a ojo, no prueba que una lección esté bien. Las pruebas
que sí bloquean (`tests/tutor_runtime`) revisan aritmética, casillas de la hoja y atajos sin derivar.

## Lo que no se verificó

- No se midió la sincronía voz-trazo en las lecciones nuevas, que son más largas.
- Las capturas se revisaron en una muestra (E16, Q043, Q112), no en las 166 lecciones.
- No se recompiló CuadernU (C++); los cambios son del tutor en Python y JavaScript.
