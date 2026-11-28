/*
 * academic/tec_mail_triage/src/es_date_course_c23.c
 * Strict ISO C23 implementation (MSVC /std:clatest, Clang 21 -std=c23, GCC 16 -std=c23).
 */

#include "academic/tec_mail_triage/triage_c_api.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(__clang__)
#ifndef nullptr
#define nullptr ((void*)0)
#endif
#define TRIAGE_NODISCARD
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define TRIAGE_NODISCARD [[nodiscard]]
#else
#ifndef nullptr
#define nullptr ((void*)0)
#endif
#define TRIAGE_NODISCARD
#endif

static_assert(sizeof(char) == 1, "char must be 1 byte");

size_t tec_normalize_es_ascii(const char* input, size_t len, char* out_buf, size_t out_cap) {
    if (input == nullptr || out_buf == nullptr || out_cap == 0) {
        return 0;
    }
    size_t pos = 0;
    for (size_t i = 0; i < len && pos + 1 < out_cap; ++i) {
        unsigned char c = (unsigned char)input[i];
        if (c == 0xC3u && i + 1 < len) {
            unsigned char n = (unsigned char)input[++i];
            char mapped = '\0';
            switch (n) {
                case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85:
                case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5:
                    mapped = 'a'; break;
                case 0x88: case 0x89: case 0x8A: case 0x8B:
                case 0xA8: case 0xA9: case 0xAA: case 0xAB:
                    mapped = 'e'; break;
                case 0x8C: case 0x8D: case 0x8E: case 0x8F:
                case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                    mapped = 'i'; break;
                case 0x92: case 0x93: case 0x94: case 0x95: case 0x96:
                case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6:
                    mapped = 'o'; break;
                case 0x99: case 0x9A: case 0x9B: case 0x9C:
                case 0xB9: case 0xBA: case 0xBB: case 0xBC:
                    mapped = 'u'; break;
                case 0x91: case 0xB1:
                    mapped = 'n'; break;
                case 0x87: case 0xA7:
                    mapped = 'c'; break;
                default:
                    mapped = ' '; break;
            }
            out_buf[pos++] = mapped;
            continue;
        }
        if (c < 0x80u) {
            out_buf[pos++] = (char)tolower(c);
        } else if ((c & 0xC0u) == 0x80u) {
            /* Skip UTF-8 continuation bytes so each multibyte code point maps to one space. */
            continue;
        } else {
            out_buf[pos++] = ' ';
        }
    }
    out_buf[pos] = '\0';
    return pos;
}

int tec_parse_spanish_month(const char* token, size_t len) {
    if (token == nullptr || len == 0 || len >= 20) {
        return 0;
    }
    char low[24];
    size_t n = tec_normalize_es_ascii(token, len, low, sizeof(low));
    if (n == 0) return 0;

    if (strcmp(low, "enero") == 0 || strcmp(low, "ene") == 0) return 1;
    if (strcmp(low, "febrero") == 0 || strcmp(low, "feb") == 0) return 2;
    if (strcmp(low, "marzo") == 0 || strcmp(low, "mar") == 0) return 3;
    if (strcmp(low, "abril") == 0 || strcmp(low, "abr") == 0) return 4;
    if (strcmp(low, "mayo") == 0 || strcmp(low, "may") == 0) return 5;
    if (strcmp(low, "junio") == 0 || strcmp(low, "jun") == 0) return 6;
    if (strcmp(low, "julio") == 0 || strcmp(low, "jul") == 0) return 7;
    if (strcmp(low, "agosto") == 0 || strcmp(low, "ago") == 0) return 8;
    if (strcmp(low, "septiembre") == 0 || strcmp(low, "sep") == 0 ||
        strcmp(low, "sept") == 0 || strcmp(low, "setiembre") == 0 || strcmp(low, "set") == 0) return 9;
    if (strcmp(low, "octubre") == 0 || strcmp(low, "oct") == 0) return 10;
    if (strcmp(low, "noviembre") == 0 || strcmp(low, "nov") == 0) return 11;
    if (strcmp(low, "diciembre") == 0 || strcmp(low, "dic") == 0) return 12;
    return 0;
}

int tec_days_in_month(int year, int month) {
    static const int kDays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (year < 1900 || year > 3000 || month < 1 || month > 12) {
        return 0;
    }
    if (month == 2) {
        const bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return kDays[month - 1];
}

TRIAGE_NODISCARD static bool all_digits_nonempty(const char* s, size_t len) {
    if (len == 0) return false;
    for (size_t i = 0; i < len; ++i) {
        if (!isdigit((unsigned char)s[i])) return false;
    }
    return true;
}

size_t tec_notebooklm_real_ext(const char* filename, size_t len, char* out_ext, size_t out_cap) {
    if (filename == nullptr || out_ext == nullptr || out_cap == 0) {
        return 0;
    }
    out_ext[0] = '\0';
    if (len == 0) return 0;

    /* Find last path separator so dots in directory names are ignored */
    size_t base_start = 0;
    for (size_t i = len; i > 0; --i) {
        char c = filename[i - 1];
        if (c == '/' || c == '\\') {
            base_start = i;
            break;
        }
    }

    size_t dot = len;
    for (size_t i = len; i > base_start; --i) {
        if (filename[i - 1] == '.') {
            dot = i - 1;
            break;
        }
    }
    if (dot == len || dot == base_start) {
        return 0;
    }

    const char* ext_ptr = filename + dot;
    size_t ext_len = len - dot;

    /* Strip trailing "-<digits>" added by TEC Digital (e.g. "guia.pdf-1" -> ".pdf") */
    for (size_t i = ext_len; i > 1; --i) {
        if (ext_ptr[i - 1] == '-') {
            if (all_digits_nonempty(ext_ptr + i, ext_len - i)) {
                ext_len = i - 1;
            }
            break;
        }
    }

    size_t written = 0;
    for (size_t i = 0; i < ext_len && written + 1 < out_cap; ++i) {
        out_ext[written++] = (char)tolower((unsigned char)ext_ptr[i]);
    }
    out_ext[written] = '\0';
    return written;
}

bool tec_notebooklm_is_compatible(const char* filename, size_t len) {
    char ext[32];
    const size_t n = tec_notebooklm_real_ext(filename, len, ext, sizeof(ext));
    if (n == 0) return false;
    return strcmp(ext, ".pdf") == 0 ||
           strcmp(ext, ".txt") == 0 ||
           strcmp(ext, ".md") == 0 ||
           strcmp(ext, ".mp3") == 0;
}
