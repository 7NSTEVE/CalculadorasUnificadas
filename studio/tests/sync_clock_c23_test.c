/*
 * studio/tests/sync_clock_c23_test.c
 * Strict ISO C23 test verifying migrated iPad state machines
 * (RelojNotaViva, SecuenciaCommitAutoguardado, EstadoAutorizacionWebIPad).
 */

#include "../c_api/cuadernu_sync_clock_c23.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(__clang__)
#ifndef nullptr
#define nullptr ((void*)0)
#endif
#endif

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        ++g_pass;
        printf("[PASS] %s\n", name);
    } else {
        ++g_fail;
        fprintf(stderr, "[FAIL] %s\n", name);
    }
}

static bool near(double a, double b) {
    return fabs(a - b) < 1e-6;
}

typedef struct CommitOrderCtx {
    int step;
    int journal_order;
    int canonical_order;
    bool fail_journal;
} CommitOrderCtx;

static bool mock_journal(void* ctx, const uint8_t* data, size_t len) {
    CommitOrderCtx* c = (CommitOrderCtx*)ctx;
    (void)data; (void)len;
    c->journal_order = ++c->step;
    return !c->fail_journal;
}

static bool mock_canonical(void* ctx, const uint8_t* data, size_t len) {
    CommitOrderCtx* c = (CommitOrderCtx*)ctx;
    (void)data; (void)len;
    c->canonical_order = ++c->step;
    return true;
}

int main(void) {
    /* 1. RelojNotaViva exact parity with RelojNotaVivaMain.swift */
    CuadernuLiveNoteClock clk;
    cuadernu_clock_init(&clk);
    check(near(cuadernu_clock_elapsed_at(&clk, 50.0), 0.0), "clock_stopped_starts_zero");
    check(cuadernu_clock_start(&clk, 10.0), "clock_start_accepts_first");
    check(!cuadernu_clock_start(&clk, 11.0), "clock_start_rejects_second");
    check(near(cuadernu_clock_elapsed_at(&clk, 12.5), 2.5), "clock_advances_recording");

    double p1 = -1.0;
    check(cuadernu_clock_pause(&clk, 14.0, &p1) && near(p1, 4.0), "clock_pause_closes_segment");
    check(!cuadernu_clock_pause(&clk, 15.0, &p1), "clock_second_pause_rejected");
    check(near(cuadernu_clock_elapsed_at(&clk, 99.0), 4.0), "clock_long_pause_no_drift");

    check(cuadernu_clock_resume(&clk, 100.0), "clock_resume_after_pause");
    check(!cuadernu_clock_resume(&clk, 101.0), "clock_double_resume_rejected");
    check(near(cuadernu_clock_elapsed_at(&clk, 103.0), 7.0), "clock_adds_second_segment");

    double p2 = -1.0;
    check(cuadernu_clock_pause(&clk, 105.0, &p2) && near(p2, 9.0), "clock_second_pause_9s");
    check(near(cuadernu_clock_stop(&clk, 1000.0), 9.0), "clock_stop_while_paused_no_gap");
    check(near(cuadernu_clock_elapsed_at(&clk, 2000.0), 9.0), "clock_final_stable");

    /* 2. SecuenciaCommitAutoguardado */
    const uint8_t payload[4] = { 'n', 'o', 't', 'a' };
    CommitOrderCtx ok_ctx = { 0, 0, 0, false };
    check(cuadernu_autosave_commit(payload, 4, mock_journal, mock_canonical, &ok_ctx) == CU_COMMIT_OK &&
              ok_ctx.journal_order == 1 && ok_ctx.canonical_order == 2,
          "autosave_journal_before_canonical");

    CommitOrderCtx fail_ctx = { 0, 0, 0, true };
    check(cuadernu_autosave_commit(payload, 4, mock_journal, mock_canonical, &fail_ctx) == CU_COMMIT_JOURNAL_FAILED &&
              fail_ctx.journal_order == 1 && fail_ctx.canonical_order == 0,
          "autosave_aborts_canonical_on_journal_fail");

    /* 3. EstadoAutorizacionWebIPad */
    CuadernuLanWebAuth auth;
    cuadernu_lan_auth_init(&auth, "qr-uno", 60.0);
    uint64_t seq = 0;
    check(cuadernu_lan_auth_open(&auth, "qr-dos", nullptr, "tok-a", 1700000000.0, &seq) == CU_LAN_AUTH_REJECTED,
          "lan_auth_rejects_bad_qr");
    check(cuadernu_lan_auth_open(&auth, "qr-uno", nullptr, "tok-a", 1700000000.0, &seq) == CU_LAN_AUTH_CREATED && seq == 1,
          "lan_auth_creates_session_seq1");
    check(cuadernu_lan_auth_validate(&auth, "tok-a", 1700000030.0), "lan_auth_valid_within_window");
    check(!cuadernu_lan_auth_validate(&auth, "tok-a", 1700000065.0), "lan_auth_expired_after_window");
    check(cuadernu_lan_auth_open(&auth, nullptr, "tok-a", "tok-b", 1700000065.0, &seq) == CU_LAN_AUTH_EXPIRED,
          "lan_auth_open_detects_expired_cookie");

    printf("SUMMARY: passed=%d failed=%d\n", g_pass, g_fail);
    if (g_fail == 0) {
        printf("SYNC_CLOCK_C23_TESTS_PASS\n");
        return 0;
    }
    return 1;
}
