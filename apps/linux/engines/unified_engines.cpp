#include "unified_engines.hpp"

#include "cw2_fallback_lcd.hpp"
#include "key_tables_linux.hpp"
#include "lcd_indicators_linux.hpp"
#include "math_eval_linux.hpp"

#include "calcemu.h"
#include "casio_ffi.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace CasioLinux {

namespace fs = std::filesystem;

namespace {

bool IsRegularFile(const fs::path& p)
{
    std::error_code ec;
    return fs::exists(p, ec) && fs::is_regular_file(p, ec);
}

bool IsDirectory(const fs::path& p)
{
    std::error_code ec;
    return fs::exists(p, ec) && fs::is_directory(p, ec);
}

bool IsExecutableFile(const fs::path& p)
{
    return IsRegularFile(p) && ::access(p.c_str(), X_OK) == 0;
}

std::string NormalizePathString(const fs::path& p)
{
    std::error_code ec;
    fs::path canon = fs::weakly_canonical(p, ec);
    if (!ec) return canon.string();
    return p.lexically_normal().string();
}

void AddCandidateUnique(std::vector<fs::path>& list, const fs::path& p)
{
    if (p.empty()) return;
    std::error_code ec;
    fs::path norm = fs::weakly_canonical(p, ec);
    if (ec) norm = p.lexically_normal();
    for (const auto& existing : list) {
        if (existing == norm) return;
    }
    list.push_back(norm);
}

void AddAnchorAndAncestors(std::vector<fs::path>& roots, const fs::path& anchor)
{
    if (anchor.empty()) return;
    fs::path cur = anchor;
    for (int depth = 0; depth < 7; ++depth) {
        AddCandidateUnique(roots, cur);
        fs::path nested = cur / "Calculadora Unificada - Core Oficial";
        if (IsDirectory(nested)) {
            AddCandidateUnique(roots, nested);
        }
        if (!cur.has_parent_path()) break;
        fs::path parent = cur.parent_path();
        if (parent == cur) break;
        cur = parent;
    }
}

fs::path GetSelfExeDir()
{
    std::vector<char> buf(4096, '\0');
    ssize_t len = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if (len > 0) {
        buf[static_cast<size_t>(len)] = '\0';
        return fs::path(buf.data()).parent_path();
    }
    return {};
}

bool HasAllNxRoms(const fs::path& dir)
{
    return IsRegularFile(dir / "fx991lax.rom") &&
           IsRegularFile(dir / "fx570vnplus.rom");
}

bool HasAllCg50Roms(const fs::path& dir)
{
    return IsRegularFile(dir / "SystemROM.raw") &&
           IsRegularFile(dir / "UserROM.raw") &&
           IsRegularFile(dir / "C2ROM.raw") &&
           IsRegularFile(dir / "official-ILRAM-FD800000-4000.bin");
}

bool HasSkins(const fs::path& dir)
{
    return IsRegularFile(dir / "caratula_classwiz.png") ||
           IsRegularFile(dir / "caratula_esplus.png") ||
           IsRegularFile(dir / "caratula_cg50.png") ||
           IsRegularFile(dir / "bitmap_103.bmp");
}

bool HasCw2Runtime(const fs::path& dir)
{
    return IsRegularFile(dir / "emulator" / "casioemu.exe") &&
           IsRegularFile(dir / "models" / "fx991cw_emu" / "rom.bin");
}

bool HasManuals(const fs::path& dir)
{
    if (!IsDirectory(dir)) return false;
    if (IsRegularFile(dir / "indice_paginas_pdf.tsv")) return true;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".pdf") {
            return true;
        }
    }
    return false;
}

bool ReadBinaryFile(const std::string& path, std::vector<std::uint8_t>& out)
{
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) return false;
    const std::streamsize sz = ifs.tellg();
    if (sz <= 0) return false;
    ifs.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(sz));
    if (!ifs.read(reinterpret_cast<char*>(out.data()), sz)) {
        out.clear();
        return false;
    }
    return true;
}

std::uint32_t DecodeNxIndicators(int hardwareId, const std::uint8_t* raw, size_t len)
{
    std::uint32_t mask = 0;
    if (!raw) return 0;
    if (hardwareId == CALCEMU_HW_CLASSWIZ) {
        if (len < 32) return 0;
        if (raw[0x00] & 0x01u) mask |= (1u << 0);  // S
        if (raw[0x01] & 0x01u) mask |= (1u << 1);  // A
        if (raw[0x02] & 0x01u) mask |= (1u << 2);  // M
        if (raw[0x03] & 0x01u) mask |= (1u << 3);  // STO
        if (raw[0x05] & 0x01u) mask |= (1u << 4);  // Math
        if (raw[0x06] & 0x01u) mask |= (1u << 5);  // D
        if (raw[0x07] & 0x01u) mask |= (1u << 6);  // R
        if (raw[0x08] & 0x01u) mask |= (1u << 7);  // G
        if (raw[0x09] & 0x01u) mask |= (1u << 8);  // FIX
        if (raw[0x0A] & 0x01u) mask |= (1u << 9);  // SCI
        if (raw[0x11] & 0x01u) mask |= (1u << 10); // DOWN
        if (raw[0x12] & 0x01u) mask |= (1u << 11); // UP
    } else if (hardwareId == CALCEMU_HW_ES_PLUS) {
        if (len < 16) return 0;
        if (raw[0x00] & 0x10u) mask |= (1u << 0);  // S
        if (raw[0x00] & 0x04u) mask |= (1u << 1);  // A
        if (raw[0x01] & 0x10u) mask |= (1u << 2);  // M
        if (raw[0x01] & 0x02u) mask |= (1u << 3);  // STO
        if (raw[0x0A] & 0x40u) mask |= (1u << 4);  // Math
        if (raw[0x07] & 0x20u) mask |= (1u << 5);  // D
        if (raw[0x07] & 0x02u) mask |= (1u << 6);  // R
        if (raw[0x08] & 0x10u) mask |= (1u << 7);  // G
        if (raw[0x08] & 0x01u) mask |= (1u << 8);  // FIX
        if (raw[0x09] & 0x20u) mask |= (1u << 9);  // SCI
        if (raw[0x0A] & 0x08u) mask |= (1u << 10); // DOWN
        if (raw[0x0B] & 0x80u) mask |= (1u << 11); // UP
    }
    return mask;
}

std::string FindExecutableInPath(const char* binName)
{
    if (!binName || !*binName) return {};
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) return {};

    std::stringstream ss(pathEnv);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty()) continue;
        fs::path cand = fs::path(dir) / binName;
        if (IsExecutableFile(cand)) {
            return cand.string();
        }
    }
    return {};
}

} // namespace

AssetPaths DiscoverAssetPaths(const char* argv0)
{
    AssetPaths result;
    std::vector<fs::path> candidateRoots;

    // Optional explicit per-asset env overrides
    if (const char* envRoms = std::getenv("CASIO_ROMS_DIR")) {
        if (*envRoms && HasAllNxRoms(fs::path(envRoms))) {
            result.romsDir = NormalizePathString(fs::path(envRoms));
        }
    }
    if (const char* envCg = std::getenv("CASIO_CG50_ROMS_DIR")) {
        if (*envCg && HasAllCg50Roms(fs::path(envCg))) {
            result.cg50RomsDir = NormalizePathString(fs::path(envCg));
        }
    }
    if (const char* envSkins = std::getenv("CASIO_SKINS_DIR")) {
        if (*envSkins && HasSkins(fs::path(envSkins))) {
            result.skinsDir = NormalizePathString(fs::path(envSkins));
        }
    }

    // 1. CASIO_UNIFIED_HOME env var takes top priority
    if (const char* envHome = std::getenv("CASIO_UNIFIED_HOME")) {
        if (*envHome) {
            fs::path p(envHome);
            if (IsDirectory(p)) {
                AddAnchorAndAncestors(candidateRoots, p);
            }
        }
    }

    // 2. Executable directory via /proc/self/exe
    const fs::path selfDir = GetSelfExeDir();
    if (!selfDir.empty()) {
        AddAnchorAndAncestors(candidateRoots, selfDir);
    }

    // 3. argv[0] directory
    fs::path argvDir;
    if (argv0 && *argv0) {
        std::error_code ec;
        fs::path aPath(argv0);
        if (aPath.is_relative()) {
            fs::path cwd = fs::current_path(ec);
            if (!ec) aPath = cwd / aPath;
        }
        if (aPath.has_parent_path()) {
            argvDir = aPath.parent_path();
            AddAnchorAndAncestors(candidateRoots, argvDir);
        }
    }

    // 4. Current working directory
    {
        std::error_code ec;
        fs::path cwd = fs::current_path(ec);
        if (!ec) {
            AddAnchorAndAncestors(candidateRoots, cwd);
        }
    }

#ifdef CASIO_SOURCE_REPO_DIR
    // 5. Lowest-priority compile-time source root fallback for isolated test builds
    if (CASIO_SOURCE_REPO_DIR[0] != '\0') {
        const fs::path srcRepo(CASIO_SOURCE_REPO_DIR);
        if (IsDirectory(srcRepo)) {
            AddAnchorAndAncestors(candidateRoots, srcRepo);
        }
    }
#endif

    for (const auto& base : candidateRoots) {
        if (result.rootDir.empty() &&
            (IsDirectory(base / "roms") ||
             IsDirectory(base / "cg50_roms") ||
             IsDirectory(base / "manuales") ||
             IsRegularFile(base / "catalog" / "casio_knowledge_catalog.json"))) {
            result.rootDir = NormalizePathString(base);
        }

        if (result.romsDir.empty()) {
            const fs::path romChoices[] = {
                base / "roms",
                base / "assets" / "roms",
                base / "linux" / "dist-linux" / "roms"
            };
            for (const auto& cand : romChoices) {
                if (HasAllNxRoms(cand)) {
                    result.romsDir = NormalizePathString(cand);
                    break;
                }
            }
        }

        if (result.cg50RomsDir.empty()) {
            const fs::path cgChoices[] = {
                base / "cg50_roms",
                base / "roms",
                base / "assets" / "roms",
                base / "linux" / "dist-linux" / "cg50_roms"
            };
            for (const auto& cand : cgChoices) {
                if (HasAllCg50Roms(cand)) {
                    result.cg50RomsDir = NormalizePathString(cand);
                    break;
                }
            }
        }

        if (result.skinsDir.empty()) {
            const fs::path skinChoices[] = {
                base / "skins",
                base / "assets" / "skins",
                base / "linux" / "dist-linux" / "skins",
                base / "extracted_white"
            };
            for (const auto& cand : skinChoices) {
                if (HasSkins(cand)) {
                    result.skinsDir = NormalizePathString(cand);
                    break;
                }
            }
        }

        if (result.fx991cwDir.empty()) {
            const fs::path cwChoices[] = {
                base / "runtime" / "fx991cw",
                base / "fx991cw"
            };
            for (const auto& cand : cwChoices) {
                if (HasCw2Runtime(cand)) {
                    result.fx991cwDir = NormalizePathString(cand);
                    break;
                }
            }
        }

        if (result.manualsDir.empty()) {
            const fs::path manDir = base / "manuales";
            if (HasManuals(manDir)) {
                result.manualsDir = NormalizePathString(manDir);
            }
        }

        if (result.catalogJsonPath.empty()) {
            const fs::path catPath = base / "catalog" / "casio_knowledge_catalog.json";
            if (IsRegularFile(catPath)) {
                result.catalogJsonPath = NormalizePathString(catPath);
            }
        }

        if (result.documentStudioBin.empty()) {
            std::vector<fs::path> binChoices;
            if (!selfDir.empty()) {
                binChoices.push_back(selfDir / "document-studio");
                binChoices.push_back(selfDir / ".." / "doc_studio_app" / "document-studio");
                binChoices.push_back(selfDir / ".." / "bin" / "document-studio");
            }
            if (!argvDir.empty()) {
                binChoices.push_back(argvDir / "document-studio");
                binChoices.push_back(argvDir / ".." / "doc_studio_app" / "document-studio");
            }
            binChoices.push_back(base / "document-studio");
            binChoices.push_back(base / "bin" / "document-studio");
            binChoices.push_back(base / "linux" / "build" / "doc_studio_app" / "document-studio");
            binChoices.push_back(base / "build" / "doc_studio_app" / "document-studio");

            for (const auto& cand : binChoices) {
                if (IsExecutableFile(cand)) {
                    result.documentStudioBin = NormalizePathString(cand);
                    break;
                }
            }
        }
    }

    return result;
}

int GetSkinWidth(CalcModelId model)
{
    switch (model) {
    case CalcModelId::BlancaClassWiz: return 644;
    case CalcModelId::GrisEsPlus:     return 405;
    case CalcModelId::Cg50Prizm:      return 664;
    case CalcModelId::NegraCw2:       return 992;
    }
    return 644;
}

int GetSkinHeight(CalcModelId model)
{
    switch (model) {
    case CalcModelId::BlancaClassWiz: return 1250;
    case CalcModelId::GrisEsPlus:     return 816;
    case CalcModelId::Cg50Prizm:      return 1364;
    case CalcModelId::NegraCw2:       return 2082;
    }
    return 1250;
}

SkinRect GetLcdSkinRect(CalcModelId model)
{
    switch (model) {
    case CalcModelId::BlancaClassWiz: return SkinRect{ 130, 235, 514, 385 };
    case CalcModelId::GrisEsPlus:     return SkinRect{  61, 132, 349, 243 };
    case CalcModelId::Cg50Prizm:      return SkinRect{  92, 153, 572, 403 };
    case CalcModelId::NegraCw2:       return SkinRect{ 107, 315, 885, 620 };
    }
    return SkinRect{};
}

const std::vector<KeyDef>& GetKeysForModel(CalcModelId model)
{
    switch (model) {
    case CalcModelId::BlancaClassWiz: return detail::BuildClasswizKeys();
    case CalcModelId::GrisEsPlus:     return detail::BuildGrayKeys();
    case CalcModelId::Cg50Prizm:      return detail::BuildCg50Keys();
    case CalcModelId::NegraCw2:       return detail::BuildCw2Keys();
    }
    return detail::BuildClasswizKeys();
}

EvalOutput EvaluateMathUtf8(const std::string& expr, bool degMode, double lastAns)
{
    const detail::AngleMode mode = degMode ? detail::AngleMode::Deg : detail::AngleMode::Rad;
    return detail::EvaluateWithAngleMode(expr, mode, lastAns);
}

struct UnifiedCalcEngine::Impl {
    CalcModelId model = CalcModelId::BlancaClassWiz;
    bool ready = false;
    AssetPaths paths;

    // nX-U8 state (Blanca / Gris)
    CalcEmu* nxEmu = nullptr;
    int nxHardwareId = 0;
    std::vector<std::uint8_t> nxRawBuf;
    std::vector<std::uint8_t> nxPixelBuf;
    std::uint32_t nxLastIndicators = 0xFFFFFFFFu;
    int nxLastMode = -1;
    bool nxHasRenderedOnce = false;

    // fx-CG50 state
    CasioCg50Handle* cgEmu = nullptr;
    std::vector<std::uint8_t> cgLastFrame;
    bool cgHasFrameOnce = false;

    // fx-991CW state (Wine64 host + built-in fallback)
    mutable pid_t cw2Pid = -1;
    std::string cw2LastDiag;
    detail::Cw2FallbackEngine cw2Fallback;

    bool CheckCw2Alive() const
    {
        if (cw2Pid <= 0) return false;
        int status = 0;
        const pid_t res = ::waitpid(cw2Pid, &status, WNOHANG);
        if (res == 0) {
            return true;
        }
        cw2Pid = -1;
        return false;
    }

    void StopCw2Process()
    {
        if (cw2Pid <= 0) return;
        ::kill(cw2Pid, SIGTERM);
        int status = 0;
        for (int i = 0; i < 25; ++i) {
            const pid_t r = ::waitpid(cw2Pid, &status, WNOHANG);
            if (r == cw2Pid || (r == -1 && errno == ECHILD)) {
                cw2Pid = -1;
                return;
            }
            ::usleep(10000);
        }
        ::kill(cw2Pid, SIGKILL);
        ::waitpid(cw2Pid, &status, 0);
        cw2Pid = -1;
    }
};

UnifiedCalcEngine::UnifiedCalcEngine()
    : impl_(std::make_unique<Impl>())
{
}

UnifiedCalcEngine::~UnifiedCalcEngine()
{
    Shutdown();
}

bool UnifiedCalcEngine::Initialize(CalcModelId model, const AssetPaths& paths, std::string* outErr)
{
    Shutdown();
    impl_->model = model;
    impl_->paths = paths;

    if (model == CalcModelId::BlancaClassWiz || model == CalcModelId::GrisEsPlus) {
        const bool isBlanca = (model == CalcModelId::BlancaClassWiz);
        const char* romName = isBlanca ? "fx991lax.rom" : "fx570vnplus.rom";
        if (paths.romsDir.empty()) {
            if (outErr) *outErr = std::string("Directorio de ROMs no encontrado para ") + romName;
            return false;
        }
        const fs::path romPath = fs::path(paths.romsDir) / romName;
        std::vector<std::uint8_t> romBytes;
        if (!ReadBinaryFile(romPath.string(), romBytes)) {
            if (outErr) *outErr = "No se pudo leer la ROM: " + romPath.string();
            return false;
        }
        const std::size_t minRomBytes = isBlanca ? 0x40000u : 0x20000u;
        if (romBytes.size() < minRomBytes) {
            if (outErr) {
                *outErr = "ROM truncada o invalida (" + romPath.string() + "): " +
                          std::to_string(romBytes.size()) + " bytes < " +
                          std::to_string(minRomBytes) + " bytes requeridos";
            }
            return false;
        }

        CalcEmuModel emuModel{};
        if (isBlanca) {
            emuModel.hardware_id = CALCEMU_HW_CLASSWIZ;
            emuModel.real_hardware = 0;
            emuModel.csr_mask = 0x0003;
            emuModel.pd_value = 0x00;
        } else {
            emuModel.hardware_id = CALCEMU_HW_ES_PLUS;
            emuModel.real_hardware = 0;
            emuModel.csr_mask = 0x0001;
            emuModel.pd_value = 0x08;
        }

        CalcEmu* emu = calcemu_create(&emuModel, romBytes.data(), romBytes.size());
        if (!emu) {
            if (outErr) *outErr = "calcemu_create fallo para " + romPath.string();
            return false;
        }

        calcemu_power_on(emu);
        calcemu_advance(emu, 2000);

        impl_->nxEmu = emu;
        impl_->nxHardwareId = emuModel.hardware_id;
        impl_->nxRawBuf.resize(calcemu_screen_buffer_size(emu));
        impl_->nxPixelBuf.resize(
            static_cast<size_t>(calcemu_screen_width(emu)) *
            static_cast<size_t>(calcemu_screen_rows(emu)));
        impl_->nxLastIndicators = 0xFFFFFFFFu;
        impl_->nxLastMode = -1;
        impl_->nxHasRenderedOnce = false;
        impl_->ready = true;
        return true;
    }

    if (model == CalcModelId::Cg50Prizm) {
        if (paths.cg50RomsDir.empty()) {
            if (outErr) *outErr = "Directorio cg50RomsDir vacio";
            return false;
        }
        const fs::path dir(paths.cg50RomsDir);
        static constexpr const char* kRequiredFiles[4] = {
            "SystemROM.raw",
            "UserROM.raw",
            "C2ROM.raw",
            "official-ILRAM-FD800000-4000.bin"
        };
        for (const char* fname : kRequiredFiles) {
            if (!IsRegularFile(dir / fname)) {
                if (outErr) {
                    *outErr = "Falta archivo requerido de CG50: " + (dir / fname).string();
                }
                return false;
            }
        }

        CasioCg50Handle* cg = casio_cg50_create_from_dir(paths.cg50RomsDir.c_str());
        if (!cg) {
            if (outErr) *outErr = "casio_cg50_create_from_dir fallo en " + paths.cg50RomsDir;
            return false;
        }
        impl_->cgEmu = cg;
        impl_->cgLastFrame.clear();
        impl_->cgHasFrameOnce = false;
        impl_->ready = true;
        return true;
    }

    // CalcModelId::NegraCw2
    impl_->cw2Fallback.Reset();
    impl_->cw2LastDiag.clear();
    impl_->ready = true;
    return true;
}

void UnifiedCalcEngine::Shutdown()
{
    if (!impl_) return;
    impl_->StopCw2Process();
    if (impl_->nxEmu) {
        calcemu_destroy(impl_->nxEmu);
        impl_->nxEmu = nullptr;
    }
    if (impl_->cgEmu) {
        casio_cg50_destroy(impl_->cgEmu);
        impl_->cgEmu = nullptr;
    }
    impl_->nxRawBuf.clear();
    impl_->nxPixelBuf.clear();
    impl_->cgLastFrame.clear();
    impl_->cgHasFrameOnce = false;
    impl_->nxHasRenderedOnce = false;
    impl_->ready = false;
}

bool UnifiedCalcEngine::IsReady() const noexcept
{
    return impl_ && impl_->ready;
}

CalcModelId UnifiedCalcEngine::Model() const noexcept
{
    return impl_ ? impl_->model : CalcModelId::BlancaClassWiz;
}

std::string UnifiedCalcEngine::StatusText() const
{
    if (!impl_ || !impl_->ready) {
        return "Inactivo";
    }
    switch (impl_->model) {
    case CalcModelId::BlancaClassWiz:
        return "nX-U8 Nativo (fx-991LA X)";
    case CalcModelId::GrisEsPlus:
        return "nX-U8 Nativo (fx-570VN PLUS)";
    case CalcModelId::Cg50Prizm:
        return "SH7305 Nativo (fx-CG50 PRIZM)";
    case CalcModelId::NegraCw2:
        if (impl_->CheckCw2Alive()) {
            return "Wine64 CasioEmuX (fx-991CW)";
        }
        return "Modo Nativo Integrado (fx-991CW)";
    }
    return "Desconocido";
}

bool UnifiedCalcEngine::UsingExternalProcess() const noexcept
{
    return impl_ && impl_->ready && impl_->model == CalcModelId::NegraCw2 && impl_->CheckCw2Alive();
}

void UnifiedCalcEngine::StepMs(std::uint32_t ms)
{
    if (!impl_ || !impl_->ready || ms == 0) return;

    if (impl_->model == CalcModelId::BlancaClassWiz || impl_->model == CalcModelId::GrisEsPlus) {
        if (impl_->nxEmu) {
            calcemu_advance(impl_->nxEmu, ms);
        }
        return;
    }

    if (impl_->model == CalcModelId::Cg50Prizm) {
        if (impl_->cgEmu) {
            // Map 1 ms -> ~120,000 SH7305 instructions + 400 sleep timer cycles,
            // sliced into <= 16 ms frames so timer/interrupt wakeups process smoothly.
            std::uint32_t remaining = ms;
            while (remaining > 0) {
                const std::uint32_t slice = std::min<std::uint32_t>(remaining, 16u);
                const std::uint64_t runBudget = static_cast<std::uint64_t>(slice) * 120000ULL;
                const std::uint64_t sleepCycles = static_cast<std::uint64_t>(slice) * 400ULL;
                casio_cg50_step(impl_->cgEmu, runBudget, sleepCycles);
                remaining -= slice;
            }
        }
        return;
    }

    if (impl_->model == CalcModelId::NegraCw2) {
        impl_->cw2Fallback.StepMs(ms);
    }
}

void UnifiedCalcEngine::KeyDown(std::uint16_t matrixCode)
{
    if (!impl_ || !impl_->ready) return;

    if (impl_->model == CalcModelId::BlancaClassWiz || impl_->model == CalcModelId::GrisEsPlus) {
        if (!impl_->nxEmu) return;
        if ((matrixCode & 0xFFu) == 0xFFu) {
            calcemu_power_on(impl_->nxEmu);
            calcemu_advance(impl_->nxEmu, 120);
        } else {
            calcemu_key_down(impl_->nxEmu, static_cast<unsigned char>(matrixCode & 0xFFu));
        }
        return;
    }

    if (impl_->model == CalcModelId::Cg50Prizm) {
        if (impl_->cgEmu) {
            casio_cg50_key_down(impl_->cgEmu, matrixCode);
        }
        return;
    }

    if (impl_->model == CalcModelId::NegraCw2 && !impl_->CheckCw2Alive()) {
        impl_->cw2Fallback.HandleMatrixCode(matrixCode);
    }
}

void UnifiedCalcEngine::KeyUp(std::uint16_t matrixCode)
{
    if (!impl_ || !impl_->ready) return;

    if (impl_->model == CalcModelId::BlancaClassWiz || impl_->model == CalcModelId::GrisEsPlus) {
        if (impl_->nxEmu) {
            calcemu_key_up(impl_->nxEmu);
        }
        return;
    }

    if (impl_->model == CalcModelId::Cg50Prizm) {
        if (impl_->cgEmu) {
            if (matrixCode == 0) {
                casio_cg50_release_all_keys(impl_->cgEmu);
            } else {
                casio_cg50_key_up(impl_->cgEmu, matrixCode);
            }
        }
        return;
    }
}

void UnifiedCalcEngine::TapKey(std::uint16_t matrixCode, std::uint32_t holdMs, std::uint32_t settleMs)
{
    if (!impl_ || !impl_->ready) return;

    if (impl_->model == CalcModelId::BlancaClassWiz || impl_->model == CalcModelId::GrisEsPlus) {
        if (!impl_->nxEmu) return;
        const std::uint32_t effSettle = std::max<std::uint32_t>(settleMs, 300u);
        if ((matrixCode & 0xFFu) == 0xFFu) {
            calcemu_power_on(impl_->nxEmu);
            calcemu_advance(impl_->nxEmu, effSettle);
        } else {
            // Core Oficial nX-U8 ROMs (1024 kHz ClassWiz / 128 kHz ES PLUS) reliably sample keys at 400/300 ms.
            const std::uint32_t effHold = std::max<std::uint32_t>(holdMs, 400u);
            calcemu_tap_key(impl_->nxEmu, static_cast<unsigned char>(matrixCode & 0xFFu), effHold, effSettle);
        }
        return;
    }

    if (impl_->model == CalcModelId::Cg50Prizm) {
        if (!impl_->cgEmu) return;
        casio_cg50_key_down(impl_->cgEmu, matrixCode);
        StepMs(holdMs > 0 ? holdMs : 1u);
        casio_cg50_key_up(impl_->cgEmu, matrixCode);
        StepMs(settleMs > 0 ? settleMs : 1u);
        return;
    }

    if (impl_->model == CalcModelId::NegraCw2 && !impl_->CheckCw2Alive()) {
        impl_->cw2Fallback.HandleMatrixCode(matrixCode);
        StepMs(holdMs + settleMs);
    }
}

void UnifiedCalcEngine::TapKeyById(int keyId, std::uint32_t holdMs, std::uint32_t settleMs)
{
    if (!impl_ || !impl_->ready) return;
    if (impl_->model == CalcModelId::NegraCw2) {
        if (!impl_->CheckCw2Alive()) {
            impl_->cw2Fallback.HandleKeyId(keyId);
            StepMs(holdMs + settleMs);
        }
        return;
    }
    const auto& keys = GetKeysForModel(impl_->model);
    if (keyId < 0 || keyId >= static_cast<int>(keys.size())) return;
    TapKey(keys[static_cast<size_t>(keyId)].matrixCode, holdMs, settleMs);
}

void UnifiedCalcEngine::Reset()
{
    if (!impl_ || !impl_->ready) return;

    if (impl_->model == CalcModelId::BlancaClassWiz || impl_->model == CalcModelId::GrisEsPlus) {
        if (impl_->nxEmu) {
            calcemu_power_on(impl_->nxEmu);
            calcemu_advance(impl_->nxEmu, 2000);
            impl_->nxHasRenderedOnce = false;
            impl_->nxLastIndicators = 0xFFFFFFFFu;
        }
        return;
    }

    if (impl_->model == CalcModelId::Cg50Prizm) {
        if (impl_->cgEmu) {
            casio_cg50_destroy(impl_->cgEmu);
            impl_->cgEmu = casio_cg50_create_from_dir(impl_->paths.cg50RomsDir.c_str());
            impl_->ready = (impl_->cgEmu != nullptr);
            impl_->cgLastFrame.clear();
            impl_->cgHasFrameOnce = false;
        }
        return;
    }

    if (impl_->model == CalcModelId::NegraCw2) {
        impl_->cw2Fallback.Reset();
    }
}

int UnifiedCalcEngine::ScreenWidth() const noexcept
{
    if (!impl_ || !impl_->ready) return 0;
    switch (impl_->model) {
    case CalcModelId::BlancaClassWiz:
    case CalcModelId::GrisEsPlus:
        return impl_->nxEmu ? calcemu_screen_width(impl_->nxEmu) : 0;
    case CalcModelId::Cg50Prizm:
        return 384;
    case CalcModelId::NegraCw2:
        return detail::kCw2LcdW;
    }
    return 0;
}

int UnifiedCalcEngine::ScreenHeight() const noexcept
{
    if (!impl_ || !impl_->ready) return 0;
    switch (impl_->model) {
    case CalcModelId::BlancaClassWiz:
    case CalcModelId::GrisEsPlus:
        return impl_->nxEmu ? calcemu_screen_rows(impl_->nxEmu) : 0;
    case CalcModelId::Cg50Prizm:
        return 216;
    case CalcModelId::NegraCw2:
        return detail::kCw2LcdH;
    }
    return 0;
}

bool UnifiedCalcEngine::RenderRgba(
    std::vector<std::uint8_t>& outRgba,
    std::uint32_t* outIndicators,
    bool forceCopy)
{
    if (!impl_ || !impl_->ready) return false;

    if (impl_->model == CalcModelId::BlancaClassWiz || impl_->model == CalcModelId::GrisEsPlus) {
        CalcEmu* emu = impl_->nxEmu;
        if (!emu) return false;
        const int w = calcemu_screen_width(emu);
        const int h = calcemu_screen_rows(emu);
        if (w <= 0 || h <= 0) return false;

        const size_t bufSz = calcemu_screen_buffer_size(emu);
        if (impl_->nxRawBuf.size() < bufSz) {
            impl_->nxRawBuf.resize(bufSz);
        }
        const size_t rawGot = calcemu_screen_buffer(emu, impl_->nxRawBuf.data(), impl_->nxRawBuf.size());
        const std::uint32_t indMask = DecodeNxIndicators(impl_->nxHardwareId, impl_->nxRawBuf.data(), rawGot);
        if (outIndicators) *outIndicators = indMask;

        const int redraw = calcemu_take_redraw_flag(emu);
        const int mode = calcemu_screen_mode(emu);
        const bool changed = !impl_->nxHasRenderedOnce ||
                             forceCopy ||
                             redraw != 0 ||
                             indMask != impl_->nxLastIndicators ||
                             mode != impl_->nxLastMode;
        if (!changed) return false;

        impl_->nxHasRenderedOnce = true;
        impl_->nxLastIndicators = indMask;
        impl_->nxLastMode = mode;

        const size_t pxCount = static_cast<size_t>(w) * static_cast<size_t>(h);
        if (impl_->nxPixelBuf.size() < pxCount) {
            impl_->nxPixelBuf.resize(pxCount);
        }
        calcemu_screen_pixels(emu, impl_->nxPixelBuf.data(), impl_->nxPixelBuf.size());

        const std::uint8_t bgR = (impl_->nxHardwareId == CALCEMU_HW_CLASSWIZ) ? 208 : 205;
        const std::uint8_t bgG = (impl_->nxHardwareId == CALCEMU_HW_CLASSWIZ) ? 222 : 217;
        const std::uint8_t bgB = (impl_->nxHardwareId == CALCEMU_HW_CLASSWIZ) ? 200 : 205;
        const std::uint8_t fgR = (impl_->nxHardwareId == CALCEMU_HW_CLASSWIZ) ? 28 : 25;
        const std::uint8_t fgG = (impl_->nxHardwareId == CALCEMU_HW_CLASSWIZ) ? 38 : 35;
        const std::uint8_t fgB = (impl_->nxHardwareId == CALCEMU_HW_CLASSWIZ) ? 56 : 50;

        outRgba.resize(pxCount * 4u);
        for (size_t i = 0; i < pxCount; ++i) {
            const bool on = impl_->nxPixelBuf[i] != 0;
            std::uint8_t* p = outRgba.data() + i * 4u;
            p[0] = on ? fgR : bgR;
            p[1] = on ? fgG : bgG;
            p[2] = on ? fgB : bgB;
            p[3] = 255u;
        }
        return true;
    }

    if (impl_->model == CalcModelId::Cg50Prizm) {
        if (outIndicators) *outIndicators = 0;
        if (!impl_->cgEmu) return false;
        constexpr size_t kBytes = 384u * 216u * 4u;
        outRgba.resize(kBytes);
        const int rc = casio_cg50_render_rgba(impl_->cgEmu, outRgba.data(), static_cast<int>(kBytes));
        if (rc <= 0) return false;
        if (!forceCopy && impl_->cgHasFrameOnce && impl_->cgLastFrame == outRgba) {
            return false;
        }
        impl_->cgHasFrameOnce = true;
        impl_->cgLastFrame = outRgba;
        return true;
    }

    // NegraCw2 built-in 192x64 LCD fallback
    return impl_->cw2Fallback.RenderRgba(outRgba, outIndicators, forceCopy);
}

void UnifiedCalcEngine::RenderIndicatorStripRgba(
    CalcModelId model,
    std::uint32_t indicatorsMask,
    std::uint8_t fgR, std::uint8_t fgG, std::uint8_t fgB,
    std::uint8_t bgR, std::uint8_t bgG, std::uint8_t bgB,
    std::vector<std::uint8_t>& outRgba,
    int& outW,
    int& outH)
{
    int calcIdx = -1;
    if (model == CalcModelId::BlancaClassWiz || model == CalcModelId::NegraCw2) calcIdx = 0;
    else if (model == CalcModelId::GrisEsPlus) calcIdx = 1;
    Indicators::RenderStripFromMask(
        calcIdx, indicatorsMask,
        fgR, fgG, fgB,
        bgR, bgG, bgB,
        outRgba, outW, outH);
}

std::vector<std::uint8_t> UnifiedCalcEngine::ExportState() const
{
    if (!impl_ || !impl_->ready) return {};

    if (impl_->model == CalcModelId::BlancaClassWiz || impl_->model == CalcModelId::GrisEsPlus) {
        if (!impl_->nxEmu) return {};
        const size_t needed = calcemu_save_state(impl_->nxEmu, nullptr, 0);
        if (needed == 0) return {};
        std::vector<std::uint8_t> blob(needed);
        const size_t written = calcemu_save_state(impl_->nxEmu, blob.data(), blob.size());
        if (written == 0 || written > blob.size()) return {};
        blob.resize(written);
        return blob;
    }

    if (impl_->model == CalcModelId::Cg50Prizm) {
        if (!impl_->cgEmu) return {};
        const size_t needed = casio_cg50_export_state(impl_->cgEmu, nullptr, 0);
        if (needed == 0) return {};
        std::vector<std::uint8_t> blob(needed);
        const size_t written = casio_cg50_export_state(impl_->cgEmu, blob.data(), blob.size());
        if (written == 0 || written > blob.size()) return {};
        blob.resize(written);
        return blob;
    }

    if (impl_->model == CalcModelId::NegraCw2) {
        return impl_->cw2Fallback.ExportState();
    }

    return {};
}

bool UnifiedCalcEngine::ImportState(const std::vector<std::uint8_t>& state)
{
    if (!impl_ || !impl_->ready || state.empty()) return false;

    if (impl_->model == CalcModelId::BlancaClassWiz || impl_->model == CalcModelId::GrisEsPlus) {
        if (!impl_->nxEmu) return false;
        const int ok = calcemu_load_state(impl_->nxEmu, state.data(), state.size());
        if (ok == 1) {
            impl_->nxHasRenderedOnce = false;
            impl_->nxLastIndicators = 0xFFFFFFFFu;
            return true;
        }
        return false;
    }

    if (impl_->model == CalcModelId::Cg50Prizm) {
        if (!impl_->cgEmu) return false;
        const int ok = casio_cg50_import_state(impl_->cgEmu, state.data(), state.size());
        if (ok == 1) {
            impl_->cgHasFrameOnce = false;
            impl_->cgLastFrame.clear();
            return true;
        }
        return false;
    }

    if (impl_->model == CalcModelId::NegraCw2) {
        return impl_->cw2Fallback.ImportState(state);
    }

    return false;
}

bool UnifiedCalcEngine::LaunchExternalCw2Window(std::string* outDiag)
{
    if (!impl_ || !impl_->ready || impl_->model != CalcModelId::NegraCw2) {
        if (outDiag) *outDiag = "LaunchExternalCw2Window solo aplica a NegraCw2";
        return false;
    }
    if (impl_->CheckCw2Alive()) {
        if (outDiag) *outDiag = "Wine64 CasioEmuX ya en ejecucion";
        return true;
    }

    if (impl_->paths.fx991cwDir.empty() || !HasCw2Runtime(impl_->paths.fx991cwDir)) {
        impl_->cw2LastDiag = "No se encontro runtime/fx991cw/emulator/casioemu.exe";
        if (outDiag) *outDiag = impl_->cw2LastDiag;
        return false;
    }

    std::string wineBin = FindExecutableInPath("wine64");
    if (wineBin.empty()) {
        wineBin = FindExecutableInPath("wine");
    }
    if (wineBin.empty()) {
        impl_->cw2LastDiag = "wine64/wine no disponible en PATH; usando Modo Nativo Integrado";
        if (outDiag) *outDiag = impl_->cw2LastDiag;
        return false;
    }

    const fs::path cwDir(impl_->paths.fx991cwDir);
    const fs::path exePath = cwDir / "emulator" / "casioemu.exe";
    if (!IsRegularFile(exePath)) {
        impl_->cw2LastDiag = "Falta ejecutable casioemu.exe";
        if (outDiag) *outDiag = impl_->cw2LastDiag;
        return false;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        impl_->cw2LastDiag = "fork() fallo al lanzar Wine64 CasioEmuX";
        if (outDiag) *outDiag = impl_->cw2LastDiag;
        return false;
    }

    if (pid == 0) {
        if (::chdir(cwDir.c_str()) != 0) {
            ::_exit(126);
        }
        std::string arg0 = wineBin;
        std::string arg1 = "emulator/casioemu.exe";
        std::string arg2 = "model=models/fx991cw_emu";
        std::string arg3 = "width=380";
        std::string arg4 = "height=798";
        std::string arg5 = "resizable=1";
        char* const argv[] = {
            arg0.data(),
            arg1.data(),
            arg2.data(),
            arg3.data(),
            arg4.data(),
            arg5.data(),
            nullptr
        };
        ::execvp(wineBin.c_str(), argv);
        ::_exit(127);
    }

    impl_->cw2Pid = pid;
    ::usleep(40000);
    if (!impl_->CheckCw2Alive()) {
        impl_->cw2LastDiag = "Wine64 CasioEmuX termino inmediatamente (sin servidor grafico X11/Wayland o prefijo Wine)";
        if (outDiag) *outDiag = impl_->cw2LastDiag;
        return false;
    }

    impl_->cw2LastDiag = "Wine64 CasioEmuX iniciado correctamente";
    if (outDiag) *outDiag = impl_->cw2LastDiag;
    return true;
}

bool UnifiedCalcEngine::IsExternalCw2Running() const
{
    if (!impl_ || impl_->model != CalcModelId::NegraCw2) return false;
    return impl_->CheckCw2Alive();
}

void UnifiedCalcEngine::StopExternalCw2()
{
    if (impl_) {
        impl_->StopCw2Process();
    }
}

bool UnifiedCalcEngine::SendKeyToCw2OrFallback(int keyId, std::string* outNote)
{
    if (!impl_ || !impl_->ready || impl_->model != CalcModelId::NegraCw2) {
        return false;
    }
    const auto& keys = GetKeysForModel(CalcModelId::NegraCw2);
    if (keyId < 0 || keyId >= static_cast<int>(keys.size())) {
        return false;
    }

    if (impl_->CheckCw2Alive()) {
        if (outNote) {
            *outNote = "Wine64 CasioEmuX activo (" + keys[static_cast<size_t>(keyId)].name + ")";
        }
        return true;
    }

    const bool handled = impl_->cw2Fallback.HandleKeyId(keyId);
    if (outNote) {
        *outNote = handled
            ? ("CW2 Nativo: " + keys[static_cast<size_t>(keyId)].name + " -> " + impl_->cw2Fallback.Result())
            : ("Tecla no asignada: " + keys[static_cast<size_t>(keyId)].name);
    }
    return handled;
}

std::string UnifiedCalcEngine::GetFallbackExpression() const
{
    if (!impl_ || impl_->model != CalcModelId::NegraCw2) return {};
    return impl_->cw2Fallback.Expression();
}

std::string UnifiedCalcEngine::GetFallbackResult() const
{
    if (!impl_ || impl_->model != CalcModelId::NegraCw2) return {};
    return impl_->cw2Fallback.Result();
}

std::string UnifiedCalcEngine::GetFallbackModeName() const
{
    if (!impl_ || impl_->model != CalcModelId::NegraCw2) return {};
    return impl_->cw2Fallback.ModeName();
}

std::uint32_t UnifiedCalcEngine::GetFallbackIndicatorsMask() const
{
    if (!impl_ || impl_->model != CalcModelId::NegraCw2) return 0u;
    return impl_->cw2Fallback.ComputeIndicatorsMask();
}

std::string UnifiedCalcEngine::EvaluateFallbackComplex(const std::string& expr)
{
    if (!impl_ || !impl_->ready || impl_->model != CalcModelId::NegraCw2) return {};
    return impl_->cw2Fallback.EvalComplexExpr(expr);
}

void UnifiedCalcEngine::SetFallbackMatrixA(int dim, const std::vector<double>& rowMajor)
{
    if (!impl_ || !impl_->ready || impl_->model != CalcModelId::NegraCw2) return;
    impl_->cw2Fallback.SetMatrixA(dim, rowMajor);
}

void UnifiedCalcEngine::SetFallbackMatrixB(int dim, const std::vector<double>& rowMajor)
{
    if (!impl_ || !impl_->ready || impl_->model != CalcModelId::NegraCw2) return;
    impl_->cw2Fallback.SetMatrixB(dim, rowMajor);
}

std::string UnifiedCalcEngine::EvaluateFallbackMatrixOp(const std::string& opName)
{
    if (!impl_ || !impl_->ready || impl_->model != CalcModelId::NegraCw2) return {};
    return impl_->cw2Fallback.EvaluateMatrixOp(opName);
}

std::vector<std::pair<double, std::string>> UnifiedCalcEngine::ComputeFallbackTable(
    const std::string& fxExpr, double start, double end, double step)
{
    if (!impl_ || !impl_->ready || impl_->model != CalcModelId::NegraCw2) return {};
    return impl_->cw2Fallback.ComputeTable(fxExpr, start, end, step);
}

std::string UnifiedCalcEngine::SolveFallbackSimultaneous(int nVars, const std::vector<double>& coeffs)
{
    if (!impl_ || !impl_->ready || impl_->model != CalcModelId::NegraCw2) return {};
    return impl_->cw2Fallback.SolveSimultaneous(nVars, coeffs);
}

std::string UnifiedCalcEngine::SolveFallbackQuadratic(double a, double b, double c)
{
    if (!impl_ || !impl_->ready || impl_->model != CalcModelId::NegraCw2) return {};
    return impl_->cw2Fallback.SolveQuadratic(a, b, c);
}

} // namespace CasioLinux
