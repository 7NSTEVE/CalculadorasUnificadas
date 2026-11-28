# Third-Party Notices & Licensing

This repository is licensed under the **GNU General Public License v3.0 (GPL-3.0)** (see `LICENSE`).

## Embedded Emulation Cores
- **`core/casio_cw2/`**: Headless C++17/C++20 port of `HW_CLASSWIZ_II` (`nX-U16/100`, 4-level grayscale LCD) derived from **CasioEmuX** (GPL-3.0). All SDL2, Lua, and ImGui dependencies have been stripped and isolated under `namespace casioemux`.
- **`core/casio_nx/`**: Headless C++17 port of `HW_CLASSWIZ` and `HW_ES_PLUS` (`CalcEmuCore`, derived from CasioEmu, GPL-3.0) under `namespace casioemu`.
- **`core/casio_cg50/`**: Native SH7305 (SH-4A) + R61524 LCD core for fx-CG50 PRIZM (`Cg50NativeCore`).

## Frontend Visualization Libraries (Offline Vendored)
- **`assets/cuadernu/tutor_web/vendor/three.module.js` & `three.core.js`**: **Three.js r187** (MIT License, see `assets/cuadernu/tutor_web/vendor/LICENCIA_THREE.txt`), bundled locally for 100% offline WebGL2 interactive 3D physics visuals inside CuadernU.
