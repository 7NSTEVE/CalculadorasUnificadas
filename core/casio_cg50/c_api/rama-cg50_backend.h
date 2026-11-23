#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Cg50Backend final {
public:
    enum class State {
        Unavailable,
        Ready,
        Faulted
    };

    enum class Engine {
        None,
        RomNative
    };

    enum class CpuState {
        Empty,
        Running,
        Sleeping,
        Halted
    };

    enum class PixelFormat {
        None,
        Bgra32
    };

    struct FramebufferView {
        const std::uint32_t* pixels = nullptr;
        std::size_t width = 0;
        std::size_t height = 0;
        std::size_t stridePixels = 0;
        PixelFormat format = PixelFormat::None;
    };

    // UI-facing snapshot. It deliberately exposes architectural/diagnostic data
    // without leaking Cg50NativeCore internals into main.cpp.
    struct RuntimeStatus {
        CpuState cpuState = CpuState::Empty;
        bool systemRomLoaded = false;
        bool userRomLoaded = false;
        bool framebufferAvailable = false;
        std::uint32_t pc = 0;
        std::uint32_t sr = 0;
        std::uint64_t instructions = 0;
        std::uint64_t cycles = 0;
        std::uint64_t exceptionsTaken = 0;
        std::uint64_t interruptsAccepted = 0;
        std::uint32_t lastExceptionEvent = 0;
        std::uint32_t lastInterruptEvent = 0;
        bool memoryFaultSeen = false;
        std::uint32_t lastMemoryFaultAddress = 0;
        bool lastMemoryFaultWasWrite = false;
        bool unsupportedOpcodeSeen = false;
        std::uint32_t unsupportedPc = 0;
        std::uint16_t unsupportedOpcode = 0;
    };

    // Kept structurally compatible with the existing UI while exposing explicit
    // ROM-native milestones. Legacy GCal stages are never activated in production.
    struct StageStatus {
        bool stage1_activationLoaded = false;
        bool stage2_activationPatched = false;
        bool stage3_gcalLoaded = false;
        bool stage4_exportsResolved = false;
        bool stage5_createEmulatorOk = false;
        bool stage6_createResidentTaskOk = false;
        bool stage7_requestResidentTaskOk = false;
        bool stage8_dlsimActive = false;
        bool stage9_lcdActive = false;
        bool stage10_cpuExecuting = false;
        bool stage11_keyInjectionActive = false;
        bool nativeFirmwareLoaded = false;
        bool nativeCpuReady = false;
        bool nativeFramebufferAvailable = false;
        std::uint32_t currentStage = 0;
        std::uint32_t totalStages = 3;
        std::uint64_t totalCpuCycles = 0;
        std::uint32_t frameCount = 0;
    };

    Cg50Backend();
    ~Cg50Backend();
    Cg50Backend(const Cg50Backend&) = delete;
    Cg50Backend& operator=(const Cg50Backend&) = delete;

    // If firmware is supplied it must be a RAW SystemROM or combined RAW image.
    // With nullptr,0 the backend searches the documented local/env ROM locations.
    bool Initialize(const std::uint8_t* firmware, std::size_t firmwareSize);

    // Explicit UI/file-picker path. Accepts a SystemROM or a combined RAW image.
    // When a SystemROM is selected, a standard-named UserROM in the same directory
    // is loaded automatically if present. A UserROM by itself is rejected.
    bool InitializeFromFile(const wchar_t* firmwarePath);

    // Camino por bytes para la ABI C (fase 2, 2026-11-02): partes RAW ya
    // leídas por el host, tamaños exactos (0x00B60000 / 0x014A0000 /
    // 0x004A0000; user/C2 opcionales, null + 0). Espejo mecánico del
    // arranque por partes del camino de descubrimiento de Initialize()
    // (LoadFirmwareParts y luego LoadC2Rom, el orden ya verificado), sin
    // tocar ficheros ni env: sin descubrimiento, sin persist.
    bool InitializeParts(const std::uint8_t* systemRom, std::size_t systemSize,
                         const std::uint8_t* userRom, std::size_t userSize,
                         const std::uint8_t* c2Rom, std::size_t c2Size);

    // Opción opt-in de la ABI (fase 2, 2026-11-02): SLEEP_ADVANCE (id 1 de
    // Cg50Option) permite que Step despierte al guest dormido vía
    // AdvanceCycles — el equivalente programático del opt-in por env del
    // build de escritorio (CG50_SLEEP_ADVANCE), que PRESERVE. RTC_VIRTUAL y
    // opciones desconocidas quedan fail-closed (devuelve false, regla 10).
    bool SetOption(std::uint32_t option, std::uint32_t value);

    void Reset();
    void Step(std::uint32_t elapsedMicroseconds);
    bool KeyDown(std::uint16_t matrixCode);
    void KeyUp(std::uint16_t matrixCode);
    FramebufferView Framebuffer();

    // Save-state of the full native machine (CPU+FPU, RAM, storage, MMU,
    // peripheral bus). Returns false when the engine is not RomNative.
    bool SaveStateToFile(const wchar_t* path);
    bool LoadStateFromFile(const wchar_t* path);

    // Camino por bytes para la ABI C (fase 3, 2026-11-03): mismos blobs que
    // SaveStateToFile/LoadStateFromFile (79 chunks + bus, kStateVersion=2)
    // sin tocar ficheros — el host guarda/lee los bytes de su lado
    // (cg50core_save/cg50core_restore). LoadStateFromBuffer es todo-o-nada
    // a nivel del Cg50NativeCore::ImportState del core; el estado del
    // backend refleja el desenlace (Ready o Faulted con diagnóstico).
    bool SaveStateToBuffer(std::vector<std::uint8_t>& bytes) const;
    bool LoadStateFromBuffer(const std::uint8_t* bytes, std::size_t size);

    // Camino por bytes para la ABI C (fase 4, 2026-11-03): lectura de la
    // UserROM cargada para la persistencia OPT-IN del HOST (la ABI nunca
    // escribe: el host guarda «<UserROM>_persist.raw» en su app-data y
    // HasPersistedUserRom equivale a "el fichero existe", lado host).
    // nullptr/0 sin UserROM cargada (el pin "read fase 1 = 0" queda igual).
    const std::uint8_t* UserRomBytes() const noexcept;
    std::size_t UserRomSize() const noexcept;
    bool ReplaceUserRom(const std::uint8_t* bytes, std::size_t size);
    bool LoadG3aAddin(const std::uint8_t* bytes, std::size_t size);
    bool SetRtcEpochSeconds(std::int64_t localUnixSeconds) noexcept;

    // Storage persistence: writes the current (mutated) UserROM image next
    // to the firmware it was loaded from, as <UserROM_persist.raw>. The
    // backend loads that persisted image on the next Initialize() instead of
    // the pristine dump, so OS formatting/mutations survive sessions. Opt-in
    // via CG50_PERSIST_STORAGE=1 (the pristine golden baseline is never
    // modified in place).
    bool PersistUserRom();
    bool HasPersistedUserRom() const;

    State GetState() const noexcept { return state_; }
    Engine GetEngine() const noexcept { return engine_; }
    bool IsRomNative() const noexcept { return engine_ == Engine::RomNative; }
    const std::wstring& Diagnostic() const noexcept { return diagnostic_; }
    const StageStatus& GetStages() const noexcept { return stages_; }
    RuntimeStatus GetRuntimeStatus() const noexcept;
    std::wstring StatusLabel() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    State state_ = State::Unavailable;
    Engine engine_ = Engine::None;
    std::wstring diagnostic_ =
        L"CG50 ROM-native: SystemROM RAW todavía no cargada";
    StageStatus stages_{};
    std::vector<std::uint32_t> framebufferCopy_;
    std::size_t fbWidth_ = 0;
    std::size_t fbHeight_ = 0;
    std::size_t fbStride_ = 0;
    /* Fase 2 (2026-11-02): SLEEP_ADVANCE por ABI (sticky al handle). */
    bool optionSleepAdvance_ = false;
    /* Fase 3 (2026-11-03): RTC virtual por ABI (sticky al handle; se
     * re-aplica al core nativo en cada re-carga de firmware). */
    bool optionRtcVirtual_ = false;

    void UnloadHost();
    bool CaptureFramebuffer();
};