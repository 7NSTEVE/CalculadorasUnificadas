#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-release"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    cat <<'EOF'
Usage: build_linux.sh

Configures a Release build with Ninja in ./build-release, builds all Linux
targets (CasioLinuxEngines, DocumentCoreLinux, calculadora-unificada,
document-studio, and test binaries), and runs the CTest suite.
EOF
    exit 0
fi

now_sec() {
    date +%s.%N
}

elapsed_sec() {
    awk -v t0="$1" -v t1="$2" 'BEGIN { printf "%.2f", (t1 - t0) }'
}

echo "=== [1/3] Configuring Release build (Ninja) ==="
t_cfg_start="$(now_sec)"
cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=Release
t_cfg_end="$(now_sec)"
echo "Configure time: $(elapsed_sec "${t_cfg_start}" "${t_cfg_end}")s"

echo "=== [2/3] Building all targets ==="
t_bld_start="$(now_sec)"
cmake --build "${BUILD_DIR}" --parallel
t_bld_end="$(now_sec)"
echo "Build time: $(elapsed_sec "${t_bld_start}" "${t_bld_end}")s"

echo "=== [3/3] Running CTest suite ==="
t_tst_start="$(now_sec)"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
t_tst_end="$(now_sec)"
echo "CTest time: $(elapsed_sec "${t_tst_start}" "${t_tst_end}")s"

echo "=== Build & CTest complete ==="
echo "  calculadora-unificada: ${BUILD_DIR}/app/calculadora-unificada"
echo "  document-studio:       ${BUILD_DIR}/doc_studio_app/document-studio"
