/*
 * API en C del emulador de calculadora.
 *
 * Es la unica superficie que ve la app: Swift no toca C++ directamente. Todo lo que
 * hace falta es crear el emulador con una ROM, pulsar teclas, dejar correr el tiempo
 * y leer la pantalla.
 *
 * El emulador NO tiene hilo propio: es el anfitrion quien decide cuando avanza, con
 * calcemu_advance(). Eso encaja con el ciclo de dibujado de SwiftUI y evita tener que
 * sincronizar nada.
 */
#ifndef CALCEMU_H
#define CALCEMU_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CalcEmu CalcEmu;

/** Familias de hardware nX-U8 soportadas. */
enum
{
    CALCEMU_HW_ES_PLUS = 3,  /* fx-570ES PLUS, fx-570VN PLUS...  LCD 96x31  */
    CALCEMU_HW_CLASSWIZ = 4  /* fx-570/991LA X, fx-991CN X...    LCD 192x63 */
};

typedef struct
{
    int hardware_id;
    /**
     * 0 si la ROM se extrajo del emulador oficial de Casio (usa su interfaz de
     * teclado), 1 si es un volcado de la calculadora fisica. Acertar esto es
     * imprescindible: con el valor equivocado el firmware se queda colgado leyendo
     * registros que nadie mapea.
     */
    int real_hardware;
    /** Segmentos de memoria de codigo menos uno: 0x0001 = 128 KiB, 0x0003 = 256 KiB. */
    int csr_mask;
    int pd_value;
} CalcEmuModel;

/** Devuelve NULL si la ROM esta vacia o el modelo es incoherente. */
CalcEmu *calcemu_create(const CalcEmuModel *model,
                        const unsigned char *rom, size_t rom_size);
void calcemu_destroy(CalcEmu *emu);

/** Pulsa ON durante un intervalo emulado suficiente para que el firmware lo lea. */
void calcemu_power_on(CalcEmu *emu);

/** Deja correr el equivalente a `milliseconds` de tiempo real. */
void calcemu_advance(CalcEmu *emu, unsigned int milliseconds);

/**
 * Pulsa la tecla del codigo de matriz dado (nibble alto KO, nibble bajo KI).
 * Hay que soltarla con calcemu_key_up: mantenerla mas de medio segundo dispara la
 * auto-repeticion del firmware, igual que en la calculadora fisica.
 */
void calcemu_key_down(CalcEmu *emu, unsigned char matrix_code);
void calcemu_key_up(CalcEmu *emu);

/**
 * Ejecuta una pulsacion completa como una sola operacion del anfitrion:
 * hundir, avanzar `hold_milliseconds`, soltar y avanzar
 * `settle_milliseconds`.
 *
 * No crea hilos ni convierte el resto de la API en concurrente. El anfitrion
 * debe seguir serializando todas las llamadas de una misma instancia. Esta
 * funcion evita que una cola de entrada deje intercaladas o desparejadas las
 * primitivas key_down/key_up. Devuelve 1 si ejecuto la pulsacion y 0 si la
 * instancia no es valida.
 */
int calcemu_tap_key(CalcEmu *emu, unsigned char matrix_code,
                    unsigned int hold_milliseconds,
                    unsigned int settle_milliseconds);

/* --- pantalla --- */

int calcemu_screen_rows(const CalcEmu *emu);       /* filas de la matriz de puntos */
int calcemu_screen_width(const CalcEmu *emu);      /* pixeles de ancho             */
int calcemu_screen_row_size(const CalcEmu *emu);   /* bytes por fila en el buffer  */
int calcemu_screen_row_offset(const CalcEmu *emu); /* donde empieza la matriz      */
size_t calcemu_screen_buffer_size(const CalcEmu *emu);

/** Copia el buffer crudo del LCD. La fila 0 son los indicadores de estado. */
size_t calcemu_screen_buffer(const CalcEmu *emu, unsigned char *out, size_t out_size);

/** Copia la matriz ya expandida a 1 byte por pixel (0 o 1), fila a fila. */
size_t calcemu_screen_pixels(const CalcEmu *emu, unsigned char *out, size_t out_size);

int calcemu_screen_mode(const CalcEmu *emu);
int calcemu_screen_contrast(const CalcEmu *emu);

/**
 * Indica si hay algo nuevo que dibujar, y consume el aviso. Devuelve 1 como mucho
 * una vez por cambio, asi que sirve para no repintar de balde.
 */
int calcemu_take_redraw_flag(CalcEmu *emu);

/* --- instantaneas de estado --- */

/**
 * Guarda el estado completo de la maquina para poder volver a el al instante.
 *
 * Sin esto, retroceder obliga a reproducir todas las teclas desde el encendido, a
 * unos 20 ms por tecla: media hora de ejercicio son varios segundos de espera cada
 * vez que se quiere deshacer.
 *
 * Se llama dos veces: primero con out = NULL para saber cuanto ocupa, y despues con
 * un buffer de ese tamano. Devuelve los bytes necesarios (o escritos), o 0 si falla.
 * Si `out_size` se queda corto no escribe nada y devuelve el tamano que hacia falta.
 */
size_t calcemu_save_state(CalcEmu *emu, unsigned char *out, size_t out_size);

/**
 * Restaura un estado guardado. Devuelve 1 si se aplico.
 *
 * Devuelve 0 y deja la maquina INTACTA si el volcado no es de este modelo, de esta
 * misma ROM y de esta version del formato. Nunca aplica un volcado a medias: si algo
 * se tuerce despues de empezar, reinicia. Una calculadora medio restaurada calcularia
 * mal sin avisar, que es peor que no restaurar nada.
 */
int calcemu_load_state(CalcEmu *emu, const unsigned char *data, size_t size);

/* --- memoria de datos --- */

/**
 * Tamano de la RAM con bateria de este modelo, en bytes.
 *
 * Es donde el firmware guarda las variables A-F, X, Y, M y la pila de operaciones.
 * Leerla permite ensenar lo que la calculadora tiene DE VERDAD en cada memoria, en
 * vez de lo que la app cree recordar que se guardo.
 */
size_t calcemu_ram_size(const CalcEmu *emu);

/**
 * Copia la RAM. Devuelve los bytes copiados, o 0 si no cabe.
 *
 * Es de solo lectura a proposito: escribir aqui desde fuera saltaria por encima del
 * firmware y produciria un estado que la calculadora real no puede alcanzar.
 */
size_t calcemu_read_ram(const CalcEmu *emu, unsigned char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif
