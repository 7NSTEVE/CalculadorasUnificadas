/*
 * Sustituto minimo de SDL para el nucleo del emulador.
 *
 * El nucleo de CasioEmu (CPU, MMU, interrupciones y la mayoria de perifericos) no usa
 * SDL para nada real: solo necesita sus typedefs de enteros, SDL_Rect para la
 * informacion de sprites, y el tipo SDL_Event porque aparece en la firma de un metodo
 * virtual. Con esta cabecera todos esos ficheros compilan tal cual, sin tocarlos, que
 * es justo lo que interesa: la logica de calculo ya esta validada y no quiero rozarla.
 */
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

/** Rectangulo. Lo usa SpriteInfo para describir zonas de la caratula. */
typedef struct SDL_Rect
{
    int x, y, w, h;
} SDL_Rect;

typedef struct SDL_Color
{
    Uint8 r, g, b, a;
} SDL_Color;

/**
 * Evento de interfaz.
 *
 * En la version de escritorio esto era la union de eventos de SDL. Aqui la entrada
 * entra por la API en C (calcemu_key_down / calcemu_key_up), asi que este tipo solo
 * existe para que las firmas virtuales `UIEvent(SDL_Event&)` sigan compilando.
 * Nunca se despacha ninguno.
 */
typedef struct SDL_Event
{
    Uint32 type;
} SDL_Event;
