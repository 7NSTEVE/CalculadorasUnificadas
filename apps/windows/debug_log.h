#pragma once
#include <windows.h>
#include <stdio.h>

inline void log_msg(const char* msg) {
    char path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "calc_debug.log");
    FILE* f = nullptr;
    if (fopen_s(&f, path, "a") == 0 && f) { fprintf(f, "%s\n", msg); fclose(f); }
}
