/*
 * studio/c_api/cuadernu_sync_clock_c23.c
 * Strict ISO C23 portable implementation (MSVC /std:clatest, Clang 21 -std=c23, GCC 16 -std=c23).
 */

#include "cuadernu_sync_clock_c23.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(__clang__)
#ifndef nullptr
#define nullptr ((void*)0)
#endif
#endif

static_assert(sizeof(double) == 8, "IEEE-754 double expected");

static void safe_copy_str(char* dst, size_t dst_cap, const char* src) {
    if (dst == nullptr || dst_cap == 0) return;
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    size_t n = strlen(src);
    if (n >= dst_cap) n = dst_cap - 1u;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* ---- 1. RelojNotaViva ---- */
void cuadernu_clock_init(CuadernuLiveNoteClock* clk) {
    if (clk == nullptr) return;
    clk->state = CU_CLOCK_STOPPED;
    clk->accumulated_sec = 0.0;
    clk->segment_start_sec = 0.0;
}

bool cuadernu_clock_start(CuadernuLiveNoteClock* clk, double now_sec) {
    if (clk == nullptr || clk->state != CU_CLOCK_STOPPED) return false;
    clk->state = CU_CLOCK_RECORDING;
    clk->accumulated_sec = 0.0;
    clk->segment_start_sec = now_sec;
    return true;
}

bool cuadernu_clock_pause(CuadernuLiveNoteClock* clk, double now_sec, double* out_elapsed) {
    if (clk == nullptr || clk->state != CU_CLOCK_RECORDING) return false;
    double delta = now_sec - clk->segment_start_sec;
    if (delta > 0.0) clk->accumulated_sec += delta;
    clk->state = CU_CLOCK_PAUSED;
    if (out_elapsed != nullptr) *out_elapsed = clk->accumulated_sec;
    return true;
}

bool cuadernu_clock_resume(CuadernuLiveNoteClock* clk, double now_sec) {
    if (clk == nullptr || clk->state != CU_CLOCK_PAUSED) return false;
    clk->state = CU_CLOCK_RECORDING;
    clk->segment_start_sec = now_sec;
    return true;
}

double cuadernu_clock_stop(CuadernuLiveNoteClock* clk, double now_sec) {
    if (clk == nullptr) return 0.0;
    if (clk->state == CU_CLOCK_RECORDING) {
        double delta = now_sec - clk->segment_start_sec;
        if (delta > 0.0) clk->accumulated_sec += delta;
    }
    clk->state = CU_CLOCK_STOPPED;
    return clk->accumulated_sec;
}

double cuadernu_clock_elapsed_at(const CuadernuLiveNoteClock* clk, double now_sec) {
    if (clk == nullptr) return 0.0;
    if (clk->state == CU_CLOCK_RECORDING) {
        double delta = now_sec - clk->segment_start_sec;
        return clk->accumulated_sec + (delta > 0.0 ? delta : 0.0);
    }
    return clk->accumulated_sec;
}

/* ---- 2. SecuenciaCommitAutoguardado ---- */
CuadernuCommitResult cuadernu_autosave_commit(
    const uint8_t* canonical_bytes,
    size_t byte_len,
    CuadernuPersistJournalFn journal_fn,
    CuadernuPersistCanonicalFn canonical_fn,
    void* user_ctx)
{
    if (canonical_bytes == nullptr || journal_fn == nullptr || canonical_fn == nullptr) {
        return CU_COMMIT_INVALID_ARGS;
    }
    if (!journal_fn(user_ctx, canonical_bytes, byte_len)) {
        return CU_COMMIT_JOURNAL_FAILED;
    }
    if (!canonical_fn(user_ctx, canonical_bytes, byte_len)) {
        return CU_COMMIT_CANONICAL_FAILED;
    }
    return CU_COMMIT_OK;
}

/* ---- 3. EstadoAutorizacionWebIPad ---- */
void cuadernu_lan_auth_init(CuadernuLanWebAuth* auth, const char* pairing_code, double duration_sec) {
    if (auth == nullptr) return;
    memset(auth, 0, sizeof(*auth));
    safe_copy_str(auth->pairing_code, sizeof(auth->pairing_code), pairing_code);
    auth->duration_sec = duration_sec > 0.0 ? duration_sec : 60.0;
    auth->next_seq = 1u;
}

CuadernuLanAuthOutcome cuadernu_lan_auth_open(
    CuadernuLanWebAuth* auth,
    const char* code_or_null,
    const char* cookie_or_null,
    const char* new_token,
    double now_sec,
    uint64_t* out_next_seq)
{
    if (auth == nullptr || new_token == nullptr || new_token[0] == '\0') {
        return CU_LAN_AUTH_REJECTED;
    }

    if (cookie_or_null != nullptr && cookie_or_null[0] != '\0' && auth->has_session) {
        if (strcmp(cookie_or_null, auth->active_token) == 0) {
            if (now_sec > auth->expires_at_sec) {
                auth->has_session = false;
                auth->active_token[0] = '\0';
                return CU_LAN_AUTH_EXPIRED;
            }
            auth->expires_at_sec = now_sec + auth->duration_sec;
            if (out_next_seq != nullptr) *out_next_seq = auth->next_seq;
            return CU_LAN_AUTH_RENEWED;
        }
    }

    if (code_or_null == nullptr || strcmp(code_or_null, auth->pairing_code) != 0) {
        return CU_LAN_AUTH_REJECTED;
    }

    safe_copy_str(auth->active_token, sizeof(auth->active_token), new_token);
    auth->expires_at_sec = now_sec + auth->duration_sec;
    auth->has_session = true;
    auth->next_seq = 1u;
    if (out_next_seq != nullptr) *out_next_seq = auth->next_seq;
    return CU_LAN_AUTH_CREATED;
}

bool cuadernu_lan_auth_validate(
    const CuadernuLanWebAuth* auth,
    const char* cookie_token,
    double now_sec)
{
    if (auth == nullptr || !auth->has_session || cookie_token == nullptr) return false;
    if (now_sec > auth->expires_at_sec) return false;
    return strcmp(auth->active_token, cookie_token) == 0;
}
