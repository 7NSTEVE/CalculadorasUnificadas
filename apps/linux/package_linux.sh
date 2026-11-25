#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CORE_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd -- "${CORE_DIR}/.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-release"
DIST_DIR="${SCRIPT_DIR}/dist-linux"

FORCE_BUILD=0
for arg in "$@"; do
    case "$arg" in
        --rebuild)
            FORCE_BUILD=1
            ;;
        --help|-h)
            cat <<'EOF'
Usage: package_linux.sh [--rebuild]

Builds Release targets if missing (or always with --rebuild) and assembles a
self-contained portable Linux bundle at ./dist-linux/.
EOF
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            exit 2
            ;;
    esac
done

CALC_BIN="${BUILD_DIR}/app/calculadora-unificada"
STUDIO_BIN="${BUILD_DIR}/doc_studio_app/document-studio"

if [[ "${FORCE_BUILD}" -eq 1 || ! -f "${BUILD_DIR}/build.ninja" || ! -x "${CALC_BIN}" || ! -x "${STUDIO_BIN}" ]]; then
    "${SCRIPT_DIR}/build_linux.sh"
else
    echo "=== Updating Release build ==="
    cmake --build "${BUILD_DIR}" --parallel
fi

if [[ ! -x "${CALC_BIN}" ]]; then
    echo "ERROR: Missing executable ${CALC_BIN}" >&2
    exit 1
fi
if [[ ! -x "${STUDIO_BIN}" ]]; then
    echo "ERROR: Missing executable ${STUDIO_BIN}" >&2
    exit 1
fi

find_first_dir_with_files() {
    local check_file="$1"
    shift
    for d in "$@"; do
        if [[ -n "$d" && -f "${d}/${check_file}" ]]; then
            printf '%s\n' "$d"
            return 0
        fi
    done
    return 1
}

ROMS_SRC="$(find_first_dir_with_files "fx991lax.rom" \
    "${CORE_DIR}/roms" \
    "${REPO_ROOT}/casio_universal_app/assets/roms")" || {
    echo "ERROR: Could not locate nX-U8 ROMs directory (fx991lax.rom)" >&2
    exit 1
}

CG50_ROMS_SRC="$(find_first_dir_with_files "official-ILRAM-FD800000-4000.bin" \
    "${CORE_DIR}/cg50_roms" \
    "${CORE_DIR}/roms" \
    "${REPO_ROOT}/casio_universal_app/assets/roms")" || {
    echo "ERROR: Could not locate CG50 ROMs directory" >&2
    exit 1
}

SKINS_SRC="$(find_first_dir_with_files "caratula_classwiz.png" \
    "${CORE_DIR}/skins" \
    "${REPO_ROOT}/casio_universal_app/assets/skins")" || {
    echo "ERROR: Could not locate calculator skins directory" >&2
    exit 1
}

CATALOG_SRC="${CORE_DIR}/catalog/casio_knowledge_catalog.json"
MANUALS_SRC="${CORE_DIR}/manuales"
RUNTIME_CW2_SRC="${CORE_DIR}/runtime/fx991cw"
INTERFACE_PNG_SRC="${RUNTIME_CW2_SRC}/models/fx991cw_emu/interface.png"
if [[ ! -f "${INTERFACE_PNG_SRC}" && -f "${SKINS_SRC}/interface.png" ]]; then
    INTERFACE_PNG_SRC="${SKINS_SRC}/interface.png"
fi

for req in \
    "${ROMS_SRC}/fx991lax.rom" \
    "${ROMS_SRC}/fx570vnplus.rom" \
    "${CG50_ROMS_SRC}/SystemROM.raw" \
    "${CG50_ROMS_SRC}/UserROM.raw" \
    "${CG50_ROMS_SRC}/C2ROM.raw" \
    "${CG50_ROMS_SRC}/official-ILRAM-FD800000-4000.bin" \
    "${SKINS_SRC}/caratula_classwiz.png" \
    "${SKINS_SRC}/caratula_esplus.png" \
    "${SKINS_SRC}/caratula_cg50.png" \
    "${INTERFACE_PNG_SRC}" \
    "${CATALOG_SRC}" \
    "${MANUALS_SRC}/indice_paginas_pdf.tsv" \
    "${RUNTIME_CW2_SRC}/emulator/casioemu.exe" \
    "${RUNTIME_CW2_SRC}/models/fx991cw_emu/rom.bin"; do
    if [[ ! -f "${req}" ]]; then
        echo "ERROR: Required runtime asset missing: ${req}" >&2
        exit 1
    fi
done

echo "=== Assembling portable bundle at ${DIST_DIR} ==="
mkdir -p \
    "${DIST_DIR}/bin" \
    "${DIST_DIR}/roms" \
    "${DIST_DIR}/cg50_roms" \
    "${DIST_DIR}/skins" \
    "${DIST_DIR}/catalog" \
    "${DIST_DIR}/manuales" \
    "${DIST_DIR}/runtime/fx991cw" \
    "${DIST_DIR}/share/applications"

install -m 0755 "${CALC_BIN}" "${DIST_DIR}/bin/calculadora-unificada"
install -m 0755 "${STUDIO_BIN}" "${DIST_DIR}/bin/document-studio"

cp -f "${ROMS_SRC}/fx991lax.rom" "${DIST_DIR}/roms/"
cp -f "${ROMS_SRC}/fx570vnplus.rom" "${DIST_DIR}/roms/"

cp -f "${CG50_ROMS_SRC}/SystemROM.raw" "${DIST_DIR}/cg50_roms/"
cp -f "${CG50_ROMS_SRC}/UserROM.raw" "${DIST_DIR}/cg50_roms/"
cp -f "${CG50_ROMS_SRC}/C2ROM.raw" "${DIST_DIR}/cg50_roms/"
cp -f "${CG50_ROMS_SRC}/official-ILRAM-FD800000-4000.bin" "${DIST_DIR}/cg50_roms/"

cp -f "${SKINS_SRC}/caratula_classwiz.png" "${DIST_DIR}/skins/"
cp -f "${SKINS_SRC}/caratula_esplus.png" "${DIST_DIR}/skins/"
cp -f "${SKINS_SRC}/caratula_cg50.png" "${DIST_DIR}/skins/"
cp -f "${INTERFACE_PNG_SRC}" "${DIST_DIR}/skins/interface.png"

cp -f "${CATALOG_SRC}" "${DIST_DIR}/catalog/casio_knowledge_catalog.json"

cp -f "${MANUALS_SRC}/"*.pdf "${DIST_DIR}/manuales/"
cp -f "${MANUALS_SRC}/indice_paginas_pdf.tsv" "${DIST_DIR}/manuales/"

cp -a "${RUNTIME_CW2_SRC}/." "${DIST_DIR}/runtime/fx991cw/"

cat > "${DIST_DIR}/run_calculadora.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
DIST_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export CASIO_UNIFIED_HOME="${DIST_DIR}"
exec "${DIST_DIR}/bin/calculadora-unificada" "$@"
EOF
chmod 0755 "${DIST_DIR}/run_calculadora.sh"

cat > "${DIST_DIR}/run_document_studio.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
DIST_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export CASIO_UNIFIED_HOME="${DIST_DIR}"
if [[ $# -eq 1 && "$1" != -* && -f "$1" ]]; then
    exec "${DIST_DIR}/bin/document-studio" --open "$1"
fi
exec "${DIST_DIR}/bin/document-studio" "$@"
EOF
chmod 0755 "${DIST_DIR}/run_document_studio.sh"

cat > "${DIST_DIR}/share/applications/calculadora-unificada.desktop" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=Calculadora Unificada CASIO
GenericName=Scientific & Graphing Calculator Suite
Comment=Unified fx-570LA X, fx-570VN PLUS, fx-CG50 PRIZM, and fx-991CW emulator suite
Exec="${DIST_DIR}/run_calculadora.sh"
Terminal=false
Categories=Education;Science;Math;
Keywords=CASIO;Calculator;ClassWiz;PRIZM;fx-CG50;fx-991CW;
StartupNotify=true
EOF

cat > "${DIST_DIR}/share/applications/document-studio.desktop" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=Document Studio — Calculadora Unificada
GenericName=PDF & Infinite Canvas Study Studio
Comment=Interactive PDF annotator, canvas workspace, and DOCX/PPTX/PDF exporter
Exec="${DIST_DIR}/run_document_studio.sh" %f
Terminal=false
Categories=Office;Education;Graphics;
MimeType=application/pdf;
Keywords=PDF;Canvas;Annotations;Study;Notes;
StartupNotify=true
EOF

cat > "${DIST_DIR}/README-LINUX.md" <<'EOF'
# Calculadora Unificada & Document Studio — Portable Linux Bundle

Self-contained Linux x86_64 GTK3/Cairo distribution containing both native applications and all required ROMs, calculator skins, manuals, and knowledge catalog assets.

## Bundle Contents

- `bin/calculadora-unificada` — Main unified CASIO suite (fx-570LA X Blanca, fx-570VN PLUS Gris, fx-CG50 PRIZM, fx-991CW Negra)
- `bin/document-studio` — Native PDF annotator & infinite canvas workspace
- `run_calculadora.sh` — Portable launcher setting `CASIO_UNIFIED_HOME` to this bundle root
- `run_document_studio.sh` — Portable launcher for Document Studio (accepts CLI flags or a file path)
- `roms/` — `fx991lax.rom`, `fx570vnplus.rom`
- `cg50_roms/` — `SystemROM.raw`, `UserROM.raw`, `C2ROM.raw`, `official-ILRAM-FD800000-4000.bin`
- `skins/` — `caratula_classwiz.png`, `caratula_esplus.png`, `caratula_cg50.png`, `interface.png`
- `catalog/` — `casio_knowledge_catalog.json`
- `manuales/` — 16 official PDF manuals + `indice_paginas_pdf.tsv`
- `runtime/fx991cw/` — Optional Wine64 runtime assets for external CasioEmuX mode
- `share/applications/` — `.desktop` launcher entries

## Quick Start

```bash
./run_calculadora.sh
./run_document_studio.sh
```

You can also move the entire `dist-linux/` directory anywhere on disk and run `./run_calculadora.sh` or `./run_document_studio.sh` directly. Both binaries locate assets automatically via `CASIO_UNIFIED_HOME` and `/proc/self/exe`.

### CLI Options

**`run_calculadora.sh`:**
- `--compact` / `--compact-gray` / `--compact-cg50` / `--compact-cw2`
- `--popup`
- `--help-search <query>`
- `--studio`
- `--smoke-test`
- `--screenshot-out <output.png>`

**`run_document_studio.sh`:**
- `--open <file.pdf|file.cudoc|file.docx|file.pptx>`
- `--page <1-based>`
- `--mode <pdf|canvas>`
- `--smoke-test`
- `--screenshot-out <output.png>`

## Runtime System Dependencies (Ubuntu / Debian)

Ensure standard desktop shared libraries are installed:

```bash
sudo apt-get install libgtk-3-0 libpoppler-glib8 libcairo2 libpango-1.0-0 libpangocairo-1.0-0 libgdk-pixbuf-2.0-0 libssl3 zlib1g
```

Optional:
- `tesseract-ocr` for OCR extraction in Document Studio
- `wine64` if launching the external `casioemu.exe` window for fx-991CW (otherwise the built-in native fx-991CW mode runs automatically)

## Desktop Integration

If you relocate `dist-linux/`, update the `Exec=` path inside `share/applications/*.desktop` to match the new location and copy or symlink them to `~/.local/share/applications/`.
EOF

pdf_count="$(find "${DIST_DIR}/manuales" -maxdepth 1 -type f -name '*.pdf' | wc -l)"
echo "=== Portable bundle assembled ==="
echo "  Location: ${DIST_DIR}"
echo "  PDFs packaged: ${pdf_count}"
echo "  Binaries:"
ls -lh "${DIST_DIR}/bin/calculadora-unificada" "${DIST_DIR}/bin/document-studio"
