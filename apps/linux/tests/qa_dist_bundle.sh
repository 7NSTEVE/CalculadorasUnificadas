#!/usr/bin/env bash
set -u

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
LINUX_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${LINUX_DIR}/build-release"
DIST_DIR="${LINUX_DIR}/dist-linux"
OUT_TMP="/tmp/casio_qa_$$"
mkdir -p "${OUT_TMP}"

COPY_SCREENSHOTS_DIR=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --copy-screenshots)
            COPY_SCREENSHOTS_DIR="${2:-}"
            shift 2
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 2
            ;;
    esac
done

PASS_COUNT=0
FAIL_COUNT=0
WARN_COUNT=0

record_pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    echo "[PASS] $1"
}

record_fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    echo "[FAIL] $1" >&2
}

record_warn() {
    WARN_COUNT=$((WARN_COUNT + 1))
    echo "[WARN] $1"
}

echo "============================================================"
echo "QA Verification Suite: Calculadora Unificada & Document Studio"
echo "LINUX_DIR=${LINUX_DIR}"
echo "DIST_DIR=${DIST_DIR}"
echo "OUT_TMP=${OUT_TMP}"
echo "============================================================"

# 1. Verify portable bundle files and permissions
required_files=(
    "bin/calculadora-unificada"
    "bin/document-studio"
    "roms/fx991lax.rom"
    "roms/fx570vnplus.rom"
    "cg50_roms/SystemROM.raw"
    "cg50_roms/UserROM.raw"
    "cg50_roms/C2ROM.raw"
    "cg50_roms/official-ILRAM-FD800000-4000.bin"
    "skins/caratula_classwiz.png"
    "skins/caratula_esplus.png"
    "skins/caratula_cg50.png"
    "skins/interface.png"
    "catalog/casio_knowledge_catalog.json"
    "manuales/indice_paginas_pdf.tsv"
    "runtime/fx991cw/emulator/casioemu.exe"
    "runtime/fx991cw/models/fx991cw_emu/rom.bin"
    "runtime/fx991cw/models/fx991cw_emu/interface.png"
    "run_calculadora.sh"
    "run_document_studio.sh"
    "share/applications/calculadora-unificada.desktop"
    "share/applications/document-studio.desktop"
    "README-LINUX.md"
)

for rel in "${required_files[@]}"; do
    if [[ -s "${DIST_DIR}/${rel}" ]]; then
        record_pass "Bundle file present & non-empty: ${rel}"
    else
        record_fail "Missing or empty bundle file: ${rel}"
    fi
done

pdf_count="$(find "${DIST_DIR}/manuales" -maxdepth 1 -type f -name '*.pdf' | wc -l)"
if [[ "${pdf_count}" -eq 16 ]]; then
    record_pass "Bundle contains 16 PDF manuals"
else
    record_fail "Expected 16 PDFs in dist-linux/manuales, got ${pdf_count}"
fi

# 2. Verify ELF x86-64 binaries and zero missing shared libs
for bin in "${DIST_DIR}/bin/calculadora-unificada" "${DIST_DIR}/bin/document-studio"; do
    f_out="$(file "${bin}")"
    if [[ "${f_out}" == *"ELF 64-bit"* && "${f_out}" == *"x86-64"* ]]; then
        record_pass "ELF x86-64 verified: $(basename "${bin}")"
    else
        record_fail "Unexpected binary format for ${bin}: ${f_out}"
    fi
    if ldd "${bin}" | grep -qi "not found"; then
        record_fail "Missing shared libraries in ${bin}"
        ldd "${bin}" | grep -i "not found" >&2
    else
        record_pass "ldd 0 missing shared libraries: $(basename "${bin}")"
    fi
done

# 3. Verify LF line endings on scripts
for sh in \
    "${LINUX_DIR}/build_linux.sh" \
    "${LINUX_DIR}/package_linux.sh" \
    "${DIST_DIR}/run_calculadora.sh" \
    "${DIST_DIR}/run_document_studio.sh"; do
    if grep -q $'\r' "${sh}"; then
        record_fail "CRLF line endings found in ${sh}"
    else
        record_pass "LF line endings & script check: $(basename "${sh}")"
    fi
done

# 4. Run build-release smoke tests directly (checking build-release asset discovery)
if [[ -x "${BUILD_DIR}/app/calculadora-unificada" ]]; then
    if env -u CASIO_UNIFIED_HOME XDG_CONFIG_HOME="${OUT_TMP}/xdg_build" \
        xvfb-run -a -s "-screen 0 1440x960x24" "${BUILD_DIR}/app/calculadora-unificada" --smoke-test \
        >"${OUT_TMP}/build_cu.out" 2>"${OUT_TMP}/build_cu.err"; then
        record_pass "build-release calculadora-unificada --smoke-test exit 0"
    else
        record_fail "build-release calculadora-unificada --smoke-test failed"
    fi
fi

if [[ -x "${BUILD_DIR}/doc_studio_app/document-studio" ]]; then
    if env -u CASIO_UNIFIED_HOME XDG_CONFIG_HOME="${OUT_TMP}/xdg_build" \
        xvfb-run -a -s "-screen 0 1440x960x24" "${BUILD_DIR}/doc_studio_app/document-studio" --smoke-test \
        >"${OUT_TMP}/build_ds.out" 2>"${OUT_TMP}/build_ds.err"; then
        record_pass "build-release document-studio --smoke-test exit 0"
    else
        record_fail "build-release document-studio --smoke-test failed"
    fi
fi

# 5. Helper to run GUI/CLI tests from dist-linux with clean XDG_CONFIG_HOME and check stderr
run_gui_case() {
    local name="$1"
    local workdir="$2"
    shift 2
    local out="${OUT_TMP}/${name}.out"
    local err="${OUT_TMP}/${name}.err"
    (
        cd "${workdir}" &&
        env XDG_CONFIG_HOME="${OUT_TMP}/xdg_${name}" "$@"
    ) >"${out}" 2>"${err}"
    local rc=$?
    if [[ ${rc} -eq 0 ]]; then
        record_pass "CLI/GUI case '${name}' exited 0"
    else
        record_fail "CLI/GUI case '${name}' failed with exit=${rc}"
        cat "${err}" >&2 || true
    fi
    if grep -Eqi "Gtk-CRITICAL|Gdk-CRITICAL|GLib-.*CRITICAL" "${err}"; then
        record_warn "GTK/GLib critical warning in '${name}': $(tr '\n' ' ' < "${err}")"
    fi
}

# 6. Run dist-linux Xvfb QA suite
run_gui_case "cu_blanca_gris" "${DIST_DIR}" \
    xvfb-run -a -s "-screen 0 1440x960x24" ./run_calculadora.sh --smoke-test --screenshot-out "${OUT_TMP}/qa_cu_blanca_gris.png"

run_gui_case "cu_compact" "${DIST_DIR}" \
    xvfb-run -a -s "-screen 0 1440x960x24" ./run_calculadora.sh --compact --smoke-test --screenshot-out "${OUT_TMP}/qa_cu_compact.png"

run_gui_case "cu_compact_gray" "${DIST_DIR}" \
    xvfb-run -a -s "-screen 0 1440x960x24" ./run_calculadora.sh --compact-gray --smoke-test --screenshot-out "${OUT_TMP}/qa_cu_gray.png"

run_gui_case "cu_compact_cg50" "${DIST_DIR}" \
    xvfb-run -a -s "-screen 0 1440x960x24" ./run_calculadora.sh --compact-cg50 --smoke-test --screenshot-out "${OUT_TMP}/qa_cu_cg50.png"

run_gui_case "cu_compact_cw2" "${DIST_DIR}" \
    xvfb-run -a -s "-screen 0 1440x960x24" ./run_calculadora.sh --compact-cw2 --smoke-test --screenshot-out "${OUT_TMP}/qa_cu_cw2.png"

run_gui_case "cu_popup" "${DIST_DIR}" \
    xvfb-run -a -s "-screen 0 1440x960x24" ./run_calculadora.sh --popup --screenshot-out "${OUT_TMP}/qa_cu_popup.png"

run_gui_case "cu_help_search" "${DIST_DIR}" \
    xvfb-run -a -s "-screen 0 1440x960x24" ./run_calculadora.sh --help-search "fraccion" --screenshot-out "${OUT_TMP}/qa_cu_help.png"

run_gui_case "ds_pdf" "${DIST_DIR}" \
    xvfb-run -a -s "-screen 0 1440x960x24" ./run_document_studio.sh --open manuales/fx-570_991LA_X_ES.pdf --page 5 --mode pdf --smoke-test --screenshot-out "${OUT_TMP}/qa_ds_pdf.png"

run_gui_case "ds_canvas" "${DIST_DIR}" \
    xvfb-run -a -s "-screen 0 1440x960x24" ./run_document_studio.sh --mode canvas --smoke-test --screenshot-out "${OUT_TMP}/qa_ds_canvas.png"

# 7. Test /proc/self/exe resolution from /tmp with CASIO_UNIFIED_HOME unset
(
    cd /tmp &&
    env -u CASIO_UNIFIED_HOME XDG_CONFIG_HOME="${OUT_TMP}/xdg_procself" \
        xvfb-run -a -s "-screen 0 1440x960x24" "${DIST_DIR}/bin/calculadora-unificada" --smoke-test \
        >"${OUT_TMP}/procself.out" 2>"${OUT_TMP}/procself.err"
)
if [[ $? -eq 0 ]] && grep -q "DiscoverAssetPaths rootDir=${DIST_DIR}" "${OUT_TMP}/procself.out"; then
    record_pass "/proc/self/exe asset resolution without CASIO_UNIFIED_HOME points to dist-linux"
else
    record_fail "/proc/self/exe resolution failed when run from /tmp without CASIO_UNIFIED_HOME"
    cat "${OUT_TMP}/procself.out" >&2 || true
fi

# 8. Test sibling --studio launch inside Xvfb (dist-linux & build-release)
verify_studio_spawn() {
    local label="$1"
    local workdir="$2"
    local calc_cmd="$3"
    local expected_exe="$4"
    local found_exe=""
    found_exe="$(
        cd "${workdir}" &&
        env XDG_CONFIG_HOME="${OUT_TMP}/xdg_${label}" \
            xvfb-run -a -s "-screen 0 1440x960x24" bash -c '
                calc_cmd="$1"
                "$calc_cmd" --studio >/dev/null 2>&1 &
                cpid=$!
                matched=""
                for _ in $(seq 1 60); do
                    for p in $(pgrep -x document-studio 2>/dev/null || true); do
                        exe="$(readlink -f "/proc/$p/exe" 2>/dev/null || true)"
                        if [[ -n "$exe" ]]; then
                            matched="$exe"
                            break
                        fi
                    done
                    if [[ -n "$matched" ]]; then
                        break
                    fi
                    sleep 0.2
                done
                pkill -x document-studio >/dev/null 2>&1 || true
                pkill -x calculadora-unificada >/dev/null 2>&1 || true
                kill "$cpid" >/dev/null 2>&1 || true
                wait "$cpid" 2>/dev/null || true
                printf "%s" "$matched"
            ' _ "${calc_cmd}"
    )"
    if [[ "${found_exe}" == "${expected_exe}" ]]; then
        record_pass "Sibling process launch (${label}): spawned ${found_exe}"
    else
        record_fail "Sibling process launch (${label}): expected '${expected_exe}', got '${found_exe}'"
    fi
}

verify_studio_spawn "dist_linux" "${DIST_DIR}" "./run_calculadora.sh" "${DIST_DIR}/bin/document-studio"
verify_studio_spawn "build_release" "${LINUX_DIR}" "${BUILD_DIR}/app/calculadora-unificada" "${BUILD_DIR}/doc_studio_app/document-studio"

# 8b. Verify test_linux_engines resolves assets inside dist-linux when CASIO_UNIFIED_HOME=dist-linux
if [[ -x "${BUILD_DIR}/engines/test_linux_engines" ]]; then
    if env CASIO_UNIFIED_HOME="${DIST_DIR}" "${BUILD_DIR}/engines/test_linux_engines" >"${OUT_TMP}/engines_dist_env.out" 2>"${OUT_TMP}/engines_dist_env.err" && \
       grep -q "DiscoverAssetPaths rootDir (${DIST_DIR})" "${OUT_TMP}/engines_dist_env.out"; then
        record_pass "test_linux_engines with CASIO_UNIFIED_HOME=dist-linux passed & resolved dist-linux"
    else
        record_fail "test_linux_engines with CASIO_UNIFIED_HOME=dist-linux failed"
    fi
fi

# 9. Verify all generated PNGs are valid non-empty PNG files
for png in \
    qa_cu_blanca_gris.png \
    qa_cu_compact.png \
    qa_cu_gray.png \
    qa_cu_cg50.png \
    qa_cu_cw2.png \
    qa_cu_popup.png \
    qa_cu_help.png \
    qa_ds_pdf.png \
    qa_ds_canvas.png; do
    p="${OUT_TMP}/${png}"
    if [[ -s "${p}" ]] && file "${p}" | grep -q "PNG image data"; then
        record_pass "Valid PNG verified: ${png} ($(stat -c%s "${p}") bytes)"
    else
        record_fail "Invalid or missing PNG: ${p}"
    fi
done

# 10. Live WSLg verification (X11 + Wayland if reachable)
if [[ -S /tmp/.X11-unix/X0 ]] && DISPLAY=:0 xdpyinfo >/dev/null 2>&1; then
    run_gui_case "wslg_cu_x11" "${DIST_DIR}" env DISPLAY=:0 GDK_BACKEND=x11 ./run_calculadora.sh --smoke-test
    run_gui_case "wslg_ds_x11" "${DIST_DIR}" env DISPLAY=:0 GDK_BACKEND=x11 ./run_document_studio.sh --smoke-test
else
    record_warn "WSLg X0 socket not reachable; skipped live X11 WSLg test"
fi

if [[ -S /mnt/wslg/runtime-dir/wayland-0 ]]; then
    run_gui_case "wslg_cu_wayland" "${DIST_DIR}" env WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir GDK_BACKEND=wayland ./run_calculadora.sh --smoke-test
    run_gui_case "wslg_ds_wayland" "${DIST_DIR}" env WAYLAND_DISPLAY=wayland-0 XDG_RUNTIME_DIR=/mnt/wslg/runtime-dir GDK_BACKEND=wayland ./run_document_studio.sh --smoke-test
else
    record_warn "WSLg wayland-0 socket not reachable; skipped live Wayland WSLg test"
fi

# 11. Test isolated relocated portable copy in /tmp (proves zero dependency on repo tree)
RELOC_DIR="${OUT_TMP}/relocated_dist"
mkdir -p "${RELOC_DIR}"
cp -a "${DIST_DIR}/." "${RELOC_DIR}/"
if (
    cd /tmp &&
    env -u CASIO_UNIFIED_HOME XDG_CONFIG_HOME="${OUT_TMP}/xdg_reloc" \
        xvfb-run -a -s "-screen 0 1440x960x24" "${RELOC_DIR}/run_calculadora.sh" --smoke-test \
        >"${OUT_TMP}/reloc_cu.out" 2>"${OUT_TMP}/reloc_cu.err"
) && grep -q "DiscoverAssetPaths rootDir=${RELOC_DIR}" "${OUT_TMP}/reloc_cu.out"; then
    record_pass "Relocated portable bundle outside repo passes --smoke-test & resolves assets locally"
else
    record_fail "Relocated portable bundle test failed"
fi

if [[ -n "${COPY_SCREENSHOTS_DIR}" ]]; then
    mkdir -p "${COPY_SCREENSHOTS_DIR}"
    cp -f "${OUT_TMP}"/qa_*.png "${COPY_SCREENSHOTS_DIR}/"
    record_pass "Copied verification screenshots to ${COPY_SCREENSHOTS_DIR}"
fi

echo "============================================================"
echo "QA Summary: PASS=${PASS_COUNT} WARN=${WARN_COUNT} FAIL=${FAIL_COUNT}"
echo "Screenshots folder: ${OUT_TMP}"
echo "============================================================"

if [[ ${FAIL_COUNT} -ne 0 ]]; then
    exit 1
fi
exit 0
