# Reporte de Cobertura Verificada — TEC Digital 2026

> **Regla de honestidad:** Ningún porcentaje se reporta sin su denominador exacto, y jamás se declara un "100% global de TEC Digital" basado en una cuenta estudiantil. Se distinguen explícitamente: superficie estudiantil accesible, superficie pública, referencias en bundles JS, mutaciones bloqueadas y superficie servidor/admin no observable.

## 1. Métricas Separadas con Denominador Explícito

| Métrica | Numerador / Denominador | Porcentaje | Notas |
|---|---:|---:|---|
| **Exploración de espacios accesibles** | `34 / 34` | **100.0%** | Todos los espacios del árbol autenticado (clases actuales e históricas, cátedras, escuelas, DOP, programas) |
| **Exploración de módulos catalogados** | `31 / 32` | **96.9%** | Módulos estudiantiles y públicos observables |
| **Documentación individual de módulos** | `32 / 32` | **100.0%** | Cada módulo tiene entrada en `TECDIGITAL_MODULE_REGISTRY.json` |
| **Documentación individual de contratos API (31 campos)** | `260 / 260` | **100.0%** | `212` HTTP/AJAX + `48` GraphQL |
| **APIs verificadas en vivo / sincronización read-only** | `58 / 188` (sobre APIs no-escritura) | **30.9%** | El resto corresponde a endpoints de rol docente/admin o condicionales referenciados en JS |
| **APIs de escritura bloqueadas en política read-only** | `72 / 72` | **100.0%** | 0 mutaciones ejecutadas contra producción (`MUTATION_MASK=0`) |
| **Reconstrucción funcional local de referencia** | `32 / 32` | **100.0%** | Cubre los 32 módulos con fixtures sanitizados en `reference_reconstruction/` |
| **Integración nativa en CuadernU (módulos completos)** | `28 / 32` completos (`+4` parciales) | **87.5%** completos / **100.0%** cubiertos (completos+parciales) | Sin WebView; 0% GPU GDI+ guías de interfaz |
| **APIs integradas en CuadernU (sobre total 260 incl. admin/writes)** | `75 / 260` | **28.8%** | Excluye intencionalmente mutaciones bloqueadas y queries exclusivamente administrativas |

## 2. Separación de Superficies (Sección 15)

1. **Superficie estudiantil accesible (observada en vivo):** `500` páginas concretas (`235` patrones únicos) a través de `34` espacios académicos y `32` módulos.
2. **Superficie pública no autenticada:** `https://tec-appsext.itcr.ac.cr/guiahorarios/` (`escuela.aspx`, `verano.aspx`, `estudiante.aspx` + 90 escuelas / 4,635 grupos).
3. **Superficie referenciada en JS pero no ejecutada:** `72` contratos de escritura (documentados desde el código cliente, probados solo vía loopback local) y queries de coordinación/administración dentro del bundle de `tda-inclusion-manager`.
4. **Superficie no observable del servidor:** Lógica Tcl/PL-SQL interna de OpenACS, vistas exclusivas de profesor/administrador, y microservicio externo `api.tec.ac.cr:3003` (retorna HTTP 401 sin token SPA dedicado).
