#pragma once
/*
 * studio/c_api/cuadernu_sync_clock_c23.h
 * Portable ISO C23 state machines ported from CuadernU iPadOS Swift modules
 * (RelojNotaViva, SecuenciaCommitAutoguardado, EstadoAutorizacionWebIPad) so
 * Windows, Linux, iPadOS (Puente.h), and Android (dart:ffi) share one verified core.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 1. RelojNotaViva (Pause-drift-free audio/stroke timeline clock) ---- */
typedef enum CuadernuClockState {
    CU_CLOCK_STOPPED = 0,
    CU_CLOCK_RECORDING = 1,
    CU_CLOCK_PAUSED = 2
} CuadernuClockState;

typedef struct CuadernuLiveNoteClock {
    CuadernuClockState state;
    double accumulated_sec;
    double segment_start_sec;
} CuadernuLiveNoteClock;

void cuadernu_clock_init(CuadernuLiveNoteClock* clk);
bool cuadernu_clock_start(CuadernuLiveNoteClock* clk, double now_sec);
bool cuadernu_clock_pause(CuadernuLiveNoteClock* clk, double now_sec, double* out_elapsed);
bool cuadernu_clock_resume(CuadernuLiveNoteClock* clk, double now_sec);
double cuadernu_clock_stop(CuadernuLiveNoteClock* clk, double now_sec);
double cuadernu_clock_elapsed_at(const CuadernuLiveNoteClock* clk, double now_sec);

/* ---- 2. SecuenciaCommitAutoguardado (Ordered Journal -> Canonical commit) ---- */
typedef bool (*CuadernuPersistJournalFn)(void* ctx, const uint8_t* data, size_t len);
typedef bool (*CuadernuPersistCanonicalFn)(void* ctx, const uint8_t* data, size_t len);

typedef enum CuadernuCommitResult {
    CU_COMMIT_OK = 0,
    CU_COMMIT_INVALID_ARGS = 1,
    CU_COMMIT_JOURNAL_FAILED = 2,
    CU_COMMIT_CANONICAL_FAILED = 3
} CuadernuCommitResult;

/* Executes journal write STRICTLY before canonical write; aborts canonical if journal fails. */
CuadernuCommitResult cuadernu_autosave_commit(
    const uint8_t* canonical_bytes,
    size_t byte_len,
    CuadernuPersistJournalFn journal_fn,
    CuadernuPersistCanonicalFn canonical_fn,
    void* user_ctx);

/* ---- 3. EstadoAutorizacionWebIPad (QR-paired LAN remote session auth) ---- */
typedef enum CuadernuLanAuthOutcome {
    CU_LAN_AUTH_REJECTED = 0,
    CU_LAN_AUTH_CREATED = 1,
    CU_LAN_AUTH_RENEWED = 2,
    CU_LAN_AUTH_EXPIRED = 3
} CuadernuLanAuthOutcome;

typedef struct CuadernuLanWebAuth {
    char pairing_code[64];
    char active_token[64];
    double duration_sec;
    double expires_at_sec;
    uint64_t next_seq;
    bool has_session;
} CuadernuLanWebAuth;

void cuadernu_lan_auth_init(CuadernuLanWebAuth* auth, const char* pairing_code, double duration_sec);
CuadernuLanAuthOutcome cuadernu_lan_auth_open(
    CuadernuLanWebAuth* auth,
    const char* code_or_null,
    const char* cookie_or_null,
    const char* new_token,
    double now_sec,
    uint64_t* out_next_seq);
bool cuadernu_lan_auth_validate(
    const CuadernuLanWebAuth* auth,
    const char* cookie_token,
    double now_sec);

#ifdef __cplusplus
}
#endif
