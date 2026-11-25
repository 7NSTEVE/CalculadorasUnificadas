#include "cg50_backend.h"
#include "cg50_keys_exact.h"
#include "classwiz_keys_exact.h"
#include "calc_engine.h"
#include "../linux/engines/tests/u13_lcd_font_helper.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int Fail(const char* what)
{
    std::cerr << "FAIL: " << what << "\n";
    return 1;
}

} // namespace

int main()
{
    if (!Cg50KeyCodesAreUnique()) {
        return Fail("CG50 key codes are not unique");
    }
    if (NUM_CG50_KEYS != 50) {
        return Fail("NUM_CG50_KEYS");
    }
    if (g_cg50ExactKeys[0].matrixCode == g_classwizExactKeys[0].matrixCode) {
        return Fail("CG50 codes must not reuse nX-U8 8-bit F1/SHIFT layout");
    }
    if (g_cg50ExactKeys[0].matrixCode != 0x070A) {
        return Fail("F1 must be 16-bit 0x070A");
    }

    // A-07: Verify runtime search candidate literals have no unescaped control chars (\r, \n, \t)
    {
        const std::vector<std::wstring> cands =
            Cg50PathDiagnostics::ResolveRuntimeSearchCandidates(LR"(D:\App\bin)", LR"(D:\App)");
        bool sawBuildVerify = false;
        bool sawProgramFiles = false;
        for (const auto& cand : cands) {
            for (wchar_t ch : cand) {
                if (ch < 32) {
                    return Fail("A-07 control character found in runtime search candidate");
                }
            }
            if (cand.find(LR"(build-verify\Release\runtime)") != std::wstring::npos) {
                sawBuildVerify = true;
            }
            if (cand == LR"(C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series)") {
                sawProgramFiles = true;
            }
        }
        if (!sawBuildVerify || !sawProgramFiles) {
            return Fail("A-07 expected runtime search candidates missing or corrupted");
        }
        std::cout << "[PASS] A-07 runtime search candidates verified (" << cands.size() << " paths)\n";
    }

    // A-08: Verify activation patch site validator accepts pre-patched/prologue bytes and rejects garbage
    {
        const std::uint8_t patched[8] = { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC2, 0x04, 0x00 };
        const std::uint8_t prologue[8] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x6A, 0xFF };
        const std::uint8_t garbage[8] = { 0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0x90, 0x90, 0x90 };
        bool already = false;
        if (!Cg50PathDiagnostics::ValidateActivationPatchSiteBytes(patched, 8, patched, 8, already) || !already) {
            return Fail("A-08 failed to recognize pre-patched activation bytes");
        }
        if (!Cg50PathDiagnostics::ValidateActivationPatchSiteBytes(prologue, 8, patched, 8, already) || already) {
            return Fail("A-08 failed to recognize original Casio x86 function prologue");
        }
        if (Cg50PathDiagnostics::ValidateActivationPatchSiteBytes(garbage, 8, patched, 8, already)) {
            return Fail("A-08 accepted incompatible/corrupt opcode bytes");
        }
        if (Cg50PathDiagnostics::ValidateActivationPatchSiteBytes(prologue, 3, patched, 8, already)) {
            return Fail("A-08 accepted truncated buffer");
        }
        std::cout << "[PASS] A-08 activation signature validator verified\n";
    }

    std::cout << "[TEST] Starting..." << std::endl;
    Cg50Backend backend;
    std::cout << "[TEST] Created backend, state=" << (int)backend.GetState() << std::endl;
    if (backend.GetState() != Cg50Backend::State::Unavailable) {
        return Fail("initial state must be Unavailable");
    }
    if (backend.Diagnostic().empty()) {
        return Fail("initial diagnostic empty");
    }
    if (backend.GetEngine() != Cg50Backend::Engine::None || backend.IsRomNative()) {
        return Fail("initial engine must be None and not RomNative");
    }
    if (backend.StatusLabel().empty()) {
        return Fail("initial StatusLabel must not be empty");
    }
    if (backend.GetRuntimeStatus().cpuState != Cg50Backend::CpuState::Empty) {
        return Fail("initial RuntimeStatus cpuState must be Empty");
    }
    if (backend.HasPersistedUserRom() || backend.SaveStateToFile(L"x.sav") ||
        backend.LoadStateFromFile(L"x.sav") || backend.PersistUserRom()) {
        return Fail("Unavailable GCal backend persistence stubs must return false");
    }

    // Verify InitializeFromFile(nonexistent) sets Faulted with diagnostic
    {
        Cg50Backend missingFw;
        if (missingFw.InitializeFromFile(LR"(Q:\__nonexistent_cg50_fw_m1_4__.bin)") ||
            missingFw.GetState() != Cg50Backend::State::Faulted ||
            missingFw.Diagnostic().empty()) {
            return Fail("InitializeFromFile(nonexistent) must fail with Faulted and diagnostic");
        }
    }

    // NOTA: se inicializa UNA sola vez. El runtime real de CASIO no admite
    // re-crear el emulador en el mismo proceso tras TerminateEmulator con los
    // modulos residentes (crash 0xC0000005 en el segundo CreateEmulator).
    // El argumento de firmware se ignora por diseno (GCal usa el DLP local),
    // lo cual se verifica pasando bytes arbitrarios: si el host arranca, el
    // firmware no se uso.
    const std::uint8_t dummyFw[] = {1, 2, 3, 4};
    std::cout << "[TEST] Initializing with dummyFw..." << std::endl;
    const bool withFw = backend.Initialize(dummyFw, sizeof(dummyFw));
    std::cout << "[TEST] Initialize returned " << withFw << ", state=" << (int)backend.GetState() << std::endl;

    if (withFw) {
        if (backend.GetState() != Cg50Backend::State::Ready) {
            return Fail("Initialize succeeded but state is not Ready");
        }
        if (backend.GetEngine() != Cg50Backend::Engine::GCal || backend.IsRomNative()) {
            return Fail("Ready backend must report Engine::GCal and IsRomNative() == false");
        }
        if (backend.StatusLabel().empty()) {
            return Fail("Ready StatusLabel must not be empty");
        }
        const auto rt0 = backend.GetRuntimeStatus();
        if (rt0.cpuState != Cg50Backend::CpuState::Running || !rt0.systemRomLoaded || !rt0.userRomLoaded) {
            return Fail("Ready RuntimeStatus must report Running + loaded system/user ROMs");
        }
        if (backend.SaveStateToFile(L"x.sav") || backend.LoadStateFromFile(L"x.sav") ||
            backend.PersistUserRom() || backend.HasPersistedUserRom()) {
            return Fail("GCal persistence stubs must return false cleanly while Ready");
        }
        auto fb = backend.Framebuffer();
        if (!fb.pixels || fb.width == 0 || fb.height == 0) {
            return Fail("Ready backend returned empty framebuffer");
        }
        if (fb.format != Cg50Backend::PixelFormat::Bgra32) {
            return Fail("expected BGRA32 framebuffer");
        }
        std::cout << "[TEST] Framebuffer " << fb.width << "x" << fb.height << std::endl;
        if (!backend.KeyDown(0x0101)) {
            return Fail("AC KeyDown failed while Ready");
        }
        backend.KeyUp(0x0101);
        std::cout << "[TEST] Key injection OK" << std::endl;
        for (int frame = 0; frame < 3000; ++frame) {
            backend.Step(16000);
        }
        std::cout << "[TEST] Stepped 3000 frames (LCD settle)" << std::endl;
        const auto fbAfterKeys = backend.Framebuffer();
        std::size_t nonzeroPixels = 0;
        for (std::size_t i = 0;
             fbAfterKeys.pixels != nullptr &&
                 i < fbAfterKeys.width * fbAfterKeys.height;
             ++i) {
            if (fbAfterKeys.pixels[i] != 0) {
                ++nonzeroPixels;
            }
        }
        std::cout << "[TEST] Framebuffer nonzero_pixels=" << nonzeroPixels << "/"
                  << fbAfterKeys.width * fbAfterKeys.height << std::endl;
        // Contrato del slot 0xE (2026-10-17): el MAIN MENU renderiza iconos
        // coloridos; la pantalla blanca del bucle pre-0xE es monocroma. Se mide
        // el conteo de pixeles con canales RGB distintos (BGRA32 en LE:
        // b=p&0xFF, g=(p>>8)&0xFF, r=(p>>16)&0xFF) para asertarlo como
        // contrato de regresion del slot 0xE (ver INFORME 2026-10-16 seccion 19).
        std::size_t colorfulPixels = 0;
        for (std::size_t i = 0;
             fbAfterKeys.pixels != nullptr &&
                 i < fbAfterKeys.width * fbAfterKeys.height;
             ++i) {
            const std::uint32_t p = fbAfterKeys.pixels[i];
            const std::uint32_t b = p & 0xFFu;
            const std::uint32_t g = (p >> 8) & 0xFFu;
            const std::uint32_t r = (p >> 16) & 0xFFu;
            if (r != g || g != b) {
                ++colorfulPixels;
            }
        }
        std::cout << "[TEST] Framebuffer colorful_pixels=" << colorfulPixels << "/"
                  << fbAfterKeys.width * fbAfterKeys.height << std::endl;
        // Contrato de regresion del slot 0xE (2026-10-17): medido 41320/88704
        // con la tarea 0xE activa (menu con iconos coloridos); la pantalla blanca
        // del bucle pre-0xE es monocroma (colorful ~ 0). Umbral 20000 = 2x margen.
        if (colorfulPixels < 20000) {
            return Fail("slot 0xE contract: MAIN MENU icons missing (colorful_pixels < 20000)");
        }
    } else {
        if (backend.GetState() == Cg50Backend::State::Ready) {
            return Fail("Initialize failed but state is Ready");
        }
        if (backend.GetState() != Cg50Backend::State::Unavailable &&
            backend.GetState() != Cg50Backend::State::Faulted) {
            return Fail("failed init must be Unavailable or Faulted");
        }
        if (backend.Diagnostic().empty()) {
            return Fail("failed init must set diagnostic");
        }
        auto fb = backend.Framebuffer();
        if (fb.pixels != nullptr) {
            return Fail("framebuffer must be empty when not Ready");
        }
        if (backend.KeyDown(0x0101)) {
            return Fail("KeyDown must fail when not Ready");
        }
        std::wcout << L"GCal ausente o falló (esperado en CI hermético): "
                   << backend.Diagnostic() << L"\n";
    }

    std::cout << "[TEST] Resetting..." << std::endl;
    backend.Reset();
    std::cout << "[TEST] Reset completed" << std::endl;
    if (!backend.Diagnostic().empty()) {
        return Fail("Reset() must clear diagnostic");
    }
    if (backend.GetState() != Cg50Backend::State::Unavailable) {
        return Fail("Reset() must return to Unavailable");
    }
    if (withFw) {
        auto fb = backend.Framebuffer();
        if (fb.pixels != nullptr) {
            return Fail("framebuffer must be empty after Reset");
        }
        if (backend.KeyDown(0x0101)) {
            return Fail("KeyDown must fail after Reset");
        }
    }

    std::cout << "[TEST] Second Initialize after Reset()..." << std::endl;
    const bool secondInit = backend.Initialize(dummyFw, sizeof(dummyFw));
    std::cout << "[TEST] Second Initialize returned " << secondInit
              << ", state=" << (int)backend.GetState() << std::endl;
    if (withFw && (!secondInit || backend.GetState() != Cg50Backend::State::Ready)) {
        return Fail("Second Initialize() failed to return Ready");
    }
    if (secondInit) {
        if (backend.GetState() != Cg50Backend::State::Ready) {
            return Fail("Second Initialize() succeeded but state is not Ready");
        }
        std::cout << "[TEST] Stepping 100 frames in second instance..." << std::endl;
        for (int frame = 0; frame < 100; ++frame) {
            backend.Step(16000);
        }
        std::cout << "[TEST] Second instance step OK" << std::endl;
        backend.Reset();
        std::cout << "[TEST] Second Reset OK" << std::endl;
    }

    // Verify GCAL_RUNTIME_DIR env override points to isolated temp dir safely without crashing
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path isoDir = fs::temp_directory_path(ec) / ("cg50_iso_env_" + std::to_string(GetCurrentProcessId()));
        fs::create_directories(isoDir, ec);
        {
            std::ofstream dummyDll(isoDir / "GCalAppIF.dll", std::ios::binary | std::ios::trunc);
        }
        SetEnvironmentVariableW(L"GCAL_RUNTIME_DIR", isoDir.wstring().c_str());
        Cg50Backend envProbe;
        const bool envOk = envProbe.Initialize(nullptr, 0);
        if (envOk || envProbe.GetState() != Cg50Backend::State::Faulted) {
            SetEnvironmentVariableW(L"GCAL_RUNTIME_DIR", nullptr);
            fs::remove_all(isoDir, ec);
            return Fail("GCAL_RUNTIME_DIR corrupt-dir override must fail cleanly with Faulted");
        }
        SetEnvironmentVariableW(L"GCAL_RUNTIME_DIR", nullptr);
        fs::remove_all(isoDir, ec);
        std::cout << "[PASS] GCAL_RUNTIME_DIR isolated probe completed\n";
    }

    // U-13 18-case differential check against MathEvaluator (calc_engine.h)
    {
        const auto& cases = U13Matrix::Get18Cases();
        if (cases.size() != 18) return Fail("U-13 matrix size != 18");
        for (const auto& c : cases) {
            std::wstring wExpr(c.expr, c.expr + std::strlen(c.expr));
            double val = 0.0;
            std::wstring err;
            if (!MathEvaluator::Evaluate(wExpr, 0.0, ANG_DEG, val, err)) {
                return Fail("U-13 MathEvaluator failed in Cg50BackendContractTest");
            }
            const std::wstring wFmt = MathEvaluator::FormatResult(val);
            const std::wstring wExp(c.expectedDot, c.expectedDot + std::strlen(c.expectedDot));
            if (wFmt != wExp) {
                return Fail("U-13 MathEvaluator result mismatch in Cg50BackendContractTest");
            }
        }
        std::cout << "[PASS] U-13 18-case MathEvaluator differential check (18/18)\n";
    }

    std::wcout << L"Cg50BackendContractTest OK\n";
    return 0;
}
