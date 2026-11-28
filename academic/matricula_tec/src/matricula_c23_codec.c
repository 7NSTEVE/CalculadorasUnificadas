/*
 * academic/matricula_tec/src/matricula_c23_codec.c
 * Strict ISO C23 portable implementation (MSVC /std:clatest, Clang 21 -std=c23, GCC 16 -std=c23).
 */

#include "academic/matricula_tec/matricula_c_api.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(__clang__)
#ifndef nullptr
#define nullptr ((void*)0)
#endif
#define MT_NODISCARD
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define MT_NODISCARD [[nodiscard]]
#else
#ifndef nullptr
#define nullptr ((void*)0)
#endif
#define MT_NODISCARD
#endif

static_assert(sizeof(unsigned char) == 1, "Byte size must be 1");

MT_NODISCARD static bool is_rfc3986_unreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

size_t matricula_strict_percent_encode(
    const char* input,
    size_t input_len,
    char* out_buf,
    size_t out_cap)
{
    static const char kHex[16] = {
        '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
    };
    if (input == nullptr && input_len > 0) {
        return 0;
    }

    size_t needed = 0;
    for (size_t i = 0; i < input_len; ++i) {
        unsigned char c = (unsigned char)input[i];
        needed += is_rfc3986_unreserved(c) ? 1u : 3u;
    }

    if (out_buf != nullptr && out_cap > needed) {
        size_t pos = 0;
        for (size_t i = 0; i < input_len; ++i) {
            unsigned char c = (unsigned char)input[i];
            if (is_rfc3986_unreserved(c)) {
                out_buf[pos++] = (char)c;
            } else {
                out_buf[pos++] = '%';
                out_buf[pos++] = kHex[(c >> 4u) & 0x0Fu];
                out_buf[pos++] = kHex[c & 0x0Fu];
            }
        }
        out_buf[pos] = '\0';
    }
    return needed;
}

int matricula_normalize_tec_day(const char* raw_day, size_t len) {
    if (raw_day == nullptr || len == 0) {
        return -1;
    }
    while (len > 0 && isspace((unsigned char)*raw_day)) {
        ++raw_day;
        --len;
    }
    while (len > 0 && isspace((unsigned char)raw_day[len - 1])) {
        --len;
    }
    if (len == 0) {
        return -1;
    }

    char up[16];
    size_t n = 0;
    for (size_t i = 0; i < len && n + 1 < sizeof(up); ++i) {
        unsigned char c = (unsigned char)raw_day[i];
        /* Handle UTF-8 accented Spanish letters É (C3 89/A9), Á (C3 81/A1) cleanly */
        if (c == 0xC3u && i + 1 < len) {
            unsigned char next = (unsigned char)raw_day[++i];
            if (next == 0x89u || next == 0xA9u) { up[n++] = 'E'; continue; }
            if (next == 0x81u || next == 0xA1u) { up[n++] = 'A'; continue; }
            continue;
        }
        up[n++] = (char)toupper(c);
    }
    up[n] = '\0';

    if (strcmp(up, "L") == 0 || strcmp(up, "1") == 0 || strcmp(up, "LUN") == 0 || strcmp(up, "LUNES") == 0) return 0;
    if (strcmp(up, "K") == 0 || strcmp(up, "2") == 0 || strcmp(up, "MAR") == 0 || strcmp(up, "MARTES") == 0) return 1;
    if (strcmp(up, "M") == 0 || strcmp(up, "3") == 0 || strcmp(up, "MIE") == 0 || strcmp(up, "MIERCOLES") == 0) return 2;
    if (strcmp(up, "J") == 0 || strcmp(up, "4") == 0 || strcmp(up, "JUE") == 0 || strcmp(up, "JUEVES") == 0) return 3;
    if (strcmp(up, "V") == 0 || strcmp(up, "5") == 0 || strcmp(up, "VIE") == 0 || strcmp(up, "VIERNES") == 0) return 4;
    if (strcmp(up, "S") == 0 || strcmp(up, "6") == 0 || strcmp(up, "SAB") == 0 || strcmp(up, "SABADO") == 0) return 5;
    if (strcmp(up, "D") == 0 || strcmp(up, "7") == 0 || strcmp(up, "DOM") == 0 || strcmp(up, "DOMINGO") == 0) return 6;
    return -1;
}

const char* matricula_day_index_to_code(int day_index) {
    switch (day_index) {
        case 0: return "L";
        case 1: return "K";
        case 2: return "M";
        case 3: return "J";
        case 4: return "V";
        case 5: return "S";
        case 6: return "D";
        default: return "";
    }
}

int matricula_parse_time_minutes(const char* raw_time, size_t len) {
    if (raw_time == nullptr || len < 4) {
        return -1;
    }
    while (len > 0 && isspace((unsigned char)*raw_time)) {
        ++raw_time;
        --len;
    }
    while (len > 0 && isspace((unsigned char)raw_time[len - 1])) {
        --len;
    }

    int hour = 0;
    int hour_digits = 0;
    size_t i = 0;
    while (i < len && isdigit((unsigned char)raw_time[i]) && hour_digits < 2) {
        hour = hour * 10 + (raw_time[i] - '0');
        ++hour_digits;
        ++i;
    }
    if (hour_digits == 0 || i >= len || raw_time[i] != ':') {
        return -1;
    }
    ++i;

    int minute = 0;
    int min_digits = 0;
    while (i < len && isdigit((unsigned char)raw_time[i]) && min_digits < 2) {
        minute = minute * 10 + (raw_time[i] - '0');
        ++min_digits;
        ++i;
    }
    if (min_digits != 2 || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return -1;
    }
    return hour * 60 + minute;
}

bool matricula_intervals_overlap(int day1, int start1, int end1,
                                 int day2, int start2, int end2)
{
    if (day1 < 0 || day2 < 0 || day1 != day2) {
        return false;
    }
    if (end1 <= start1 || end2 <= start2) {
        return false;
    }
    return (start1 < end2) && (start2 < end1);
}
