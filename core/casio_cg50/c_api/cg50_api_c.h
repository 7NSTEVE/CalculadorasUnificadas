/* ==========================================================================
 * cg50_api_c.h — ABI C del "Core Propio" fx-CG50 (CPU SH7305 / SH-4A)
 * Proyecto: Calculadora Unificada — edición nativa (CG50_ENGINE=native)
 *
 * ESTADO: ARTEFACTO GUÍA (SPEC/DOCS ONLY, fase F1 del plan de producto).
 * Ningún target de build lo incluye todavía; ningún shim lo implementa aún.
 * La interfaz replica el motor NATIVO (Cg50Backend implementado en
 * third_party/rom-native-reference/rama-cg50_backend.cpp por encima de
 * Cg50NativeCore) y EXCLUYE por diseño la edición GCal (DLLs 32-bit CASIO).
 *
 * Origen: PROPUESTA-INTEGRACION.md (D:\Temp\opencode\producto-integracion\,
 * sha16 57241823EC1E9149) sección 5; firmas de esa propuesta reproducidas
 * EXACTAS. Nombre de fichero `cg50_api_c.h` fijado por mandato del dueño
 * (la propuesta lo llamaba `cg50core_c_api.h`); guard interno: CG50_API_C_H.
 *
 * REGLAS NORMATIVAS DE LA ABI (de la propuesta, §5):
 *   1. Nada cruza excepciones C++: todo resultado va por valores o códigos
 *      (0 = éxito). El shim envuelve llamadas C++ con try/catch y las
 *      traduce a códigos; jamás deja propagar una excepción a C.
 *   2. La instancia se crea con create() y se libera con destroy(). Ninguna
 *      función ejecuta asignaciones después de destroy(); el handle
 *      destruido es inválido y su uso es UB (precondición del host).
 *   3. Todas las llamadas a UNA misma instancia ocurren desde UN único hilo
 *      del host (hoy: el hilo de la interfaz). El modelo de hilos (F2) lo
 *      implementa el host: la ABI misma no lleva threading.
 *   4. Presupuesto determinista de tick: no depende del reloj de pared. El
 *      presupuesto interior genera `µs×60` instrucciones (SH7305 ≈ 58 MHz)
 *      con tope 2 500 000, salvo que el host pase un máximo explícito.
 *      Garantía de paridad de probe: los contratos NOP-ROM y FR2 deben
 *      pasar por la ABI byte a byte, igual que por el C++ directo.
 *   5. La ABI NUNCA lee variables de entorno ni descubre ficheros. Las
 *      opciones viajan por cg50core_set_option; los bytes de ROM los
 *      entrega el host por puntero+tamaño (rutas y knobs CG50_* viven en
 *      el lado host).
 *   6. Framebuffer BGRA32 crudo: sin JPEG/PNG, sin codificación y sin
 *      transformaciones de geometría dentro de la ABI.
 *   7. save/restore/user-rom usan el protocolo de DOS llamadas (dst=NULL ⇒
 *      se devuelve el tamaño requerido); restore es todo-o-nada.
 *   8. La ABI no escribe ficheros ni logs por efecto lateral: la UI decide
 *      destino y rotación de cualquier salida.
 *   9. La entrada de tecla es por código de matriz 16 bits (nibble alto
 *      KO, nibble bajo KI): contrato de 50 claves únicas con F1 = 0x070A
 *      (cg50_keys_exact.h). NO hay mapeo de teclas virtuales en la ABI.
 *  10. Las opciones desconocidas fallan cerradas: no se aplica nada y se
 *      devuelve error (la instancia queda igual que antes de la llamada).
 *
 * Todo lo relativo al motor queda documentado junto a cada función; el
 * mapeado completo ABI↔Cg50Backend vive al final de este fichero.
 * ========================================================================== */

/**
 * @file
 * @brief ABI C del Core Propio fx-CG50 (SH7305): ciclo de vida, tick
 *        determinista, framebuffer BGRA32, teclado por matriz, telemetría,
 *        snapshot save/load (dos llamadas) y persistencia opt-in del UserROM.
 * @date 2026-10-29
 * @version ABI 0.1.0 (F1, SPEC — sin build integrado)
 * @note Cabecera STRICTAMENTE C: válida en C99/C11 y en C++ bajo `extern "C"`.
 *       Sin excepciones, sin allocations en la ABI, sin dependencias más
 *       allá de <stddef.h>/<stdint.h>, sin #pragma pack (ver asserts de
 *       layout al final) y sin partes específicas de Windows/POSIX.
 */

#ifndef CG50_API_C_H
#define CG50_API_C_H

#include <stddef.h>
#include <stdint.h>

/* Enlazado: por defecto (nada) es enlazado estático; CG50CORE_BUILD +
 * CG50CORE_SHARED exportan el símbolo (.dll/.so); CG50CORE_SHARED sin
 * CG50CORE_BUILD lo importa. */
#if defined(CG50CORE_SHARED)
#  if defined(_WIN32)
#    if defined(CG50CORE_BUILD)
#      define CG50CORE_API __declspec(dllexport)
#    else
#      define CG50CORE_API __declspec(dllimport)
#    endif
#  else
#    define CG50CORE_API __attribute__((visibility("default")))
#  endif
#else
#  define CG50CORE_API /* enlazado estático */
#endif

/* ==========================================================================
 * Versionado estructurado de la ABI
 * ==========================================================================
 * Semántica: MAJOR rompe la compatibilidad al subir; MINOR añade funciones
 * compatibles (F2: cg50core_abi_version, cg50core_diagnostic_utf16,
 * cg50core_set_frame_callback); PATCH corrige sin cambio de semántica ni
 * de blob. */
#define CG50CORE_ABI_VERSION_MAJOR 0u
#define CG50CORE_ABI_VERSION_MINOR 1u
#define CG50CORE_ABI_VERSION_PATCH 0u

#define CG50CORE_ABI_MAKE_VERSION(major, minor, patch)                 \
    ((uint32_t)((((uint32_t)(major)) << 16) |                          \
                (((uint32_t)(minor)) << 8)  |                          \
                ((uint32_t)(patch))))

/** ABI de esta cabecera empaquetada (May<<16 | Men<<8 | Parche):
 *  0.1.0 => 0x00000100. Corrección documental de F1 (2026-10-30): el valor
 *  citado antes (0x00010000) no correspondía a la fórmula empaquetada del
 *  propio encabezado; lo detectó el TU C99 (cg50_api_cssert.c) al compilar. */
#define CG50CORE_ABI_VERSION \
    CG50CORE_ABI_MAKE_VERSION(CG50CORE_ABI_VERSION_MAJOR, \
                              CG50CORE_ABI_VERSION_MINOR, \
                              CG50CORE_ABI_VERSION_PATCH)

/* ==========================================================================
 * Constantes de contrato (valores verificados en las fuentes del core)
 * ========================================================================== */
#define CG50CORE_FRAMEBUFFER_WIDTH        384u   /**< Panel LCD (R61524). */
#define CG50CORE_FRAMEBUFFER_HEIGHT       216u
#define CG50CORE_FRAMEBUFFER_PIXELS       (384u * 216u)   /* 82944.        */
#define CG50CORE_FRAMEBUFFER_STRIDE_BYTES (384u * 4u)     /* 1536.         */
/* BGRA32 crudo para el host: la ABI no premultiplica ni convierte al
 * formato de render; eso lo hace el host. */
#define CG50CORE_PIXEL_FORMAT_BGRA32      0u

/* kStateVersion del blob (chunk 1); restore rechaza otra versión
 * (fail-closed). */
#define CG50CORE_STATE_VERSION            2u

/* Tamaños exactos de ROM, en bytes (cg50_native_core.h):
 * SystemROM 0x00B60000 (11 927 552), UserROM 0x014A0000, C2ROM 0x004A0000.
 * La ROM combinada (System||User, 0x02000000) NO se acepta como tal en la
 * ABI: el host entrega cada parte por separado (LoadFirmwareParts). */
#define CG50CORE_SYSTEM_ROM_SIZE          0x00B60000u
#define CG50CORE_USER_ROM_SIZE            0x014A0000u
#define CG50CORE_C2_ROM_SIZE              0x004A0000u

/* Teclado (cg50_keys_exact.h): 50 códigos únicos; F1 = 0x070A. */
#define CG50CORE_KEY_COUNT                50u
#define CG50CORE_KEY_FIRST_F1             0x070Au

/* Presupuesto por tick (InstructionBudgetForTick): min(µs×60, 2 500 000)
 * instrucciones; tope interior de la ABI. */
#define CG50CORE_BUDGET_SCALE             60u
#define CG50CORE_BUDGET_TICK_CAP          2500000u

/* ==========================================================================
 * Manejador opaco
 * ========================================================================== */
/** Instancia del core (Cg50Backend nativo bajo el shim). Opaca: solo se
 *  manipula con las funciones de esta ABI. El handle NO es copiable y su
 *  validez muere en cg50core_destroy(). */
typedef struct Cg50Core Cg50Core;

/* ==========================================================================
 * Códigos de error y de estado (enum)
 * ========================================================================== */
/** Dominio de resultados de la ABI. Toda función que retorna `int` devuelve
 *  exclusivamente valores de este enum o, si es de copia/tamaño, un
 *  recuento no-negativo documentado en su @return. */
typedef enum Cg50Result {
    CG50CORE_OK            =  0, /**< Éxito.                               */
    CG50CORE_ERR_ARG       = -1, /**< Argumento inválido (NULL, tamaño de
                                      ROM no exacto, opción desconocida,
                                      instancia incoherente).              */
    CG50CORE_ERR_NOT_READY = -2, /**< El core no tiene firmware cargado
                                      (operación que exige estado Ready).  */
    CG50CORE_ERR_CAPACITY  = -3, /**< Buffer del host insuficiente: NADA se
                                      escribió; repetir con el tamaño
                                      reportado.                           */
    CG50CORE_ERR_FAULTED   = -4, /**< El core quedó detenido (opcode no
                                      soportado / fallo de memoria). La
                                      instancia sigue consultable para
                                      diagnóstico, pero no es reusable
                                      salvo reset o restore.               */
    CG50CORE_ERR_STATE     = -5  /**< Blob de snapshot incompatible
                                      (magic, kStateVersion, chunk
                                      ilegible). Solo restore lo devuelve. */
} Cg50Result;

/** Estado de máquina de producto (mapa de Cg50Backend::State). */
typedef enum Cg50MachineState {
    CG50CORE_MACHINE_UNAVAILABLE = 0, /* Sin firmware (tras create; el
                                         reset nativo NO lo produce).   */
    CG50CORE_MACHINE_READY       = 1, /* OS listo para operar.            */
    CG50CORE_MACHINE_FAULTED     = 2  /* Detenido por error insalvable;
                                         ver cg50core_last_error().      */
} Cg50MachineState;

/** Estado de la CPU del core (mapa de Cg50Backend::CpuState). */
typedef enum Cg50CoreCpuState {
    CG50CORE_CPU_EMPTY    = 0, /**< Aún sin CPU montada (sin firmware).      */
    CG50CORE_CPU_RUNNING  = 1, /**< Ejecutando instrucciones normales.       */
    CG50CORE_CPU_SLEEPING = 2, /**< En SLEEP (espera de interrupciones;
                                    avanza periféricos SOLO con la opción
                                    CG50CORE_OPT_SLEEP_ADVANCE activa).    */
    CG50CORE_CPU_HALTED   = 3  /* Instrucción no soportada / decisión
                                   fail-closed: la máquina no continúa.   */
} Cg50CoreCpuState;

/* ==========================================================================
 * Opciones (set_option)
 * ========================================================================== */
/** Opciones de comportamiento opt-in. Valor: 0 = OFF, != 0 = ON.
 *  Defaults de la ABI: todos OFF (el build de escritorio y los probes
 *  deterministas dependen de ello). El shell host decide qué activa. */
typedef enum Cg50Option {
    /** Avanzar TMU/CMT/WDT/RTC mientras la CPU está en SLEEP (N-12).
     *  OFF (default): el tiempo no avanza en SLEEP — bit-paridad de los
     *  probes. ON: la GUI en móviles no se congela cuando el OS entra en
     *  SLEEP. */
    CG50CORE_OPT_SLEEP_ADVANCE = 1,
    /** Reloj RTC virtual del core (SetRtcVirtualClockEnabled). OFF
     *  (default): determinismo preservado. ON: los resultados del probe
     *  dejan de ser bit-comparables tras el arranque — solo productos
     *  móviles, no en golden runs. */
    CG50CORE_OPT_RTC_VIRTUAL   = 2
} Cg50Option;

/* ==========================================================================
 * Estructuras de datos (cero bitfields, cero wchar_t, padding natural)
 * ========================================================================== */
/** Snapshot de telemetría (mapa de Cg50Backend::RuntimeStatus). */
typedef struct Cg50Status {
    uint8_t  cpuState;            /**< Cg50CoreCpuState (0..3).             */
    uint8_t  systemRomLoaded;     /**< 0/1: SystemROM válida cargada.       */
    uint8_t  userRomLoaded;       /**< 0/1: UserROM cargada (exacta).       */
    uint8_t  framebufferAvailable;/* 0/1: R61524 recibió GRAM real (no se
                                     sintetiza pantalla).                 */
    uint8_t  faulted;             /**< 1 si Cg50Backend::State==Faulted.    */
    uint32_t pc;                  /**< PC actual (dirección lógica).        */
    uint32_t sr;                  /**< Registro de estado SH-4A.            */
    uint32_t lastExceptionEvent;  /**< INTEVT/última excepción (0 si no).   */
    uint32_t lastInterruptEvent;  /**< Último evento de interrupción.       */
    uint64_t instructions;        /**< Instrucciones ejecutadas (total).    */
    uint64_t cycles;              /**< Ciclos CPU acumulados.               */
    uint64_t exceptionsTaken;     /**< Excepciones aceptadas.               */
    uint64_t interruptsAccepted;  /**< Interrupciones aceptadas.            */
    uint8_t  unsupportedOpcodeSeen; /**< 0/1: opcode no soportado detectado */
    uint32_t unsupportedPc;       /**< PC del opcode no soportado.          */
    uint16_t unsupportedOpcode;   /**< Opcode que la máquina no decodificó. */
    uint8_t  memoryFaultSeen;     /**< 0/1: fallo de memoria (fail-closed). */
    uint8_t  lastMemoryFaultWasWrite; /**< 0/1: el fallo fue de escritura.  */
    uint32_t lastMemoryFaultAddress;  /**< Dirección del fallo de memoria. */
} Cg50Status;

/** Descripción del framebuffer actual (mapa de Cg50Backend::FramebufferView;
 *  la ABI jamás entrega punteros prestados del core: vista por copia). */
typedef struct Cg50FrameInfo {
    uint32_t width;        /**< 384 en producción.                        */
    uint32_t height;       /**< 216 en producción.                        */
    uint32_t strideBytes;  /**< 1536 = width*4 (BGRA32 empaquetado).      */
    uint32_t pixelFormat;  /**< CG50CORE_PIXEL_FORMAT_BGRA32.             */
} Cg50FrameInfo;

/* --- Asserts de layout: contrato de marshalling binario (JNI F2) -------- */
#if defined(__cplusplus)
#  define CG50CORE_STATIC_ASSERT(cond, name) static_assert((cond), #cond)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define CG50CORE_STATIC_ASSERT(cond, name) _Static_assert((cond), #cond)
#else
#  define CG50CORE_STATIC_ASSERT(cond, name) \
     typedef char cg50_assert_##name[(cond) ? 1 : -1]
#endif

CG50CORE_STATIC_ASSERT(sizeof(Cg50Status) == 72, Cg50StatusSize);
CG50CORE_STATIC_ASSERT(offsetof(Cg50Status, pc) == 8, Cg50StatusPc);
CG50CORE_STATIC_ASSERT(offsetof(Cg50Status, instructions) == 24, Cg50StatusInstr);
CG50CORE_STATIC_ASSERT(offsetof(Cg50Status, unsupportedPc) == 60, Cg50StatusUnPc);
CG50CORE_STATIC_ASSERT(offsetof(Cg50Status, lastMemoryFaultAddress) == 68,
                       Cg50StatusMemFault);
CG50CORE_STATIC_ASSERT(sizeof(Cg50FrameInfo) == 16, Cg50FrameInfoSize);

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Ciclo de vida
 * ========================================================================== */

/**
 * @brief Crea una instancia del core sin firmware (estado UNAVAILABLE).
 * @return Handle nuevo, o NULL si no hubo memoria. Nunca lanza.
 * @note Solo asigna en create(). No fabrica pantalla, no toca ficheros.
 */
CG50CORE_API Cg50Core* cg50core_create(void);

/**
 * @brief Destruye la instancia y libera TODA memoria derivada.
 * @param core Handle (NULL es no-op seguro). Tras la llamada, inválido.
 * @note Ninguna función posterior ejecuta allocations; el handle no es
 *       reusable (para re-usar, cg50core_create de nuevo).
 */
CG50CORE_API void cg50core_destroy(Cg50Core* core);

/**
 * @brief Carga el universo de arranque desde punteros del host (init).
 *
 * Mapa del backend: Cg50Backend::Initialize + LoadFirmwareParts del core.
 * OJO: los bytes son COPIADOS por la instancia (el host puede liberar sus
 * buffers al volver). Doble llamada permitida (re-carga controlada, N-9):
 * el estado se reconstruye desde el nuevo firmware; las opciones fijadas
 * con set_option son sticky al handle y NO se reinician.
 *
 * @param core       Handle de la instancia.
 * @param system     SystemROM RAW, requerida, EXACTA 0x00B60000 bytes. La
 *                   variante combinada (0x02000000) NO se acepta como un
 *                   solo buffer aquí: fail-closed.
 * @param system_len Tamaño de system (bytes).
 * @param user       UserROM RAW, opcional (NULL si el host no la carga):
 *                   si no-NULL, EXACTA 0x014A0000 bytes.
 * @param user_len   Tamaño de user (bytes).
 * @param c2         C2ROM RAW, opcional (NULL): 0x004A0000 bytes exactos.
 * @param c2_len     Tamaño de c2 (bytes).
 * @return CG50CORE_OK; CG50CORE_ERR_ARG (tamaños no exactos / NULL
 *         system); CG50CORE_ERR_FAULTED si el firmware es inadmisible
 *         (p.ej. contenedor DLSim): el core queda Faulted con diagnóstico.
 * @note La ABI NO descubre ficheros ni lee env (regla 5): el auto-arranque
 *       de C2ROM junto al UserROM y el emparejamiento por nombre de la
 *       variante fichero NO aplican aquí; el host los materializa.
 */
CG50CORE_API int cg50core_load_firmware_raw(Cg50Core* core,
                                            const uint8_t* system,
                                            size_t system_len,
                                            const uint8_t* user,
                                            size_t user_len,
                                            const uint8_t* c2,
                                            size_t c2_len);

/**
 * @brief RESET de la CPU conservando la placa (mapa: Cg50Backend::Reset).
 *
 * Edición nativa: ResetCpuState — reinicia PC/SR/registros a
 * kRomBase/kResetSr; conserva SDRAM/periféricos/teclado; la máquina vuelve
 * a Ready. No devuelve valor: la incidencia se lee con cg50core_state()
 * y cg50core_status().
 *
 * @param core Handle de la instancia. NULL es no-op por seguridad.
 */
CG50CORE_API void cg50core_reset_cpu(Cg50Core* core);

/**
 * @brief Aplica una opción opt-in (fail-closed: nada se aplica si falla).
 * @param core   Handle de la instancia.
 * @param option Cg50Option (SLEEP_ADVANCE o RTC_VIRTUAL).
 * @param value  0 = OFF; != 0 = ON.
 * @return CG50CORE_OK, o CG50CORE_ERR_ARG si la opción es desconocida o el
 *         core es NULL (la instancia queda SIN cambio — regla 10).
 * @note Las opciones son sticky al handle (sobreviven a re-carga de
 *       firmware) y son reconfigurables en cualquier estado.
 */
CG50CORE_API int cg50core_set_option(Cg50Core* core,
                                     uint32_t option, uint32_t value);

/* ==========================================================================
 * Avance determinista (tick)
 * ========================================================================== */

/**
 * @brief Avanza el core `elapsed_us` de tiempo simulado
 *        (mapa: Cg50Backend::Step).
 *
 * PRESUPUESTO INTERIOR: `min(elapsed_us×60, 2 500 000)` instrucciones
 * (SH7305 ≈ 58 MHz), idéntico a InstructionBudgetForTick del backend. Con
 * max_instructions != 0, ese tope del host SUSTITUYE al presupuesto
 * automático. El core NO tiene hilo: quien avanza es el host; cero
 * dependencia del reloj de pared.
 *
 * @param core             Handle de la instancia.
 * @param elapsed_us       Tiempo simulado transcurrido (microsegundos).
 * @param max_instructions Tope duro de instrucciones (0 = presupuesto
 *                         automático interno de la ABI).
 * @note void a propósito: un tick puede detener la máquina (fault); la
 *       incidencia queda en cg50core_state()/cg50core_status() (Halted →
 *       Faulted). En SLEEP sin la opción SLEEP_ADVANCE, el tick NO avanza
 *       instrucciones del guest (comportamiento bit-idéntico al build de
 *       escritorio).
 */
CG50CORE_API void cg50core_tick(Cg50Core* core, uint32_t elapsed_us,
                                uint32_t max_instructions);

/* ==========================================================================
 * Teclado (matriz 16 bits)
 * ========================================================================== */

/**
 * @brief Pulsa/suelta tecla por código de matriz 16 bits (KO alto / KI bajo).
 *
 * Mapa: Cg50Backend::KeyDown (down != 0) / KeyUp (down == 0). El ciclo de
 * vida de la pulsación (down/…/up) lo gobierna el host. Contrato: 50
 * códigos únicos, F1 = 0x070A (cg50_keys_exact.h). Sin mapeo de teclas
 * virtuales (eso vive en el host).
 *
 * @param core        Handle de la instancia.
 * @param matrix_code Código de matriz de 16 bits de la tecla.
 * @param down        != 0 → pulsar (KeyDown); == 0 → liberar (KeyUp).
 * @return 1 = aplicada; 0 = rechazada (estado no Ready — fail-closed).
 */
CG50CORE_API int cg50core_key(Cg50Core* core, uint16_t matrix_code, int down);

/* ==========================================================================
 * Estado y telemetría
 * ========================================================================== */

/**
 * @brief Estado de máquina de producto (mapa: Cg50Backend::GetState()).
 * @return Cg50MachineState (0/1/2) o CG50CORE_ERR_ARG si core NULL.
 */
CG50CORE_API int cg50core_state(const Cg50Core* core);

/**
 * @brief Snapshot completo de telemetría (mapa: GetRuntimeStatus + State).
 *
 * Snapshot consistente al punto de retorno: cpuState/flags/faulted
 * reflejan SIEMPRE el par (CpuState, State) de forma coherente; sin
 * filtrar detalles internos del emulador a la capa producto. Telemetría
 * honesta: NO fabrica pantalla y no se usa para callar fallos.
 *
 * @param core Handle (const).
 * @param out  Estructura rellenada por la ABI. NULL ⇒ CG50CORE_ERR_ARG.
 * @return CG50CORE_OK; CG50CORE_ERR_ARG si out NULL o core NULL.
 */
CG50CORE_API int cg50core_status(const Cg50Core* core, Cg50Status* out);

/* ==========================================================================
 * Framebuffer (BGRA32 crudo, vista por copia)
 * ========================================================================== */

/**
 * @brief Descriptor del framebuffer disponible (384×216, BGRA32).
 * @param core Handle (const).
 * @param out  Descriptor rellenado si hay GRAM real del R61524.
 * @return CG50CORE_OK si disponible; CG50CORE_ERR_CAPACITY si NO lo está
 *         (pantalla no sintetizada aún o tras reset): out sin tocar;
 *         CG50CORE_ERR_ARG si argumentos NULL.
 */
CG50CORE_API int cg50core_frame_info(const Cg50Core* core, Cg50FrameInfo* out);

/**
 * @brief Copia los píxeles actuales a un buffer del host (BGRA32 crudo).
 *
 * Semántica idéntica al FramebufferView del backend (todas las
 * transformaciones ya aplicadas por el core: la ABI no re-toca geometría).
 * La copia es el trabajo de producto: es la vía segura para render
 * (Bitmap/Surface en F2) y la única apta para cruzar hilos.
 *
 * @param core                Handle (const).
 * @param dst                 Buffer del host (uint32 por píxel). Puede ser
 *                            NULL para sondear el tamaño (idem save).
 * @param dst_capacity_pixels Capacidad de dst en uint32 (píxeles).
 * @return Píxeles copiados (≤ min(total, capacidad)); 0 si no hay
 *         framebuffer o core NULL. La copia parcial es legítima y queda
 *         documentada: copia los primeros min(capacidad, 82944) píxeles
 *         en orden lineal de frame, sin otro indicio de truncamiento — el
 *         host puede usar la capacidad exacta
 *         CG50CORE_FRAMEBUFFER_PIXELS; el total se consulta con
 *         cg50core_frame_info().
 */
CG50CORE_API size_t cg50core_frame_copy(const Cg50Core* core, uint32_t* dst,
                                        size_t dst_capacity_pixels);

/* ==========================================================================
 * Snapshot save/restore — protocolo de DOS llamadas (todo-o-nada)
 * ========================================================================== */

/**
 * @brief Serializa el estado completo del core (blob ~53 MB en boot real).
 *
 * Protocolo de dos llamadas: con dst NULL se devuelve el tamaño requerido
 * (blob completo: 79 chunks + bus, con kStateVersion=2 en el chunk 1).
 * Con dst no-NULL y capacidad suficiente, escribe y devuelve bytes
 * escritos. Con capacidad insuficiente: NO escribe nada y devuelve el
 * tamaño requerido (re-llamada idempotente, sin estado parcial). El blob
 * incluye CPU+FPU, UTLB/MMU, RAM (bancos 8 y C 0x0C000000), periféricos,
 * bus y SystemROM/UserROM/C2ROM: por eso queda alrededor de 53 MB en boot
 * real. Si el core no puede exportar, se devuelve 0 (fail-closed): leer
 * cg50core_state()/cg50core_last_error().
 *
 * @param core         Handle (const).
 * @param dst          Buffer destino o NULL (sondeo de tamaño).
 * @param dst_capacity Capacidad real de dst.
 * @return Bytes requeridos o escritos; 0 si core NULL o si el core no
 *         puede exportar (consultar estado y diagnóstico).
 */
CG50CORE_API size_t cg50core_save(const Cg50Core* core, uint8_t* dst,
                                  size_t dst_capacity);

/**
 * @brief Restaura el estado completo del core desde un blob previo.
 *
 * TODO-O-NADA: o el blob entero se aplica (semántica exacta de
 * Cg50NativeCore::ImportState) o no se aplica nada. Tras un fallo
 * intermedio el core puede quedar Halted/Faulted: honesto por diseño; el
 * host debe leer cg50core_state()/last_error() y decidir (reset, nuevo
 * load o restore de otro blob). Determinismo: restore seguido de save ⇒
 * bytes idénticos al original (contrato FR2, probado en rama-tests).
 *
 * @param core Handle de la instancia.
 * @param data Blob previamente serializado por cg50core_save() (de esta u
 *             otra instancia: el restore autentica magic, kStateVersion y
 *             contenido).
 * @param size Tamaño exacto del blob.
 * @return CG50CORE_OK; CG50CORE_ERR_ARG (data NULL o size 0); si el blob
 *         es incompatible → CG50CORE_ERR_STATE (fail-closed); si el blob
 *         es admisible pero falla a mitad → CG50CORE_ERR_FAULTED.
 */
CG50CORE_API int cg50core_restore(Cg50Core* core, const uint8_t* data,
                                  size_t size);

/* ==========================================================================
 * Persistencia opt-in del UserROM (del lado host: la ABI nunca escribe)
 * ========================================================================== */

/**
 * @brief Entrega los bytes de la UserROM cargada (persistencia del host).
 *
 * Sustituto ABI de PersistUserRom/HasPersistedUserRom SIN variables de
 * entorno: el host llama y escribe «<UserROM>_persist.raw» en su app-data
 * (opt-in del dueño); al arrancar, el host re-carga con
 * cg50core_load_firmware_raw. HasPersistedUserRom ≡ «el fichero existe»
 * en el lado host.
 *
 * @param core         Handle (const).
 * @param dst          Buffer del host o NULL (sondeo: devuelve tamaño).
 * @param dst_capacity Capacidad real de dst.
 * @return Bytes requeridos o copiados; 0 si core NULL o sin UserROM.
 */
CG50CORE_API size_t cg50core_read_user_rom(const Cg50Core* core, uint8_t* dst,
                                           size_t dst_capacity);

/* ==========================================================================
 * Extensiones no-break Fase 5 (RTC real, escritura UserROM, carga .g3a)
 * ========================================================================== */

CG50CORE_API int cg50core_set_rtc_epoch(Cg50Core* core, int64_t local_unix_seconds);
CG50CORE_API int cg50core_write_user_rom(Cg50Core* core, const uint8_t* data,
                                         size_t size);
CG50CORE_API int cg50core_load_addin(Cg50Core* core, const uint8_t* g3a_bytes,
                                     size_t size);

/* ==========================================================================
 * Diagnóstico (UTF-8 explícito; la variante de caracteres anchos es F2)
 * ========================================================================== */

/**
 * @brief Copia el diagnóstico honesto de la instancia (UTF-8 explícito).
 *
 * Mapa: Cg50Backend::Diagnostic() — texto humano pensado para mostrarse
 * en la UI, nunca vacío en instancias vivas. Protocolo de dos llamadas:
 * dst NULL ⇒ devuelve los bytes requeridos (sin contar terminador).
 * Siempre escribe el '\0' final dentro de la capacidad (truncado limpio
 * si no cabe).
 *
 * @param core         Handle (const).
 * @param dst_utf8     Buffer del host (bytes UTF-8) o NULL (sondeo).
 * @param dst_capacity Capacidad real de dst (incluido el terminador).
 * @return Bytes copiados (sin terminador), ≥ 0; CG50CORE_ERR_ARG si core
 *         NULL. Retorno negativo reservado exclusivamente a errores de
 *         argumento.
 * @note UTF-16 para shells de caracteres anchos (Windows wstring) es la
 *       adición no-break de F2: cg50core_diagnostic_utf16().
 */
CG50CORE_API int cg50core_last_error(const Cg50Core* core, char* dst_utf8,
                                     size_t dst_capacity);

#ifdef __cplusplus
} /* extern "C" */
#endif

/* ==========================================================================
 * Mapeado ABI ↔ Cg50Backend (referencia de implementación del shim F1)
 * --------------------------------------------------------------------------
 *   cg50core_create            → new Cg50Backend() (impl NATIVO: asegurar
 *                                CG50_ENGINE=native; GCal NO es alcanzable
 *                                por esta ABI — fail-closed)
 *   cg50core_destroy           → delete backend
 *   cg50core_load_firmware_raw → Initialize/LoadFirmwareParts (system y
 *                                user por separado); c2 explícita (sin
 *                                descubrimiento de ficheros)
 *   cg50core_set_option        → (N-12) gates de SLEEP-advance y
 *                                Cg50NativeCore::SetRtcVirtualClockEnabled
 *   cg50core_reset_cpu         → Reset() (nativo: ResetCpuState)
 *   cg50core_tick              → Step(elapsed_us) con budget
 *                                InstructionBudgetForTick / max_instructions
 *   cg50core_key               → KeyDown/KeyUp(matrixCode)
 *   cg50core_state             → GetState() (Unavailable/Ready/Faulted)
 *   cg50core_status            → GetRuntimeStatus() + State + flags
 *   cg50core_frame_info        → FramebufferView {384,216,1536,BGRA32}
 *   cg50core_frame_copy        → copia de FramebufferView (memoria del core)
 *   cg50core_save/restore      → ExportState/ImportState (kStateVersion=2)
 *                                [la forma ABI es blob en memoria con
 *                                two-call; SaveStateToFile/LoadStateToFile
 *                                solo si el shim decide envolver ficheros]
 *   cg50core_read_user_rom     → UserRomData()/UserRomSize()
 *   cg50core_last_error        → Diagnostic() (UTF-8)
 *
 * NO incluidas deliberadamente en F1 (adiciones candidatas de F2,
 * compatibles hacia atrás):
 *   - cg50core_abi_version()        (chequeo runtime en dlopen/dll load)
 *   - cg50core_diagnostic_utf16()   (shells de caracteres anchos Windows)
 *   - cg50core_set_frame_callback() (push de frame; hoy: pull-lockstep)
 * ========================================================================== */

#endif /* CG50_API_C_H */
