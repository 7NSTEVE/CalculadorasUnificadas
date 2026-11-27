<!-- UI_DESIGN_IPADOS_HIG_POLICY_BEGIN -->
# Reglas Permanentes de Diseño UI/UX (guías de interfaz + material translúcido + Cero Estética IA)

Al diseñar, rediseñar o auditar cualquier interfaz en este espacio de trabajo (**Calculadora Unificada** y **Document Studio / CuadernU**), priorizar las **guías de diseño de interfaces** (``) y **material translúcido** como referencia de calidad, sin copiar ciegamente macOS ni sacrificar convenciones útiles de Windows.

## Filosofía Visual
- **Minimalismo real**: no significa eliminar funciones; significa eliminar la complejidad innecesaria de su presentación (divulgación progresiva y menús contextuales).
- **Cuatro pilares**: precisión, simplicidad, elegancia y profesionalismo.
- **Funcionalidad primero**: no confundir una interfaz bonita con una interfaz funcional; cada componente debe justificar su presencia y cada herramienta debe ser fácil de encontrar aunque no permanezca visible todo el tiempo.
- **Diseño profesional de producto**: prohibido el aspecto de consola de depuración interna o dashboard oscuro genérico ensamblado por IA.

## 18 Anti-patrones de Estética IA (Prohibidos)
1. **Emojis en lugar de iconos SVG**: cero emojis decorativos; usar solo iconografía vectorial consistente.
2. **Exceso de texto y jerga interna**: evitar títulos redundantes, subtítulos vacíos y etiquetas técnicas internas (`"LaTeX"`, `"Vista LaTeX"`, `"4 calculadoras integradas"`, `"Registro visual"`, `"Core oficial"`, `"Guías para alumnos"` → usar término neutro `"Guía"`).
3. **Exceso de botones**: no mostrar diez controles visibles cuando bastan dos o tres acciones principales y un menú contextual.
4. **Oscuridad innecesaria**: preferir diseños claros luminosos y neutros por defecto (ej. *Porcelana Claro*), manteniendo un modo oscuro y alto contraste bien calibrados.
5. **Estética gamer o neón**: prohibidos colores fluorescentes, resplandores azules, bordes luminosos y contrastes exagerados.
6. **Tarjetas dentro de tarjetas**: no encerrar todo en rectángulos redondeados anidados con bordes, sombras y fondos compitiendo.
7. **Abuso de material translúcido**: usar transparencias, desenfoques y degradados únicamente como materiales funcionales con propósito jerárquico, nunca como decoración gratuita.
8. **Jerarquía visual deficiente**: distinguir claramente principal, secundario y terciario; las calculadoras o el lienzo son los protagonistas absolutos.
9. **Espaciados inconsistentes**: mantener grilla de espaciado coherente, márgenes simétricos, alineación real y tamaños consistentes.
10. **Botones píldora para todo**: reservar cápsulas cerradas solo para interruptores o indicadores puntuales y usar radios moderados por jerarquía.
11. **Barras laterales innecesarias**: hacerlas compactas u ocultables bajo demanda para no consumir espacio valioso.
12. **Interfaces tipo dashboard**: no llenar la pantalla de estadísticas, métricas, tarjetas y paneles innecesarios.
13. **Decoración sin función**: eliminar sombras exageradas, iconos gigantes, gradientes de fondo y badges inútiles.
14. **Falta de identidad**: evitar plantillas genéricas SaaS.
15. **Inconsistencia entre pantallas**: todas las ventanas y diálogos (principal, configuración, exportador, ayuda, estudio, historial) deben compartir paleta, tipografía, espaciado, botones y tema sincronizado.
16. **Falsa sofisticación**: lograr aspecto premium por proporciones, materiales, tipografía y composición limpia, no por efectos recargados.
17. **Desaprovechamiento del espacio**: evitar grandes vacíos con controles diminutos; garantizar escalado real 4K (`PerMonitorV2`).
18. **Controles duplicados**: evitar repetir la misma función en barra superior, lateral e inferior sin razón clara.

## Tipografía Matemática y Científica
- Renderizar visualmente en notación matemática natural 2D todas las ecuaciones, fórmulas, unidades físicas y letras griegas, sin LaTeX crudo, sin texto plano monoespaciado y sin textos meta anunciando que está renderizado en LaTeX.
<!-- UI_DESIGN_IPADOS_HIG_POLICY_END -->

<!-- WORKSPACE_HYGIENE_CONTRACT_BEGIN -->
# Reglas Permanentes de Orden, Clasificación y Anti-Despiche

Estas 8 reglas son **OBLIGATORIAS** para todo agente IA (Antigravity, Codex, Claude, Copilot, OpenCode) y todo desarrollador que toque este proyecto:

1. **Raíz Única Obligatoria:** TODO lo relacionado con Calculadoras Unificadas, CuadernU y Document Studio vive exclusivamente en `D:\Proyectos\CalculadorasUnificadas\`. **PROHIBIDO** crear clones, copias, carpetas paralelas, backups manuales ni worktrees en `OneDrive\Desktop`, `E:\Descompilados`, `D:\Temp` o `D:\A- Proyectos Antigravity Codex`.
2. **Lista Blanca Estricta en la Raíz (Cero Archivos Sueltos):** En la raíz de `D:\Proyectos\CalculadorasUnificadas\` solo se permiten:
   - Archivos de manifiesto: `README.md`, `AGENTS.md`, `LICENSE`, `THIRD_PARTY_NOTICES.md`, `.gitignore`, `.gitattributes`, `.editorconfig`, `CMakeLists.txt`.
   - Directorios de primer nivel aprobados: `.git/`, `.github/`, `.githooks/`, `core/`, `studio/`, `academic/`, `ui_glass/`, `apps/`, `assets/`, `docs/`, `tests/`, `tools/`, `private_local/` (ignorado por Git), `build/` (ignorado por Git), `Fisica_III/` (material del curso de Física III, en la raíz por decisión del dueño e ignorado por Git: sus PDF nunca se suben al repo público).
   - **PROHIBIDO** dejar archivos `.obj`, `.png`, `.bmp`, `.log`, `.txt`, `.ps1`, `.py`, `.bak*` o reportes sueltos en la raíz.
3. **Fuente Única de Motores (Cero Copias Vendored):** Existe **UNA SOLA** copia de cada motor C++ en `core/` (`casio_cg50`, `casio_cw2`, `casio_nx`, `casio_ffi`) y `studio/` (`tinta_core`, `core/`, `interop/`, `pdf/`, `math/`, `circuit/`). Todas las plataformas en `apps/` (`windows`, `android`, `ios_native`, `linux`) enlazan contra esas rutas. Prohibido duplicar código fuente de motores dentro de `apps/`.
4. **Cero Archivos `.bak` ni Copias con Fecha:** Todo historial y respaldo vive en Git. Prohibido crear `*.bak*`, `*_old*`, `*_copy*` o copias numeradas de fuentes.
5. **Una Sola Compilación por Programa, Exclusivamente en `build/`:** Todos los builds (`cmake`, `ninja`, `msbuild`, `flutter`), trazas, capturas de pruebas y logs van bajo `build/<preset>/` (ignorado por Git). CuadernU se compila **solo** en `build/studio-x64`. Prohibido crear carpetas de compilación nuevas o paralelas (por ejemplo `build/studio-x64-b1/`, `build-verify/`, `build-native-x64/`, `build-win32/`, `out_test/`) o volcar corridas en `D:\Temp\`. Lo temporal de una tarea se borra al terminarla.
6. **Separación Clara de Activos Locales Pesados (`private_local/`):** Instaladores originales (`instaladores_casio/`), runtime DLLs oficiales de Casio (`official_casio_runtime/`), manuales PDF (`manuales/`), tipografía personal (`tipografia_personal/`) e investigación histórica (`investigacion/`) viven exclusivamente en `private_local/` (ignorado por Git y verificado con `git check-ignore`). Las ROMs de emulación permanecen en `assets/roms/` según política del dueño.
7. **Una Sola Rama: `main` (decisión del dueño, 27-nov-2026):** Prohibido crear ramas, worktrees o copias del árbol. Todo el trabajo va directo en `main`, en commits pequeños que solo incluyen los archivos propios: nada de `git add -A` ni `git commit -a` si hay trabajo ajeno en el árbol. `main` debe seguir compilando y con las pruebas en verde; si un commit deja algo en rojo, se dice en el mensaje y se arregla en el siguiente. El control de higiene rechaza commits si existe otra rama local.
8. **Puerta de Cierre Obligatoria:** Ninguna tarea puede declararse completada sin ejecutar `D:\DevTools\Python311\python.exe tools/check_workspace_hygiene.py` con código de salida `0`.
<!-- WORKSPACE_HYGIENE_CONTRACT_END -->
