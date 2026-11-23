# Calculadoras Unificadas — Monorepo Multiplataforma

Monorepo unificado fuera de OneDrive con una sola copia de cada núcleo C++ y una sola carpeta de activos compartidos.

## Estructura
- `core/casio_nx/`: núcleo `nX-U8`/`nX-U16` (`fx-570LA X`, `fx-570VN PLUS`).
- `core/casio_cw2/`: núcleo headless `nX-U16/100` 4 niveles de gris (`fx-991CW`, GPL-3.0).
- `core/casio_cg50/`: núcleo nativo `SH7305` + `R61524` (`fx-CG50 PRIZM`).
- `core/casio_ffi/`: capa ABI C pura compartida (`libcasio_native.so` / `casio_native.dll`).
- `assets/`: carátulas (`skins/`), fuentes (`fonts/`), catálogo (`catalog/`), manuales (`manuals/`) y `roms/` (ignorado por Git).
- `apps/android/`: aplicación Flutter + Android NDK (Prioridad #1).
- `tests/core/`: pruebas nativas C++ en host y barrera C++20.
- `tools/`: sincronización verificada de activos (`sync_shared_assets.ps1`) y empaquetado local (`publish_portable.ps1`).

## Reglas de Diseño
Consultar `AGENTS.md` (guías de interfaz + material translúcido funcional + 18 anti-patrones prohibidos).
