// Verifies that all pure-C headers imported by apps/ios_native/CalculadorasCasio/Fuentes/Puente.h
// (calcemu.h, casio_ffi.h, studio/tinta_core/api.h, cuadernu_studio_ffi.h) coexist cleanly
// inside a single C++20 translation unit with zero symbol or macro collisions.

#include "calcemu.h"
#include "casio_ffi.h"
#include "api.h"
#include "cuadernu_studio_ffi.h"

#include <cstdio>
#include <cstring>

int main() {
    char caps[1024] = {};
    int n = cuadernu_ffi_capabilities_json(caps, static_cast<int>(sizeof(caps)));
    if (n <= 0 || std::strstr(caps, "cudoc_zip_interop") == nullptr) {
        std::fprintf(stderr, "FAIL: cuadernu_ffi_capabilities_json failed (%d)\n", n);
        return 1;
    }

    // Verify tinta_core layout calculation function exposed to iPadOS PanelCasio.swift
    float ol = 0, al = 0, op = 0, ap = 0;
    tinta_panel_reparto(1024.0f, true, 0, 0.38f, 320.0f, 0.66f, 700.0f, &ol, &al, &op, &ap);
    if (al <= 0.0f || ap <= 0.0f) {
        std::fprintf(stderr, "FAIL: tinta_panel_reparto produced invalid split (%f, %f)\n", al, ap);
        return 2;
    }

    // Verify casio_ffi allocation helper coexists cleanly
    void* mem = casio_alloc(64);
    if (!mem) {
        std::fprintf(stderr, "FAIL: casio_alloc returned null\n");
        return 3;
    }
    casio_free(mem);

    std::printf("ios_bridge_coexistence_test: PASS (calcemu + casio_ffi + tinta_core + cuadernu_studio_ffi in C++20; split=%.1f/%.1f)\n", al, ap);
    return 0;
}
