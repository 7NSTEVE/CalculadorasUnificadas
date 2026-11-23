#pragma once

#include <cstdint>

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef int8_t Sint8;
typedef int16_t Sint16;
typedef int32_t Sint32;
typedef int64_t Sint64;

typedef int32_t SDL_Keycode;
#define SDLK_UNKNOWN 0

typedef struct SDL_Rect
{
    int x, y, w, h;
} SDL_Rect;

typedef struct SDL_Color
{
    Uint8 r, g, b, a;
} SDL_Color;

typedef struct SDL_Event
{
    Uint32 type;
} SDL_Event;
