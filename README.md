# Calculadoras Unificadas + CuadernU — Monorepo Canónico Multiplataforma

Raíz única oficial del proyecto fuera de OneDrive (`D:\Proyectos\CalculadorasUnificadas\`), con una sola copia de cada motor C++17 y soporte multiplataforma para **Windows x64**, **Android 16**, **iOS / iPadOS** y **Linux**.

## Arquitectura del Monorepo

- `core/` — **Motores nativos C++17 `v47`** (64-bit, cero DLLs de Casio):
  - `core/casio_cg50/`: `fx-CG50 PRIZM` (`SH7305 FULL_SILICON` + `R61524` + `USB2` + `Fugue FTL` + `SMEM` + `QR`)
  - `core/casio_cw2/`: `fx-991CW ClassWiz II` (`nX-U16/100`, 4 niveles de gris, headless)
  - `core/casio_nx/`: `fx-570LA X` y `fx-570VN PLUS` (`nX-U8`/`nX-U16`)
  - `core/casio_ffi/`: ABI C pura compartida (`casio_native.dll` / `libcasio_native.so` / `CasioNativeCore.xcframework`)
- `studio/` — **CuadernU / Document Studio C++17** (`tinta_nucleo`, `DocumentCore`, Lienzo Infinito 1,600+ págs, Printout PDF/Word/PPTX/Excel, Lasso/Crop, Multi-Format Migration `.goodnotes/.note/.one/.onepkg/.tinta/.docx/.pptx/.pdf`, Knowledge Graph, Audio Timeline, Gemini Notebook).
- `academic/` — Integración académica **TEC Digital** (`TDA/TDS/GraphQL/Guía Pública`) y **HorariosTEC**.
- `ui_glass/` — Sistema de diseño **guías de interfaz + material translúcido 2.0 + 18 Anti-patrones + Tipografía Matemática 2D**.
- `apps/` — Aplicaciones por plataforma sobre los mismos motores compartidos:
  - `apps/windows/` — Suite nativa **Windows x64** (`CalculadoraUnificada.exe` + `CuadernU.exe`), con **3 modos para `fx-CG50`**:
    1. **Núcleo propio independiente (`SH7305 v47`)**: 64-bit puro, sin DLLs de Casio.
    2. **Núcleo oficial (DLLs Casio)**: mediante proceso auxiliar aislado de 32 bits (`cg50_official_gcal_host32.exe`) por memoria compartida.
    3. **Comparador en vivo (Oficial vs. Propio)**: activable opcionalmente desde el menú *Motor fx-CG50* (vista LCD lado a lado, mapa de diferencias píxel a píxel, métricas en vivo y captura PNG + JSON).
  - `apps/android/` — App **Flutter + Android NDK** (16 KB ELF, 50/50 teclas táctiles, teclado físico extendido, Key-Log, puente LAN/ADB).
  - `apps/ios_native/` — App unificada **iOS / iPadOS** (`TintaEstudio`: Swift + Metal Ink 120 Hz + `CalculadorasCasio` + `project.yml` XcodeGen enlazando directo a `core/` y `studio/tinta_core`).
  - `apps/linux/` — Puerto de escritorio **GTK + Cairo** enlazado directamente a `core/` y `studio/`.
- `assets/` — Carátulas (`skins/`), fuentes (`fonts/`), catálogo (`catalog/`), guías (`guias/`), recursos CuadernU (`cuadernu/`) y `roms/`.
- `private_local/` *(ignorado por Git)* — Activos locales pesados: instaladores originales (`instaladores_casio/`), runtime DLLs oficiales Casio (`official_casio_runtime/`), 16 manuales PDF (`manuales/`), tipografía personal (`tipografia_personal/`) e investigación histórica (`investigacion/`).
- `build/` *(ignorado por Git)* — Todas las salidas de compilación (`core-v47/`, `windows-x64/`, `windows-gcal32/`, `studio-x64/`).
- `tools/` — Scripts de compilación, auditoría (`check_workspace_hygiene.py`, `build_ios_xcframework.sh`, `cg50_ci_runner.py`).

## Reglas Permanentes de Diseño y Orden

Consultar [AGENTS.md](AGENTS.md). Ninguna tarea se cierra sin pasar `python tools/check_workspace_hygiene.py` con código `0`.
