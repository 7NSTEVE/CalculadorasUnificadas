#pragma once
/*
 * academic/matricula_tec/matricula_c_api.h
 * Dual-clean ISO C23 / ISO C++23 low-level codec and schedule primitives
 * for official ITCR Matricula TEC protocol.
 */

#include <stddef.h>
#include <stdint.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Strict RFC 3986 percent-encoding (encodes +, /, =, $, &, etc.).
 * Returns required bytes (excluding NUL). If out_buf != NULL and out_cap > required,
 * writes NUL-terminated encoded string. */
size_t matricula_strict_percent_encode(
    const char* input,
    size_t input_len,
    char* out_buf,
    size_t out_cap);

/* Normalizes TEC day token ("L","K","M","J","V","S","1".."6","Lunes".."Sabado") -> 0..5, or -1. */
int matricula_normalize_tec_day(const char* raw_day, size_t len);

/* Converts day index 0..5 -> "L","K","M","J","V","S" (or "" if out of range). */
const char* matricula_day_index_to_code(int day_index);

/* Parses "HH:MM" or "H:MM" or "HH:MM:SS" -> minutes since 00:00 (0..1439), or -1 on error. */
int matricula_parse_time_minutes(const char* raw_time, size_t len);

/* Returns true if two half-open intervals [s1, e1) and [s2, e2) on same day overlap. */
bool matricula_intervals_overlap(int day1, int start1, int end1,
                                 int day2, int start2, int end2);

#ifdef __cplusplus
}
#endif
