// Portabilidad (2026-11-02, iOS-fix-1): las únicas dependencias Win32 de este
// backend son GetEnvironmentVariableW y GetModuleFileNameW. Bajo _WIN32 el
// comportamiento es byte-idéntico al anterior; en POSIX/macOS el env sale de
// std::getenv y el directorio del módulo cae a current_path (la ABI C no
// descubre ficheros: regla 5 de cg50_api_c.h — en iOS/móvil el host pasa
// los bytes y estas rutas de descubrimiento ni se usan).
#include <cstdlib>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "cg50_backend.h"
#include "cg50_native_core.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace fs = std::filesystem;

std::optional<fs::path> EnvironmentPath(const wchar_t* name)
{
#if defined(_WIN32)
    const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required == 0) return std::nullopt;

    std::wstring value(required, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, value.data(), required);
    if (written == 0 || written >= required) return std::nullopt;
    value.resize(written);
    if (value.empty()) return std::nullopt;
    return fs::path(value);
#else
    if (name == nullptr) return std::nullopt;
    std::string key;
    key.reserve(64);
    for (int i = 0; name[i] != L'\0'; ++i) {
        // Los nombres de variables CG50_* son ASCII; un nombre ancho con bytes no
        // ASCII no corresponde a una variable de entorno portable: fail-closed.
        constexpr wchar_t kMaxAscii = L'\x7f';
        if (name[i] > kMaxAscii) return std::nullopt;
        key.push_back(static_cast<char>(name[i]));
    }
    const char* value = std::getenv(key.c_str());
    if (value == nullptr || value[0] == '\0') return std::nullopt;
    return fs::path(value);
#endif
}

fs::path ModuleDirectory()
{
#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD written = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (written == 0) return fs::current_path();
        if (written < buffer.size() - 1) {
            buffer.resize(written);
            return fs::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    // Sin equivalente portable directo al dir del exe; el descubrimiento de
    // ficheros es un auxiliar del camino ficheros (no de la ABI C). El cwd
    // cubre a los binarios de probe/desktop POSIX que sí lo utilizan.
    return fs::current_path();
#endif
}

bool ReadBinaryFile(const fs::path& path,
                    std::vector<std::uint8_t>& bytes,
                    std::wstring& error)
{
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec) {
        error = L"No se pudo obtener tamaño de " + path.wstring();
        return false;
    }
    if (size == 0 || size > Cg50NativeCore::kCombinedRomSize) {
        error = L"Tamaño inválido para firmware CG50 RAW: " + path.wstring();
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = L"No se pudo abrir " + path.wstring();
        return false;
    }

    bytes.resize(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!input || static_cast<std::size_t>(input.gcount()) != bytes.size()) {
        bytes.clear();
        error = L"Lectura incompleta de " + path.wstring();
        return false;
    }
    return true;
}

struct FirmwarePaths {
    fs::path system;
    fs::path user;
    fs::path combined;
};

fs::path FirstExisting(const std::vector<fs::path>& roots,
                       const std::vector<const wchar_t*>& names)
{
    std::error_code ec;
    for (const auto& root : roots) {
        if (root.empty()) continue;
        for (const auto* name : names) {
            const auto candidate = root / name;
            if (fs::is_regular_file(candidate, ec) && !ec) return candidate;
            ec.clear();
        }
    }
    return {};
}

bool ResolveFirmwarePaths(FirmwarePaths& paths, std::wstring& error)
{
    const auto envSystem = EnvironmentPath(L"CG50_SYSTEM_ROM");
    const auto envUser = EnvironmentPath(L"CG50_USER_ROM");
    const auto envCombined = EnvironmentPath(L"CG50_COMBINED_ROM");

    std::error_code ec;
    const auto requireExisting = [&](const std::optional<fs::path>& value,
                                     const wchar_t* variable,
                                     fs::path& out) -> bool {
        if (!value) return true;
        if (!fs::is_regular_file(*value, ec) || ec) {
            error = std::wstring(variable) + L" apunta a un archivo inexistente: " +
                    value->wstring();
            ec.clear();
            return false;
        }
        out = *value;
        return true;
    };

    if (!requireExisting(envSystem, L"CG50_SYSTEM_ROM", paths.system) ||
        !requireExisting(envUser, L"CG50_USER_ROM", paths.user) ||
        !requireExisting(envCombined, L"CG50_COMBINED_ROM", paths.combined)) {
        return false;
    }

    // Explicit SystemROM wins over an explicit combined image. UserROM is optional.
    if (!paths.system.empty()) return true;
    if (!paths.combined.empty()) return true;

    std::vector<fs::path> roots;
    if (const auto dir = EnvironmentPath(L"CG50_ROM_DIR")) roots.push_back(*dir);
    if (const auto dir = EnvironmentPath(L"ROMS_DIR")) roots.push_back(*dir);

    const auto exeDir = ModuleDirectory();
    roots.push_back(exeDir / L"roms");
    roots.push_back(exeDir / L"runtime");
    roots.push_back(exeDir);

    std::error_code cwdEc;
    const auto cwd = fs::current_path(cwdEc);
    if (!cwdEc) {
        roots.push_back(cwd / L"roms");
        roots.push_back(cwd / L"runtime");
        roots.push_back(cwd);
        roots.push_back(cwd / L"build-verify" / L"Release" / L"runtime");
        roots.push_back(cwd / L"build-native-x64" / L"Release" / L"runtime");
    }

    paths.system = FirstExisting(roots, {
        L"SystemROM.raw",
        L"systemrom.raw",
        L"fxcg50_system.rom",
        L"fxcg50_system.bin",
        L"fx-cg50_system.rom",
        L"cg50_system.rom"
    });
    if (!paths.system.empty()) {
        if (paths.user.empty()) {
            paths.user = FirstExisting(roots, {
                L"UserROM.raw",
                L"userrom.raw",
                L"fxcg50_user.rom",
                L"fxcg50_user.bin",
                L"fx-cg50_user.rom",
                L"cg50_user.rom"
            });
        }
        return true;
    }

    paths.combined = FirstExisting(roots, {
        L"fxcg50_combined.rom",
        L"fxcg50_combined.bin"
    });
    return true;
}

std::uint64_t InstructionBudgetForTick(const std::uint32_t elapsedMicroseconds) noexcept
{
    // The fx-CG50 SH7305 CPU runs at ~58 MHz. In normal UI operation, the guest
    // enters SLEEP between input events. During initial boot and transitions,
    // budget up to ~2.5 million instructions per tick (roughly 20-30 ms on x64)
    // so the OS completes boot in ~2 seconds without starving the Win32 message pump.
    const std::uint64_t scaled =
        std::max<std::uint64_t>(1, static_cast<std::uint64_t>(elapsedMicroseconds) * 60ull);
    return std::min<std::uint64_t>(scaled, 2500000ull);
}

Cg50Backend::CpuState PublicCpuState(const Cg50NativeCore::State state) noexcept
{
    switch (state) {
    case Cg50NativeCore::State::Running:  return Cg50Backend::CpuState::Running;
    case Cg50NativeCore::State::Sleeping: return Cg50Backend::CpuState::Sleeping;
    case Cg50NativeCore::State::Halted:   return Cg50Backend::CpuState::Halted;
    default:                              return Cg50Backend::CpuState::Empty;
    }
}

} // namespace

struct Cg50Backend::Impl {
    std::unique_ptr<Cg50NativeCore> native;
    fs::path systemPath;
    fs::path userPath;
    fs::path combinedPath;
};

Cg50Backend::Cg50Backend() : impl_(std::make_unique<Impl>()) {}

Cg50Backend::~Cg50Backend()
{
    UnloadHost();
}

void Cg50Backend::UnloadHost()
{
    if (impl_ && impl_->native && engine_ == Engine::RomNative) {
        if (const auto flag = EnvironmentPath(L"CG50_PERSIST_STORAGE")) {
            if (*flag == L"1") (void)PersistUserRom();
        }
    }
    if (impl_) {
        impl_->native.reset();
        impl_->systemPath.clear();
        impl_->userPath.clear();
        impl_->combinedPath.clear();
    }
    engine_ = Engine::None;
    framebufferCopy_.clear();
    fbWidth_ = fbHeight_ = fbStride_ = 0;
}

bool Cg50Backend::CaptureFramebuffer()
{
    framebufferCopy_.clear();
    fbWidth_ = fbHeight_ = fbStride_ = 0;

    if (!impl_ || !impl_->native) {
        stages_.nativeFramebufferAvailable = false;
        stages_.stage9_lcdActive = false;
        return false;
    }

    // The R61524 model deliberately does not expose pixels until command/data,
    // GRAM and pixel format are reconstructed from evidence. Never manufacture
    // a synthetic screen merely to satisfy the unified host UI.
    stages_.nativeFramebufferAvailable =
        impl_->native->Peripherals().Lcd().FramebufferAvailable();
    stages_.stage9_lcdActive = stages_.nativeFramebufferAvailable;
    if (!stages_.nativeFramebufferAvailable) return false;

    const std::uint32_t lastSar = impl_->native->LastLcdDmaSar();
    const std::uint32_t vramBase = (lastSar == 0x08028800u) ? 0x08028800u : 0x08000000u;

    fbWidth_ = 384;
    fbHeight_ = 216;
    fbStride_ = 384;
    framebufferCopy_.resize(fbWidth_ * fbHeight_);
    for (std::size_t y = 0; y < fbHeight_; ++y) {
        const std::size_t srcY = fbHeight_ - 1 - y;
        for (std::size_t x = 0; x < fbWidth_; ++x) {
            const std::size_t srcX = fbWidth_ - 1 - x;
            const std::size_t srcIdx = srcY * fbWidth_ + srcX;
            std::uint16_t rgb565 = 0;
            impl_->native->Read16(static_cast<std::uint32_t>(vramBase + srcIdx * 2), rgb565);
            const std::uint32_t r = ((rgb565 >> 11) & 0x1Fu) * 255u / 31u;
            const std::uint32_t g = ((rgb565 >> 5) & 0x3Fu) * 255u / 63u;
            const std::uint32_t b = (rgb565 & 0x1Fu) * 255u / 31u;
            framebufferCopy_[y * fbWidth_ + x] = (0xFFu << 24) | (r << 16) | (g << 8) | b;
        }
    }

    stages_.currentStage = 3;
    return true;
}

bool Cg50Backend::Initialize(const std::uint8_t* firmware,
                             const std::size_t firmwareSize)
{
    UnloadHost();
    stages_ = StageStatus{};
    state_ = State::Unavailable;
    engine_ = Engine::None;
    diagnostic_ = L"CG50 ROM-native: buscando SystemROM RAW";

    impl_->native = std::make_unique<Cg50NativeCore>();
    // Fase 3: bandera RTC del handle al nativo nuevo (regla sticky).
    impl_->native->SetRtcVirtualClockEnabled(optionRtcVirtual_);
    bool loaded = false;

    if (firmware != nullptr || firmwareSize != 0) {
        if (firmware == nullptr || firmwareSize == 0) {
            state_ = State::Faulted;
            diagnostic_ = L"CG50 ROM-native: firmware/size incoherentes";
            return false;
        }
        loaded = impl_->native->LoadFirmware(firmware, firmwareSize);
    } else {
        FirmwarePaths paths{};
        std::wstring pathError;
        if (!ResolveFirmwarePaths(paths, pathError)) {
            state_ = State::Faulted;
            diagnostic_ = std::move(pathError);
            return false;
        }

        impl_->systemPath = paths.system;
        impl_->userPath = paths.user;
        impl_->combinedPath = paths.combined;

        if (!paths.system.empty()) {
            std::vector<std::uint8_t> systemBytes;
            std::vector<std::uint8_t> userBytes;
            std::wstring readError;
            if (!ReadBinaryFile(paths.system, systemBytes, readError)) {
                state_ = State::Faulted;
                diagnostic_ = std::move(readError);
                return false;
            }
            if (!paths.user.empty()) {
                fs::path userPath = paths.user;
                if (const auto flag = EnvironmentPath(L"CG50_PERSIST_STORAGE")) {
                    if (*flag == L"1") {
                        const fs::path persisted =
                            paths.user.parent_path() /
                            (paths.user.stem().wstring() + L"_persist.raw");
                        std::error_code ec;
                        if (fs::is_regular_file(persisted, ec) && !ec) {
                            userPath = persisted;
                        }
                    }
                }
                if (!ReadBinaryFile(userPath, userBytes, readError)) {
                    state_ = State::Faulted;
                    diagnostic_ = std::move(readError);
                    return false;
                }
            }

            loaded = impl_->native->LoadFirmwareParts(
                systemBytes.data(), systemBytes.size(),
                userBytes.empty() ? nullptr : userBytes.data(), userBytes.size());

            // C2ROM (N-8): segunda imagen flash de la ventana [0xB60000,0x1000000),
            // reconstruida del GUEST_DUMP del oficial. Se autocarga desde el
            // directorio del UserROM; sin ella el montaje FS falla.
            if (loaded && !paths.user.empty()) {
                const fs::path c2Path = paths.user.parent_path() / L"C2ROM.raw";
                std::error_code c2ec;
                if (fs::is_regular_file(c2Path, c2ec) && !c2ec) {
                    std::vector<std::uint8_t> c2Bytes;
                    std::wstring c2Error;
                    if (ReadBinaryFile(c2Path, c2Bytes, c2Error)) {
                        (void)impl_->native->LoadC2Rom(c2Bytes.data(), c2Bytes.size());
                    }
                }
            }
        } else if (!paths.combined.empty()) {
            std::vector<std::uint8_t> combinedBytes;
            std::wstring readError;
            if (!ReadBinaryFile(paths.combined, combinedBytes, readError)) {
                state_ = State::Faulted;
                diagnostic_ = std::move(readError);
                return false;
            }
            loaded = impl_->native->LoadFirmware(combinedBytes.data(), combinedBytes.size());
        } else {
            impl_->native.reset();
            state_ = State::Unavailable;
            diagnostic_ =
                L"CG50 ROM-native: falta fxcg50_system.rom (o CG50_SYSTEM_ROM). "
                L"No se usará GCal/DLSim como sustituto del emulador real.";
            return false;
        }
    }

    if (!loaded) {
        state_ = State::Faulted;
        diagnostic_ = impl_->native->Diagnostic();
        return false;
    }

    engine_ = Engine::RomNative;
    state_ = State::Ready;
    stages_.nativeFirmwareLoaded = true;
    stages_.nativeCpuReady = true;
    stages_.stage10_cpuExecuting = true;
    stages_.stage11_keyInjectionActive = true;
    stages_.currentStage = 2;
    CaptureFramebuffer();

    diagnostic_ = impl_->native->Diagnostic();
    if (!stages_.nativeFramebufferAvailable) {
        diagnostic_ +=
            L"; host unificado conectado al core nativo; framebuffer R61524 aún no implementado";
    }
    return true;
}

bool Cg50Backend::InitializeParts(const std::uint8_t* systemRom,
                                  const std::size_t systemSize,
                                  const std::uint8_t* userRom,
                                  const std::size_t userSize,
                                  const std::uint8_t* c2Rom,
                                  const std::size_t c2Size)
{
    // Fase 2 (2026-11-02): el host entrega las partes RAW ya leídas (el
    // core nativo repite la policía de tamaños con diagnóstico honesto).
    // Espejo mecánico del arranque por partes del camino de descubrimiento
    // de Initialize() (LoadFirmwareParts y después LoadC2Rom, el mismo
    // orden verificado en las corridas canónicas), sin tocar ficheros ni
    // env: sin descubrimiento, sin persist.
    UnloadHost();
    stages_ = StageStatus{};
    state_ = State::Unavailable;
    engine_ = Engine::None;
    diagnostic_ = L"CG50 ROM-native: montando partes RAW (System/User/C2)";

    if (systemRom == nullptr || systemSize == 0 ||
        (userRom == nullptr) != (userSize == 0) ||
        (c2Rom == nullptr) != (c2Size == 0)) {
        state_ = State::Faulted;
        diagnostic_ = L"CG50 ROM-native: partes/tamaños incoherentes";
        return false;
    }

    impl_->native = std::make_unique<Cg50NativeCore>();
    // Opciones sticky al handle (fase 3, 2026-11-03): el RTC virtual vive
    // en el core nativo y cada re-carga crea un nativo nuevo, así que la
    // bandera guardada se re-aplica aquí. SLEEP_ADVANCE es del backend y no
    // necesita re-aplicarse.
    impl_->native->SetRtcVirtualClockEnabled(optionRtcVirtual_);
    const bool loaded = impl_->native->LoadFirmwareParts(
        systemRom, systemSize, userRom, userSize);
    if (loaded && c2Rom != nullptr) {
        // C2 explícita del host: si la placa la rechaza, el montaje FS
        // falla (arranque System-only: parada en MEDIA-FACTORY/RECINIT a
        // ~10.4M inst, write fault en la ventana C2 [0x00B60000,0x01000000),
        // verificado en el smoke del ABI 2026-11-02). Fallo cerrado con el
        // diagnóstico del core.
        if (!impl_->native->LoadC2Rom(c2Rom, c2Size)) {
            state_ = State::Faulted;
            diagnostic_ = impl_->native->Diagnostic();
            return false;
        }
    }

    if (!loaded) {
        state_ = State::Faulted;
        diagnostic_ = impl_->native->Diagnostic();
        return false;
    }

    engine_ = Engine::RomNative;
    state_ = State::Ready;
    stages_.nativeFirmwareLoaded = true;
    stages_.nativeCpuReady = true;
    stages_.stage10_cpuExecuting = true;
    stages_.stage11_keyInjectionActive = true;
    stages_.currentStage = 2;
    CaptureFramebuffer();

    diagnostic_ = impl_->native->Diagnostic();
    if (!stages_.nativeFramebufferAvailable) {
        diagnostic_ +=
            L"; host unificado conectado al core nativo; framebuffer R61524 aún no implementado";
    }
    return true;
}

void Cg50Backend::Reset()
{
    if (!impl_ || !impl_->native || engine_ != Engine::RomNative) {
        state_ = State::Unavailable;
        diagnostic_ = L"CG50 ROM-native: reset solicitado sin firmware RAW";
        return;
    }

    impl_->native->ResetCpuState();
    state_ = State::Ready;
    stages_.nativeCpuReady = true;
    stages_.stage10_cpuExecuting = true;
    stages_.totalCpuCycles = 0;
    stages_.frameCount = 0;
    diagnostic_ = impl_->native->Diagnostic();
}

bool Cg50Backend::SetOption(std::uint32_t option, std::uint32_t value)
{
    // Mapa ABI (regla 10, fail-closed): SLEEP_ADVANCE = 1 (Cg50Option);
    // RTC_VIRTUAL = 2 (fase 3, 2026-11-03) activa el reloj RTC virtual
    // determinista del core; desconocidas: sin plomería registrada ->
    // sin cambio.
    if (option == 1u /* CG50CORE_OPT_SLEEP_ADVANCE */) {
        optionSleepAdvance_ = (value != 0);
        return true;
    }
    if (option == 2u /* CG50CORE_OPT_RTC_VIRTUAL */) {
        optionRtcVirtual_ = (value != 0);
        if (engine_ == Engine::RomNative && impl_ && impl_->native) {
            impl_->native->SetRtcVirtualClockEnabled(optionRtcVirtual_);
        }
        return true;
    }
    return false;
}

void Cg50Backend::Step(const std::uint32_t elapsedMicroseconds)
{
    if (state_ != State::Ready || engine_ != Engine::RomNative ||
        !impl_ || !impl_->native) {
        return;
    }

    auto& core = *impl_->native;
    const auto stateBefore = core.GetState();
    const auto cyclesBefore = core.GetStats().cycles;

    if (stateBefore == Cg50NativeCore::State::Running) {
        core.RunInstructions(InstructionBudgetForTick(elapsedMicroseconds));
    } else if (stateBefore == Cg50NativeCore::State::Sleeping &&
               (optionSleepAdvance_ ||
                std::getenv("CG50_SLEEP_ADVANCE") != nullptr)) {
        // El guest duerme entre eventos. Sin avanzar el reloj virtual, los
        // temporizadores (TMU/CMT/WDT/RTC) nunca vencen y el OS no despierta:
        // la GUI se quedaba congelada en SLEEP hasta que llegaba otra tecla.
        // AdvanceCycles consume el tiempo dormido, entrega las IRQ pendientes
        // y devuelve true cuando el CPU vuelve a Running.
        core.AdvanceCycles(InstructionBudgetForTick(elapsedMicroseconds));
        if (core.GetState() == Cg50NativeCore::State::Running) {
            core.RunInstructions(InstructionBudgetForTick(elapsedMicroseconds));
        }
    }

    const auto cyclesAfter = core.GetStats().cycles;
    stages_.totalCpuCycles += cyclesAfter - cyclesBefore;
    stages_.nativeCpuReady = core.GetState() != Cg50NativeCore::State::Empty;
    stages_.stage10_cpuExecuting = core.GetState() == Cg50NativeCore::State::Running;

    if (core.GetState() == Cg50NativeCore::State::Halted) {
        state_ = State::Faulted;
        stages_.stage10_cpuExecuting = false;
        diagnostic_ = core.Diagnostic();
        return;
    }

    if (core.GetState() == Cg50NativeCore::State::Sleeping) {
        diagnostic_ = L"CG50 ROM-native: CPU en SLEEP, esperando una interrupción real/modelada";
    } else {
        diagnostic_ = core.Diagnostic();
    }

    CaptureFramebuffer();
    if (stages_.nativeFramebufferAvailable) ++stages_.frameCount;

    // Observabilidad opt-in del nucleo nativo (CG50_STEP_LOG=1): una linea
    // cada ~10 ticks con el contador real de instrucciones, ciclos, estado y
    // PC. Sin esta traza el log solo mostraba Initialize/READY y no habia
    // forma de saber si el guest avanzaba o estaba congelado en SLEEP.
    static const bool stepLog = std::getenv("CG50_STEP_LOG") != nullptr;
    static unsigned stepCount = 0;
    if (stepLog && (++stepCount % 10u) == 0u) {
        const auto& st = core.GetStats();
        if (FILE* f = std::fopen("calc_debug_step.log", "a")) {
            std::fprintf(f, "inst=%llu cyc=%llu state=%d pc=%08X\n",
                         static_cast<unsigned long long>(st.instructions),
                         static_cast<unsigned long long>(st.cycles),
                         static_cast<int>(core.GetState()),
                         core.GetRegisters().pc);
            std::fclose(f);
        }
    }
}

bool Cg50Backend::KeyDown(const std::uint16_t matrixCode)
{
    if (state_ != State::Ready || engine_ != Engine::RomNative ||
        !impl_ || !impl_->native) {
        return false;
    }
    return impl_->native->KeyDown(matrixCode);
}

void Cg50Backend::KeyUp(const std::uint16_t matrixCode)
{
    if (engine_ != Engine::RomNative || !impl_ || !impl_->native) return;
    impl_->native->KeyUp(matrixCode);
}

bool Cg50Backend::SaveStateToFile(const wchar_t* path)
{
    if (engine_ != Engine::RomNative || !impl_ || !impl_->native ||
        path == nullptr) {
        return false;
    }
    std::vector<std::uint8_t> bytes;
    if (!impl_->native->ExportState(bytes)) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    return output.good();
}

bool Cg50Backend::LoadStateFromFile(const wchar_t* path)
{
    if (engine_ != Engine::RomNative || !impl_ || !impl_->native ||
        path == nullptr) {
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    if (bytes.empty()) return false;
    if (!impl_->native->ImportState(bytes.data(), bytes.size())) {
        state_ = State::Faulted;
        diagnostic_ = impl_->native->Diagnostic();
        return false;
    }
    state_ = State::Ready;
    diagnostic_ = impl_->native->Diagnostic();
    stages_.nativeCpuReady = true;
    CaptureFramebuffer();
    return true;
}

bool Cg50Backend::SaveStateToBuffer(std::vector<std::uint8_t>& bytes) const
{
    if (engine_ != Engine::RomNative || !impl_ || !impl_->native) {
        return false;
    }
    return impl_->native->ExportState(bytes);
}

bool Cg50Backend::LoadStateFromBuffer(const std::uint8_t* bytes,
                                      std::size_t const size)
{
    if (engine_ != Engine::RomNative || !impl_ || !impl_->native ||
        bytes == nullptr || size == 0) {
        return false;
    }
    // Todo-o-nada al nivel del ImportState del core; si el core marca
    // Halted con un blob cochino, el backend lo refleja (Faulted +
    // diagnóstico del core) y el host lee estado/diagnóstico: honesto por
    // diseño, igual que el camino por ficheros de arriba.
    if (!impl_->native->ImportState(bytes, size)) {
        state_ = State::Faulted;
        diagnostic_ = impl_->native->Diagnostic();
        return false;
    }
    state_ = State::Ready;
    diagnostic_ = impl_->native->Diagnostic();
    stages_.nativeCpuReady = true;
    CaptureFramebuffer();
    return true;
}

const std::uint8_t* Cg50Backend::UserRomBytes() const noexcept
{
    if (engine_ != Engine::RomNative || !impl_ || !impl_->native ||
        !impl_->native->HasUserRom()) {
        return nullptr;
    }
    return impl_->native->UserRomData();
}

std::size_t Cg50Backend::UserRomSize() const noexcept
{
    if (engine_ != Engine::RomNative || !impl_ || !impl_->native ||
        !impl_->native->HasUserRom()) {
        return 0;
    }
    return impl_->native->UserRomSize();
}

bool Cg50Backend::ReplaceUserRom(const std::uint8_t* bytes, const std::size_t size)
{
    if (engine_ != Engine::RomNative || !impl_ || !impl_->native) return false;
    if (!impl_->native->ReplaceUserRom(bytes, size)) {
        diagnostic_ = impl_->native->Diagnostic();
        return false;
    }
    state_ = State::Ready;
    diagnostic_ = impl_->native->Diagnostic();
    CaptureFramebuffer();
    return true;
}

bool Cg50Backend::LoadG3aAddin(const std::uint8_t* bytes, const std::size_t size)
{
    if (engine_ != Engine::RomNative || !impl_ || !impl_->native) return false;
    const bool ok = impl_->native->LoadG3aAddin(bytes, size);
    diagnostic_ = impl_->native->Diagnostic();
    return ok;
}

bool Cg50Backend::SetRtcEpochSeconds(const std::int64_t localUnixSeconds) noexcept
{
    if (engine_ != Engine::RomNative || !impl_ || !impl_->native) return false;
    impl_->native->SetRtcEpochSeconds(localUnixSeconds);
    return true;
}

bool Cg50Backend::PersistUserRom()
{
    if (engine_ != Engine::RomNative || !impl_ || !impl_->native ||
        !impl_->native->HasUserRom() || impl_->userPath.empty()) {
        return false;
    }
    const fs::path persisted = impl_->userPath.parent_path() /
                               (impl_->userPath.stem().wstring() +
                                L"_persist.raw");
    std::ofstream output(persisted, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(
        reinterpret_cast<const char*>(impl_->native->UserRomData()),
        static_cast<std::streamsize>(impl_->native->UserRomSize()));
    return output.good();
}

bool Cg50Backend::HasPersistedUserRom() const
{
    if (engine_ != Engine::RomNative || !impl_ || impl_->userPath.empty()) {
        return false;
    }
    const fs::path persisted = impl_->userPath.parent_path() /
                               (impl_->userPath.stem().wstring() +
                                L"_persist.raw");
    std::error_code ec;
    return fs::is_regular_file(persisted, ec) && !ec;
}

Cg50Backend::FramebufferView Cg50Backend::Framebuffer()
{
    if (state_ != State::Ready || engine_ != Engine::RomNative) return {};
    if (framebufferCopy_.empty()) CaptureFramebuffer();
    if (framebufferCopy_.empty()) return {};

    return {
        framebufferCopy_.data(),
        fbWidth_,
        fbHeight_,
        fbStride_,
        PixelFormat::Bgra32
    };
}

Cg50Backend::RuntimeStatus Cg50Backend::GetRuntimeStatus() const noexcept
{
    RuntimeStatus out{};
    if (!impl_ || !impl_->native) return out;

    const auto& core = *impl_->native;
    const auto& regs = core.GetRegisters();
    const auto& stats = core.GetStats();

    out.cpuState = PublicCpuState(core.GetState());
    out.systemRomLoaded = core.HasSystemRom();
    out.userRomLoaded = core.HasUserRom();
    out.framebufferAvailable = core.Peripherals().Lcd().FramebufferAvailable();
    out.pc = regs.pc;
    out.sr = regs.sr;
    out.instructions = stats.instructions;
    out.cycles = stats.cycles;
    out.exceptionsTaken = stats.exceptionsTaken;
    out.interruptsAccepted = stats.interruptsAccepted;
    out.lastExceptionEvent = stats.lastExceptionEvent;
    out.lastInterruptEvent = stats.lastInterruptEvent;
    out.memoryFaultSeen = stats.lastMemoryFaultAddress.updates != 0;
    out.lastMemoryFaultAddress = stats.lastMemoryFaultAddress.value;
    out.lastMemoryFaultWasWrite = stats.lastMemoryFaultWasWrite;
    out.unsupportedOpcodeSeen = stats.unsupportedOpcode.updates != 0;
    out.unsupportedPc = stats.unsupportedPc;
    out.unsupportedOpcode = stats.unsupportedOpcode.value;
    return out;
}

std::wstring Cg50Backend::StatusLabel() const
{
    if (engine_ != Engine::RomNative) {
        return state_ == State::Faulted
            ? L"ROM-native: firmware inválido/error"
            : L"ROM-native: falta SystemROM RAW";
    }

    const auto runtime = GetRuntimeStatus();
    if (state_ == State::Faulted || runtime.cpuState == CpuState::Halted)
        return L"ROM-native: core detenido";
    if (runtime.cpuState == CpuState::Sleeping)
        return L"ROM-native: SLEEP / espera IRQ";
    if (runtime.framebufferAvailable)
        return L"ROM-native: CPU + framebuffer activos";
    if (runtime.cpuState == CpuState::Running)
        return L"ROM-native: CPU activa / LCD pendiente";
    return L"ROM-native: firmware cargado";
}