<!-- UI_DESIGN_IPADOS_HIG_POLICY_BEGIN -->
# Reglas Permanentes de Diseño UI/UX (guías de interfaz + material translúcido + Cero Estética IA)

Al diseñar, rediseñar o auditar cualquier interfaz en este espacio de trabajo (**Calculadora Unificada** y **Document Studio**), priorizar las **guías de diseño de interfaces** (``) y **material translúcido** como referencia de calidad, sin copiar ciegamente macOS ni sacrificar convenciones útiles de Windows.

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
