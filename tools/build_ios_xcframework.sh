#!/usr/bin/env bash
# Builds CasioNativeCore.xcframework (iOS device arm64 + Simulator arm64/x86_64)
# directly from canonical single-source core/ v47 (all 4 Casio engines) +
# portable C++17 CuadernU Studio FFI (studio/c_api/cuadernu_studio_ffi.h).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_ROOT="${ROOT_DIR}/build/ios-xcframework"
OUT_XCFW="${BUILD_ROOT}/CasioNativeCore.xcframework"

rm -rf "${BUILD_ROOT}"
mkdir -p "${BUILD_ROOT}/headers"
cp "${ROOT_DIR}/core/casio_ffi/casio_ffi.h" "${BUILD_ROOT}/headers/"
cp "${ROOT_DIR}/core/casio_cg50/c_api/cg50_api_c.h" "${BUILD_ROOT}/headers/"
cp "${ROOT_DIR}/studio/c_api/cuadernu_studio_ffi.h" "${BUILD_ROOT}/headers/"

build_slice() {
  local name="$1"
  local sysname="$2"
  local sdk="$3"
  local archs="$4"
  local bdir="${BUILD_ROOT}/${name}"

  cmake -S "${ROOT_DIR}/core" -B "${bdir}" \
    -DCMAKE_SYSTEM_NAME="${sysname}" \
    -DCMAKE_OSX_SYSROOT="${sdk}" \
    -DCMAKE_OSX_ARCHITECTURES="${archs}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_CORE_TESTS=OFF \
    -DBUILD_CG50_BATTERY=OFF \
    -DCUADERNU_BUILD_STUDIO_FFI=ON

  cmake --build "${bdir}" --target casio_nx casio_cw2 casio_cg50 cuadernu_studio_portable --parallel

  # Combine static archives + FFI object into a single static library for XCFramework
  local ffi_obj="${bdir}/casio_ffi.o"
  local min_flag="-miphoneos-version-min=17.0"
  if [ "${sdk}" = "iphonesimulator" ]; then
    min_flag="-mios-simulator-version-min=17.0"
  fi
  clang++ -std=c++17 -O3 -isysroot "$(xcrun --sdk "${sdk}" --show-sdk-path)" \
    $(for a in $(echo "${archs}" | tr ';' ' '); do echo -arch "$a"; done) \
    "${min_flag}" \
    -I"${ROOT_DIR}/core/casio_ffi" \
    -I"${ROOT_DIR}/core/casio_nx/include" \
    -I"${ROOT_DIR}/core/casio_cw2/include" \
    -I"${ROOT_DIR}/core/casio_cg50" \
    -c "${ROOT_DIR}/core/casio_ffi/casio_ffi.cpp" -o "${ffi_obj}"

  libtool -static -o "${bdir}/libCasioNativeCore.a" \
    "${bdir}/libcasio_nx.a" \
    "${bdir}/libcasio_cw2.a" \
    "${bdir}/libcasio_cg50.a" \
    "${bdir}/libcuadernu_studio_portable.a" \
    "${ffi_obj}"
}

build_slice "ios-device" "iOS" "iphoneos" "arm64"
build_slice "ios-sim"    "iOS" "iphonesimulator" "arm64;x86_64"

xcodebuild -create-xcframework \
  -library "${BUILD_ROOT}/ios-device/libCasioNativeCore.a" -headers "${BUILD_ROOT}/headers" \
  -library "${BUILD_ROOT}/ios-sim/libCasioNativeCore.a"    -headers "${BUILD_ROOT}/headers" \
  -output "${OUT_XCFW}"

echo "Built ${OUT_XCFW}"
