/*
 * tests/core/c23_abi_conformance_test.c
 * Strict ISO C23 ABI conformance verification for CalculadorasUnificadas
 * FFI boundaries (casio_ffi.h + cuadernu_studio_ffi.h).
 * Compiles under MSVC /std:clatest, Clang 21 -std=c23, and GCC 16 -std=c23.
 */

#include "casio_ffi.h"
#include "cuadernu_studio_ffi.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(__clang__)
#ifndef nullptr
#define nullptr ((void*)0)
#endif
#define CU_NODISCARD
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define CU_NODISCARD [[nodiscard]]
#else
#ifndef nullptr
#define nullptr ((void*)0)
#endif
#define CU_NODISCARD
#endif

static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");
static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");

CU_NODISCARD static bool test_casio_alloc_and_null_guards(void) {
    void* p = casio_alloc(64);
    if (p == nullptr) {
        return false;
    }
    memset(p, 0x5A, 64);
    casio_free(p);

    if (casio_nx_screen_width(nullptr) > 0) return false;
    if (casio_cw2_screen_width(nullptr) > 0) return false;
    if (casio_cg50_step(nullptr, 1, 0) > 0) return false;
    return true;
}

CU_NODISCARD static bool test_studio_ffi_roundtrip(void) {
    char caps[4096];
    int n = cuadernu_ffi_capabilities_json(caps, (int)sizeof(caps));
    if (n <= 0 || n >= (int)sizeof(caps)) {
        return false;
    }
    if (strstr(caps, "{") == nullptr) {
        return false;
    }

    CuadernuDocHandle* doc = cuadernu_doc_create(0);
    if (doc == nullptr) {
        return false;
    }

    bool ok = true;
    if (cuadernu_doc_set_title(doc, "C23 ABI Conformance Doc") < 0) ok = false;
    if (cuadernu_doc_add_page(doc, 0) < 0) ok = false;
    if (cuadernu_doc_page_count(doc) < 1) ok = false;

    const float pts[6] = { 10.0f, 10.0f, 0.5f, 42.0f, 28.0f, 0.8f };
    uint64_t stroke_id = cuadernu_doc_add_ink_stroke(doc, 0, 0, pts, 2, 0x112233FFu, 2.0, 0, 0);
    if (stroke_id == 0) ok = false;

    cuadernu_doc_destroy(doc);
    return ok;
}

int main(void) {
    const bool alloc_ok = test_casio_alloc_and_null_guards();
    const bool studio_ok = test_studio_ffi_roundtrip();
    if (!alloc_ok || !studio_ok) {
        fprintf(stderr, "C23_ABI_CONFORMANCE_FAIL alloc_ok=%d studio_ok=%d\n",
                alloc_ok ? 1 : 0, studio_ok ? 1 : 0);
        return 1;
    }
    printf("C23_ABI_CONFORMANCE_PASS stdc_version=%ld\n",
#ifdef __STDC_VERSION__
           (long)__STDC_VERSION__
#else
           0L
#endif
    );
    return 0;
}
