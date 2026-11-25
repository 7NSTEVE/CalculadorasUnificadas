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
        GCal,
        RomNative
    };

    // Runtime-selectable execution mode (Windows dual-engine + live comparator)
    enum class EngineMode {
        NativeIndependent = 0, // Core propio SH7305 v47 (independiente de DLLs Casio)
        OfficialCasioDll  = 1, // Core oficial Casio (DLLs originales via host 32-bit aislado)
        CompareLive       = 2  // Comparador en vivo: ambos motores en lockstep
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

    struct LiveCompareMetrics {
        bool officialReady = false;
        bool nativeReady = false;
        std::uint32_t width = 384;
        std::uint32_t height = 216;
        std::uint32_t totalPixels = 384 * 216;
        std::uint32_t exactMatchPixels = 0;
        std::uint32_t diffPixels = 0;
        double matchPercent = 0.0;
        std::uint32_t maxChannelDelta = 0;
        double meanAbsError = 0.0;
        double officialStepMs = 0.0;
        double nativeStepMs = 0.0;
        std::uint64_t syncSteps = 0;
        std::uint64_t syncKeysDispatched = 0;
        std::wstring officialDiag;
        std::wstring nativeDiag;
    };

    // UI-facing snapshot. It deliberately exposes architectural/diagnostic data
    // without leaking emulator internals into main.cpp.
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
        std::uint32_t totalStages = 11;
        std::uint64_t totalCpuCycles = 0;
        std::uint32_t frameCount = 0;
    };

    Cg50Backend();
    ~Cg50Backend();
    Cg50Backend(const Cg50Backend&) = delete;
    Cg50Backend& operator=(const Cg50Backend&) = delete;

    bool Initialize(const std::uint8_t* firmware, std::size_t firmwareSize);
    bool InitializeFromFile(const wchar_t* firmwarePath);

    // Runtime dual-engine mode control (Native v47 / Official Casio DLL / Live Compare)
    bool SetEngineMode(EngineMode mode);
    EngineMode GetEngineMode() const noexcept { return engineMode_; }
    bool IsOfficialHelperAvailable() const;
    FramebufferView NativeFramebuffer();
    FramebufferView OfficialFramebuffer();
    LiveCompareMetrics ComputeLiveComparison(std::vector<std::uint32_t>* outDiffHeatmapBgra = nullptr);

    void Reset();
    void Step(std::uint32_t elapsedMicroseconds);
    bool KeyDown(std::uint16_t matrixCode);
    void KeyUp(std::uint16_t matrixCode);
    bool KeyStroke(std::uint16_t matrixCode, std::uint32_t holdMs = 60);
    FramebufferView Framebuffer();

    bool SaveStateToFile(const wchar_t* path);
    bool LoadStateFromFile(const wchar_t* path);
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
    EngineMode engineMode_ = EngineMode::NativeIndependent;
    std::wstring diagnostic_ =
        L"N\u00FAcleo SH7305/SH-4A y firmware fx-CG50 pendientes de inicializar";
    StageStatus stages_{};
    std::vector<std::uint32_t> framebufferCopy_;
    std::size_t fbWidth_ = 0;
    std::size_t fbHeight_ = 0;
    std::size_t fbStride_ = 0;

    void UnloadHost();
    bool CaptureFramebuffer();
};

namespace Cg50PathDiagnostics {

std::vector<std::wstring> ResolveRuntimeSearchCandidates(
    const std::wstring& exeDir,
    const std::wstring& cwd);

bool ValidateActivationPatchSiteBytes(
    const std::uint8_t* mem,
    std::size_t avail,
    const std::uint8_t* expectedPatched,
    std::size_t len,
    bool& outAlreadyPatched);

} // namespace Cg50PathDiagnostics
