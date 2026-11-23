/* ==========================================================================
 * cg50_core_c_shim.cpp — implementación de la ABI C (cg50_api_c.h)
 * Proyecto: Calculadora Unificada — edición nativa (CG50_ENGINE=native)
 *
 * ESTADO: FASE 3 (2026-11-03): el trío entra por InitializeParts (fase 2,
 * 2026-11-02), las opciones respetan el gate por estado (pin fase 1 intacto
 * y SLEEP_ADVANCE + RTC_VIRTUAL con máquina READY) y el snapshot
 * save/restore corre por bytes con el ExportState/ImportState del core. Los
 * comodines falle que subsisten se marcan y la fase siguiente los sustituye
 * SIN cambiar la firma ni esta cabecera:
 *   - cg50core_set_option      -> FASE 3 (2026-11-03): gate por estado en la
 *                                 ABI (ERR_ARG sin firmware: pin fase 1
 *                                 intacto); con READY, SLEEP_ADVANCE = 1 y
 *                                 RTC_VIRTUAL = 2 entran al backend (el
 *                                 RTC activa el reloj virtual del core);
 *                                 desconocidas ERR_ARG.
 *                                 (2026-11-02: SLEEP_ADVANCE = SetOption
 *                                 backend, sticky al handle.)
 *   - cg50core_save            -> FASE 3 (2026-11-03): SaveStateToBuffer
 *                                 (ExportState del core, 79 chunks + bus,
 *                                 kStateVersion=2); el protocolo de dos
 *                                 llamadas pinado por el contrato.
 *   - cg50core_restore         -> FASE 3 (2026-11-03): LoadStateFromBuffer
 *                                 (ImportState, todo-o-nada; blob basura =
 *                                 ERR_STATE y el core acusa el desenlace).
 *   - cg50core_read_user_rom   -> FASE 4 (2026-11-03): UserRomBytes/Size
 *                                 del backend (persistencia OPT-IN del
 *                                 host; la ABI nunca escribe). El pin de
 *                                 fase 1 "sin UserROM = 0" queda igual.
 *   - cg50core_tick con max_instructions != 0 -> avanza con el presupuesto
 *                                 interno (el progreso gana; la no-honra
 *                                 queda registrada en el handle); fase 2 =
 *                                 Step(µs, maxInstructions)
 *   - cg50core_load_firmware_raw -> FASE 2 REALIZADA (2026-11-02):
 *                                 SystemROM exacta 0x00B60000 sola, o el
 *                                 trío con user/c2 exactas vía
 *                                 Cg50Backend::InitializeParts (partes RAW
 *                                 del host; LoadC2Rom del core). Boot real
 *                                 del OS con el trío.
 *
 * Reglas ABI implementadas (cg50_api_c.h): cero excepciones que cruzan a
 * C (1); ciclo de vida estricto en create/destroy (2); sin threading (3);
 * presupuesto automático de tick (4); la ABI jamás lee env ni ficheros (5);
 * framebuffer BGRA32 crudo por copia (6); protocolo de dos llamadas (7);
 * sin I/O lateral (8); teclado por matriz 16 bits con F1=0x070A (9);
 * opciones fail-closed (10).
 *
 * Blanco del shim: Cg50Backend de rama-cg50_backend.{h,cpp} (variante sin
 * GCal: Estructura que hace imposible enlazar el motor CASIO 32-bit por
 * esta ABI). core.cpp queda intocado en fase 1.
 * ========================================================================== */

#include "cg50_api_c.h"
#include "cg50_backend.h" /* alias de inclusión -> rama-cg50_backend.h */

#include <cstring>
#include <new>
#include <string>
#include <vector>

/* -------------------------------------------------------------------------
 * Definición única del opaco de la ABI ("typedef struct Cg50Core").
 * NOTA: a scope GLOBAL — dentro del namespace anónimo el nombre queda
 * ambiguo frente al typedef del encabezado (hit MSVC C2872).
 * ---------------------------------------------------------------------- */
struct Cg50Core final {
    Cg50Backend* backend = nullptr;
    /* Fase 1: registro interno de efectos pendientes (sin canal ABI aún;
     * la fase 2 lo expone sin romper el contrato actual). */
    unsigned tickMaxNotHonored = 0; /* >=1 si tick se llamó con max!=0 */
};

namespace {

/* -------------------------------------------------------------------------
 * UTF-16 -> UTF-8 explícito (regla: sin API de caracteres anchos; determinista,
 * con pares sustitutos y U+FFFD para solitarias/inválidas).
 * ---------------------------------------------------------------------- */
std::size_t Utf8EncodedLength(std::uint32_t cp) noexcept
{
    if (cp <= 0x7Fu) return 1;
    if (cp <= 0x7FFu) return 2;
    if (cp <= 0xFFFFu) return 3;
    return 4;
}

void Utf8Encode(std::uint32_t cp, char* dst) noexcept
{
    if (cp <= 0x7Fu) {
        dst[0] = static_cast<char>(cp);
    } else if (cp <= 0x7FFu) {
        dst[0] = static_cast<char>(0xC0u | (cp >> 6));
        dst[1] = static_cast<char>(0x80u | (cp & 0x3Fu));
    } else if (cp <= 0xFFFFu) {
        dst[0] = static_cast<char>(0xE0u | (cp >> 12));
        dst[1] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
        dst[2] = static_cast<char>(0x80u | (cp & 0x3Fu));
    } else {
        dst[0] = static_cast<char>(0xF0u | (cp >> 18));
        dst[1] = static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
        dst[2] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
        dst[3] = static_cast<char>(0x80u | (cp & 0x3Fu));
    }
}

/* Longitud UTF-8 del texto completo (bytes, SIN terminador). */
std::size_t Utf16ToUtf8Size(const std::wstring& text) noexcept
{
    std::size_t bytes = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        std::uint32_t cp = static_cast<std::uint32_t>(text[i]) & 0xFFFFu;
        if (cp >= 0xD800u && cp <= 0xDBFFu) {
            if (i + 1 < text.size()) {
                const std::uint32_t lo =
                    static_cast<std::uint32_t>(text[i + 1]) & 0xFFFFu;
                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                    cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                    ++i;
                } else {
                    cp = 0xFFFDu; /* sustituto alto solitario */
                }
            } else {
                cp = 0xFFFDu;
            }
        } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
            cp = 0xFFFDu; /* sustituto bajo solitario */
        }
        bytes += Utf8EncodedLength(cp);
    }
    return bytes;
}

/* Escribe el UTF-8 truncado a `bytes` máximo; devuelve bytes escritos. */
std::size_t Utf16ToUtf8Write(const std::wstring& text, char* dst,
                             std::size_t bytes) noexcept
{
    std::size_t written = 0;
    for (std::size_t i = 0; i < text.size() && written < bytes; ++i) {
        std::uint32_t cp = static_cast<std::uint32_t>(text[i]) & 0xFFFFu;
        if (cp >= 0xD800u && cp <= 0xDBFFu) {
            if (i + 1 < text.size()) {
                const std::uint32_t lo =
                    static_cast<std::uint32_t>(text[i + 1]) & 0xFFFFu;
                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                    cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                    ++i;
                } else {
                    cp = 0xFFFDu;
                }
            } else {
                cp = 0xFFFDu;
            }
        } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
            cp = 0xFFFDu;
        }
        char tmp[4];
        const std::size_t need = Utf8EncodedLength(cp);
        Utf8Encode(cp, tmp);
        if (written + need > bytes) break;
        std::memcpy(dst + written, tmp, need);
        written += need;
    }
    return written;
}

/* Diagnóstico como std::wstring backend -> vista shim con fallback. */
const std::wstring& DiagnosticOf(const Cg50Core* core)
{
    return core->backend->Diagnostic();
}

} // namespace

/* ==========================================================================
 * Ciclo de vida (regla 2)
 * ========================================================================== */

extern "C" CG50CORE_API Cg50Core* cg50core_create(void)
{
    Cg50Core* handle = nullptr;
    try {
        handle = new Cg50Core();
        handle->backend = new Cg50Backend();
        return handle;
    } catch (...) {
        /* Regla 1: jamás cruza una excepción a C. */
        delete handle;
        return nullptr;
    }
}

extern "C" CG50CORE_API void cg50core_destroy(Cg50Core* core)
{
    if (core == nullptr) return; /* no-op seguro por header */
    try {
        delete core->backend;
    } catch (...) {
        /* Regla 1: la destrucción nunca propaga. */
    }
    delete core;
}

/* ==========================================================================
 * Carga de firmware (regla 5: punteros del host; sin ficheros ni env)
 * ========================================================================== */

extern "C" CG50CORE_API int cg50core_load_firmware_raw(
    Cg50Core* core, const uint8_t* system, size_t system_len,
    const uint8_t* user, size_t user_len, const uint8_t* c2, size_t c2_len)
{
    if (core == nullptr) return CG50CORE_ERR_ARG;
    try {
        /* Fase 2 (2026-11-02): SystemROM RAW exacta sola (conveniencia del
         * contrato) o el trío real user/c2 exactas — el encabezado ya lo
         * declara. Las variantes admitidas por core.cpp (combinada
         * 0x02000000) siguen EXCLUIDAS ("el host entrega cada parte por
         * separado"). */
        if (system == nullptr ||
            system_len != CG50CORE_SYSTEM_ROM_SIZE) {
            return CG50CORE_ERR_ARG; /* fail-closed: nada se aplicó */
        }
        if (user != nullptr && user_len != CG50CORE_USER_ROM_SIZE)
            return CG50CORE_ERR_ARG; /* rechazo antes de tocar el backend */
        if (c2 != nullptr && c2_len != CG50CORE_C2_ROM_SIZE)
            return CG50CORE_ERR_ARG; /* rechazo antes de tocar el backend */

        if (user != nullptr || c2 != nullptr) {
            const std::size_t userSize = user != nullptr ? user_len : 0;
            const std::size_t c2Size = c2 != nullptr ? c2_len : 0;
            if (!core->backend->InitializeParts(system, system_len,
                                                user, userSize,
                                                c2, c2Size)) {
                /* Inadmisible p.ej. contenedor DLSim: el backend quedó
                 * Faulted con diagnóstico honesto (regla de telemetría). */
                if (core->backend->GetState() == Cg50Backend::State::Faulted)
                    return CG50CORE_ERR_FAULTED;
                return CG50CORE_ERR_ARG;
            }
        } else if (!core->backend->Initialize(system, system_len)) {
            /* Inadmisible p.ej. contenedor DLSim: el backend quedó Faulted
             * con diagnóstico honesto (regla de telemetría). */
            if (core->backend->GetState() == Cg50Backend::State::Faulted)
                return CG50CORE_ERR_FAULTED;
            return CG50CORE_ERR_ARG;
        }
        return CG50CORE_OK;
    } catch (...) {
        return CG50CORE_ERR_ARG; /* regla 1: ninguna excepción cruza a C */
    }
}

/* ==========================================================================
 * Reset de CPU
 * ========================================================================== */

extern "C" CG50CORE_API void cg50core_reset_cpu(Cg50Core* core)
{
    if (core == nullptr) return; /* no-op por seguridad */
    try {
        core->backend->Reset();
    } catch (...) {
        /* Regla 1: void; la incidencia queda consultable en state/status. */
    }
}

/* ==========================================================================
 * Opciones (regla 10: fail-closed; fase 1: ERR_ARG sin cambios)
 * ========================================================================== */

extern "C" CG50CORE_API int cg50core_set_option(Cg50Core* core,
                                                uint32_t option,
                                                uint32_t value)
{
    if (core == nullptr) return CG50CORE_ERR_ARG;
    try {
        /* Fail-closed por estado (regla 10) y pin del contrato fase 1:
         * sin firmware cargado la instancia no toma decisiones — el
         * chequeo "set_option SLEEP_ADVANCE (fase 1) = ERR_ARG" y el
         * "no alteró nada" se conservan EXACTOS porque la opción solo
         * entra al backend con la máquina READY (2026-11-03: el mapeo
         * directo de la fase 2 parcial rompía ese pin; gates restaurado).
         * RTC_VIRTUAL y desconocidas siguen ERR_ARG sí o sí (fase 3). */
        if (core->backend->GetState() != Cg50Backend::State::Ready) {
            return CG50CORE_ERR_ARG;
        }
        const bool aplicada = core->backend->SetOption(option, value);
        return aplicada ? CG50CORE_OK : CG50CORE_ERR_ARG;
    } catch (...) {
        return CG50CORE_ERR_ARG; /* regla 1 */
    }
}

/* ==========================================================================
 * Tick determinista (regla 4)
 * ========================================================================== */

extern "C" CG50CORE_API void cg50core_tick(Cg50Core* core, uint32_t elapsed_us,
                                           uint32_t max_instructions)
{
    if (core == nullptr) return;
    try {
        /* Tope de elasticidad del presupuesto interior
         * (InstructionBudgetForTick): min(µs×60, 2 500 000). El backend
         * Step(elapsed_us) ya lo aplica idéntico. */
        core->backend->Step(elapsed_us);
        if (max_instructions != 0) {
            /* Fase 1: el tope del host NO sustituye aún el presupuesto
             * automático (plomería pendiente); el progreso gana para no
             * congelar al huésped y la no-honra queda registrada. */
            ++core->tickMaxNotHonored;
        }
    } catch (...) {
        /* Regla 1: void; la incidencia se lee con state/status/last_error. */
    }
}

/* ==========================================================================
 * Teclado (regla 9: matriz 16 bits, F1=0x070A; ciclo down/up del host)
 * ========================================================================== */

extern "C" CG50CORE_API int cg50core_key(Cg50Core* core, uint16_t matrix_code,
                                         int down)
{
    if (core == nullptr) return 0; /* rechaza (fail-closed) */
    try {
        if (down != 0) {
            return core->backend->KeyDown(matrix_code) ? 1 : 0;
        }
        /* KeyUp del backend es void; se considera aplicada si la instancia
         * está Ready (precondición del enganche nativo). */
        if (core->backend->GetState() != Cg50Backend::State::Ready) return 0;
        core->backend->KeyUp(matrix_code);
        return 1;
    } catch (...) {
        return 0; /* regla 1 */
    }
}

/* ==========================================================================
 * Estado y telemetría
 * ========================================================================== */

extern "C" CG50CORE_API int cg50core_state(const Cg50Core* core)
{
    if (core == nullptr) return CG50CORE_ERR_ARG;
    try {
        switch (core->backend->GetState()) {
        case Cg50Backend::State::Ready:
            return CG50CORE_MACHINE_READY;
        case Cg50Backend::State::Faulted:
            return CG50CORE_MACHINE_FAULTED;
        case Cg50Backend::State::Unavailable:
        default:
            return CG50CORE_MACHINE_UNAVAILABLE;
        }
    } catch (...) {
        return CG50CORE_ERR_ARG; /* regla 1 */
    }
}

extern "C" CG50CORE_API int cg50core_status(const Cg50Core* core,
                                            Cg50Status* out)
{
    if (core == nullptr || out == nullptr) return CG50CORE_ERR_ARG;
    try {
        const auto rt = core->backend->GetRuntimeStatus();
        *out = Cg50Status{};
        out->cpuState = static_cast<uint8_t>(rt.cpuState);
        out->systemRomLoaded = rt.systemRomLoaded ? 1 : 0;
        out->userRomLoaded = rt.userRomLoaded ? 1 : 0;
        out->framebufferAvailable = rt.framebufferAvailable ? 1 : 0;
        out->faulted =
            core->backend->GetState() == Cg50Backend::State::Faulted ? 1 : 0;
        out->pc = rt.pc;
        out->sr = rt.sr;
        out->lastExceptionEvent = rt.lastExceptionEvent;
        out->lastInterruptEvent = rt.lastInterruptEvent;
        out->instructions = rt.instructions;
        out->cycles = rt.cycles;
        out->exceptionsTaken = rt.exceptionsTaken;
        out->interruptsAccepted = rt.interruptsAccepted;
        out->unsupportedOpcodeSeen = rt.unsupportedOpcodeSeen ? 1 : 0;
        out->unsupportedPc = rt.unsupportedPc;
        out->unsupportedOpcode = rt.unsupportedOpcode;
        out->memoryFaultSeen = rt.memoryFaultSeen ? 1 : 0;
        out->lastMemoryFaultWasWrite = rt.lastMemoryFaultWasWrite ? 1 : 0;
        out->lastMemoryFaultAddress = rt.lastMemoryFaultAddress;
        return CG50CORE_OK;
    } catch (...) {
        return CG50CORE_ERR_ARG; /* regla 1 */
    }
}

/* ==========================================================================
 * Framebuffer (regla 6: BGRA32 crudo; vista por copia; nada sintéticos)
 * ========================================================================== */

extern "C" CG50CORE_API int cg50core_frame_info(const Cg50Core* core,
                                                Cg50FrameInfo* out)
{
    if (core == nullptr || out == nullptr) return CG50CORE_ERR_ARG;
    try {
        const auto fb = core->backend->Framebuffer();
        if (fb.format != Cg50Backend::PixelFormat::Bgra32 ||
            fb.pixels == nullptr || fb.width == 0 || fb.height == 0) {
            /* Regla de honestidad: el core NO fabrica pantalla. */
            return CG50CORE_ERR_CAPACITY;
        }
        *out = Cg50FrameInfo{};
        out->width = static_cast<uint32_t>(fb.width);
        out->height = static_cast<uint32_t>(fb.height);
        out->strideBytes =
            static_cast<uint32_t>(fb.stridePixels * sizeof(std::uint32_t));
        out->pixelFormat = CG50CORE_PIXEL_FORMAT_BGRA32;
        return CG50CORE_OK;
    } catch (...) {
        return CG50CORE_ERR_ARG; /* regla 1 */
    }
}

extern "C" CG50CORE_API size_t cg50core_frame_copy(const Cg50Core* core,
                                                   uint32_t* dst,
                                                   size_t dst_capacity_pixels)
{
    if (core == nullptr) return 0;
    try {
        const auto fb = core->backend->Framebuffer();
        if (fb.format != Cg50Backend::PixelFormat::Bgra32 ||
            fb.pixels == nullptr) {
            return 0; /* sin framebuffer: 0 píxeles, sin sintéticos */
        }
        const std::size_t total =
            static_cast<std::size_t>(fb.width) * fb.height;
        if (dst == nullptr) {
            return total; /* sondeo de tamaño (protocolo de dos llamadas) */
        }
        const std::size_t count =
            dst_capacity_pixels < total ? dst_capacity_pixels : total;
        if (count != 0) {
            std::memcpy(dst, fb.pixels, count * sizeof(std::uint32_t));
        }
        return count;
    } catch (...) {
        return 0; /* regla 1 */
    }
}

/* ==========================================================================
 * Snapshot save/restore (regla 7: protocolo de dos llamadas)
 * Fase 3 (2026-11-03): ExportState/ImportState del core por bytes. Los
 * pines de fase 1 (instancia sin firmware o Faulted) quedan intactos: sin
 * máquina RomNative no se exporta y no se importa nada.
 * ========================================================================== */

extern "C" CG50CORE_API size_t cg50core_save(const Cg50Core* core, uint8_t* dst,
                                             size_t dst_capacity)
{
    if (core == nullptr) return 0;
    try {
        if (core->backend == nullptr ||
            core->backend->GetState() != Cg50Backend::State::Ready) {
            /* Fase 1 pin: sin máquina READY el core no exporta (0 =
             * "no puede exportar"; estado/diagnóstico a la vista). */
            return 0;
        }
        std::vector<std::uint8_t> bytes;
        if (!core->backend->SaveStateToBuffer(bytes)) return 0;
        const size_t requerido = bytes.size();
        if (dst == nullptr) return requerido;            /* sondeo */
        if (dst_capacity < requerido) return requerido;  /* sin escribir */
        std::memcpy(dst, bytes.data(), requerido);
        return requerido;
    } catch (...) {
        return 0; /* regla 1 */
    }
}

extern "C" CG50CORE_API int cg50core_restore(Cg50Core* core, const uint8_t* data,
                                             size_t size)
{
    if (core == nullptr || data == nullptr || size == 0)
        return CG50CORE_ERR_ARG;
    try {
        if (core->backend == nullptr ||
            core->backend->GetState() != Cg50Backend::State::Ready) {
            /* Fase 1 pin: sin máquina RomNative no entra ningún blob —
             * ERR_STATE fail-closed sin alterar la instancia. */
            return CG50CORE_ERR_STATE;
        }
        return core->backend->LoadStateFromBuffer(data, size)
                   ? CG50CORE_OK
                   : CG50CORE_ERR_STATE;
        /* Nota honesta: si el core rechaza el blob, ImportState marca
         * Halted (cabecera de la ABI: el host lee estado/diagnóstico y
         * decide). El backend refleja Faulted tras LoadStateFromBuffer. */
    } catch (...) {
        return CG50CORE_ERR_STATE; /* regla 1: sin cruces a C */
    }
}

/* ==========================================================================
 * UserROM readback (fase 1: comodín declarado; fase 2: accesores)
 * ========================================================================== */

extern "C" CG50CORE_API size_t cg50core_read_user_rom(const Cg50Core* core,
                                                      uint8_t* dst,
                                                      size_t dst_capacity)
{
    if (core == nullptr || core->backend == nullptr) return 0;
    try {
        const size_t requerido = core->backend->UserRomSize();
        if (requerido == 0) return 0;               /* sin UserROM: 0.     */
        if (dst == nullptr) return requerido;       /* sondeo.             */
        if (dst_capacity < requerido) return requerido; /* sin escribir.   */
        const std::uint8_t* bytes = core->backend->UserRomBytes();
        if (bytes == nullptr) return 0;             /* imposible: mismo
                                                     * handle, sin UserROM */
        std::memcpy(dst, bytes, requerido);
        return requerido;
    } catch (...) {
        return 0; /* regla 1 */
    }
}

extern "C" CG50CORE_API int cg50core_set_rtc_epoch(Cg50Core* core,
                                                   int64_t local_unix_seconds)
{
    if (core == nullptr) return CG50CORE_ERR_ARG;
    try {
        if (core->backend == nullptr ||
            core->backend->GetState() != Cg50Backend::State::Ready) {
            return CG50CORE_ERR_STATE;
        }
        return core->backend->SetRtcEpochSeconds(local_unix_seconds)
                   ? CG50CORE_OK
                   : CG50CORE_ERR_STATE;
    } catch (...) {
        return CG50CORE_ERR_STATE;
    }
}

extern "C" CG50CORE_API int cg50core_write_user_rom(Cg50Core* core,
                                                    const uint8_t* data,
                                                    size_t size)
{
    if (core == nullptr || data == nullptr || size != CG50CORE_USER_ROM_SIZE) {
        return CG50CORE_ERR_ARG;
    }
    try {
        if (core->backend == nullptr ||
            core->backend->GetState() != Cg50Backend::State::Ready) {
            return CG50CORE_ERR_STATE;
        }
        return core->backend->ReplaceUserRom(data, size)
                   ? CG50CORE_OK
                   : CG50CORE_ERR_STATE;
    } catch (...) {
        return CG50CORE_ERR_STATE;
    }
}

extern "C" CG50CORE_API int cg50core_load_addin(Cg50Core* core,
                                                const uint8_t* g3a_bytes,
                                                size_t size)
{
    if (core == nullptr || g3a_bytes == nullptr || size == 0) {
        return CG50CORE_ERR_ARG;
    }
    try {
        if (core->backend == nullptr ||
            core->backend->GetState() != Cg50Backend::State::Ready) {
            return CG50CORE_ERR_STATE;
        }
        return core->backend->LoadG3aAddin(g3a_bytes, size)
                   ? CG50CORE_OK
                   : CG50CORE_ERR_ARG;
    } catch (...) {
        return CG50CORE_ERR_STATE;
    }
}

/* ==========================================================================
 * Diagnóstico (UTF-8 explícito; protocolo de dos llamadas; truncado limpio)
 * ========================================================================== */

extern "C" CG50CORE_API int cg50core_last_error(const Cg50Core* core,
                                                char* dst_utf8,
                                                size_t dst_capacity)
{
    if (core == nullptr) return CG50CORE_ERR_ARG;
    try {
        static const std::wstring fallback =
            L"CG50 ABI: sin diagnóstico disponible";
        const std::wstring& text = DiagnosticOf(core);
        const std::wstring& source = text.empty() ? fallback : text;

        const std::size_t required = Utf16ToUtf8Size(source);
        if (dst_utf8 == nullptr || dst_capacity == 0) {
            /* Sondeo: bytes requeridos SIN terminador. */
            return static_cast<int>(required);
        }
        const std::size_t budget = dst_capacity - 1; /* espacio del '\0' */
        const std::size_t taken =
           Utf16ToUtf8Write(source, dst_utf8, budget);
        dst_utf8[taken] = '\0'; /* regla: '\0' siempre dentro de capacidad */
        return static_cast<int>(taken);
    } catch (...) {
        return CG50CORE_ERR_ARG; /* regla 1 */
    }
}
