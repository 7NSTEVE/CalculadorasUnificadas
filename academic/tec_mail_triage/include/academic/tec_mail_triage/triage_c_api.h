#pragma once
/*
 * academic/tec_mail_triage/triage_c_api.h
 * Dual-clean ISO C23 / ISO C++23 Spanish date/text normalization, TEC course
 * lexer, and NotebookLM extension classifier migrated from AppEstudio.Core (C#).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Normalizes UTF-8 text to lowercase ASCII without Spanish diacritics (a/e/i/o/u/n).
 * Returns written length (excluding NUL). */
size_t tec_normalize_es_ascii(const char* input, size_t len, char* out_buf, size_t out_cap);

/* Parses Spanish month name or abbreviation (enero/ene..diciembre/dic, incl. setiembre/set) -> 1..12, or 0. */
int tec_parse_spanish_month(const char* token, size_t len);

/* Exact Gregorian days in month (1..12) with leap-year handling; 0 if invalid. */
int tec_days_in_month(int year, int month);

/* Extracts effective lowercase extension handling TEC Digital's ".pdf-1" duplicate suffix.
 * Writes into out_ext (including leading '.') and returns length. */
size_t tec_notebooklm_real_ext(const char* filename, size_t len, char* out_ext, size_t out_cap);

/* Returns true if filename is compatible with NotebookLM (.pdf, .txt, .md, .mp3, incl. .pdf-1). */
bool tec_notebooklm_is_compatible(const char* filename, size_t len);

#ifdef __cplusplus
}
#endif
