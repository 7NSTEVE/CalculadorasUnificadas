#include "studio_profile_store.hpp"
#include "utf_conv.hpp"
#include "utf8_path.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace DocStudio {

namespace {

bool WriteUtf8File(const fs::path& p, const std::string& content, std::string* outErr) {
    try {
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            if (outErr) *outErr = "No se pudo abrir archivo para escritura: " + PathToUtf8(p);
            return false;
        }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        ofs.flush();
        return ofs.good();
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

bool ReadUtf8File(const fs::path& p, std::string& outContent, std::string* outErr) {
    try {
        std::ifstream ifs(p, std::ios::binary | std::ios::ate);
        if (!ifs.is_open()) {
            if (outErr) *outErr = "Archivo no encontrado: " + PathToUtf8(p);
            return false;
        }
        const auto sz = ifs.tellg();
        if (sz < 0) return false;
        outContent.resize(static_cast<std::size_t>(sz));
        ifs.seekg(0, std::ios::beg);
        if (!outContent.empty()) ifs.read(outContent.data(), static_cast<std::streamsize>(outContent.size()));
        return ifs.good();
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

} // namespace

namespace {
std::wstring profileRootOverride;
}

bool StudioProfileStore::SetProfileRootOverride(const std::wstring& path) {
    const fs::path requested(path);
    if (!requested.is_absolute()) return false;
    const fs::path root = requested.lexically_normal();
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec || !fs::is_directory(root, ec) || ec) return false;
    profileRootOverride = root.wstring();
    return true;
}

bool StudioProfileStore::HasProfileRootOverride() noexcept {
    return !profileRootOverride.empty();
}

void StudioProfileStore::ClearProfileRootOverride() noexcept {
    profileRootOverride.clear();
}

std::wstring StudioProfileStore::DefaultProfileRootDir() {
    if (!profileRootOverride.empty()) return profileRootOverride;
#ifdef _WIN32
    PWSTR appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appData)) && appData) {
        fs::path root(appData);
        CoTaskMemFree(appData);
        root /= L"CuadernU";
        std::error_code ec;
        fs::create_directories(root, ec);
        return root.wstring();
    }
#endif
    fs::path fallback = fs::current_path() / "CuadernU_Profile";
    std::error_code ec;
    fs::create_directories(fallback, ec);
    return fallback.wstring();
}

void StudioProfileStore::PackExtensionsIntoDocument(DocumentModel& doc, const DocumentExtensionBundle& bundle) {
    // 1. YAML properties, tags, aliases
    json propJson;
    propJson["yamlRaw"] = bundle.yamlFrontmatterUtf8;
    propJson["aliases"] = bundle.aliasesUtf8;
    propJson["tags"] = bundle.tagsUtf8;
    propJson["activeDbViewKind"] = bundle.activeDbViewKind;
    json pArr = json::array();
    for (const auto& p : bundle.properties) {
        pArr.push_back({
            {"key", p.keyUtf8},
            {"type", static_cast<int>(p.type)},
            {"value", p.valueUtf8},
            {"list", p.listValuesUtf8}
        });
    }
    propJson["properties"] = std::move(pArr);
    doc.SetExtensionPayload("properties.json", propJson.dump(2));

    // 2. Comments
    if (bundle.comments) {
        doc.SetExtensionPayload("comments.json", bundle.comments->SerializeToJson());
    }
    // 3. Structured Database (.base JSON)
    if (bundle.database) {
        doc.SetExtensionPayload("database.json", bundle.database->SerializeToBaseJson());
    }
    // 4. Permissions & salted PBKDF2 lock hash (never plaintext password)
    if (bundle.permissions) {
        doc.SetExtensionPayload("permissions.json", bundle.permissions->SerializeToJson());
    }
    // 5. Automations
    if (bundle.automations) {
        doc.SetExtensionPayload("automations.json", bundle.automations->SerializeToJson());
    }
    // 6. Flexcil study overlay state
    if (bundle.flexcil) {
        doc.SetExtensionPayload("flexcil.json", bundle.flexcil->SerializeStateToJson());
    }
    // 7. StudyDeck SM-2 spaced repetition state (preserves repetitions, easeFactor, dueEpochSec)
    if (bundle.studyDeck && !bundle.studyDeck->cards.empty()) {
        json dj;
        dj["title"] = bundle.studyDeck->titleUtf8;
        json arr = json::array();
        for (const auto& c : bundle.studyDeck->cards) {
            arr.push_back({
                {"id", c.id},
                {"q", c.questionUtf8},
                {"a", c.answerUtf8},
                {"page", c.sourcePageIndex},
                {"reps", c.repetitions},
                {"intervalDays", c.intervalDays},
                {"ease", c.easeFactor},
                {"due", c.dueEpochSec}
            });
        }
        dj["cards"] = std::move(arr);
        doc.SetExtensionPayload("study_deck.json", dj.dump(2));
    }
    // 8. Custom CSS snippet
    if (!bundle.customCssUtf8.empty()) {
        doc.SetExtensionPayload("custom_css.txt", bundle.customCssUtf8);
    }
}

void StudioProfileStore::UnpackExtensionsFromDocument(const DocumentModel& doc, DocumentExtensionBundle& bundle) {
    if (const auto* pRaw = doc.FindExtensionPayload("properties.json")) {
        try {
            const json pj = json::parse(*pRaw);
            bundle.yamlFrontmatterUtf8 = pj.value("yamlRaw", "");
            bundle.activeDbViewKind = pj.value("activeDbViewKind", 0);
            bundle.aliasesUtf8.clear();
            if (pj.contains("aliases") && pj["aliases"].is_array()) {
                for (const auto& a : pj["aliases"]) if (a.is_string()) bundle.aliasesUtf8.push_back(a.get<std::string>());
            }
            bundle.tagsUtf8.clear();
            if (pj.contains("tags") && pj["tags"].is_array()) {
                for (const auto& t : pj["tags"]) if (t.is_string()) bundle.tagsUtf8.push_back(t.get<std::string>());
            }
            bundle.properties.clear();
            if (pj.contains("properties") && pj["properties"].is_array()) {
                for (const auto& item : pj["properties"]) {
                    NoteProperty np;
                    np.keyUtf8 = item.value("key", "");
                    np.type = static_cast<NotePropType>(std::clamp(item.value("type", 0), 0, 4));
                    np.valueUtf8 = item.value("value", "");
                    if (item.contains("list") && item["list"].is_array()) {
                        for (const auto& lv : item["list"]) if (lv.is_string()) np.listValuesUtf8.push_back(lv.get<std::string>());
                    }
                    if (!np.keyUtf8.empty()) bundle.properties.push_back(std::move(np));
                }
            }
        } catch (...) {}
    }

    if (bundle.comments) {
        if (const auto* raw = doc.FindExtensionPayload("comments.json")) {
            bundle.comments->DeserializeFromJson(*raw);
        }
    }
    if (bundle.database) {
        if (const auto* raw = doc.FindExtensionPayload("database.json")) {
            bundle.database->DeserializeFromBaseJson(*raw);
        }
    }
    if (bundle.permissions) {
        if (const auto* raw = doc.FindExtensionPayload("permissions.json")) {
            bundle.permissions->DeserializeFromJson(*raw);
        }
    }
    if (bundle.automations) {
        if (const auto* raw = doc.FindExtensionPayload("automations.json")) {
            bundle.automations->DeserializeFromJson(*raw);
        }
    }
    if (bundle.flexcil) {
        if (const auto* raw = doc.FindExtensionPayload("flexcil.json")) {
            bundle.flexcil->DeserializeStateFromJson(*raw);
        }
    }
    if (bundle.studyDeck) {
        if (const auto* raw = doc.FindExtensionPayload("study_deck.json")) {
            try {
                const json dj = json::parse(*raw);
                bundle.studyDeck->titleUtf8 = dj.value("title", bundle.studyDeck->titleUtf8);
                bundle.studyDeck->cards.clear();
                if (dj.contains("cards") && dj["cards"].is_array()) {
                    for (const auto& cj : dj["cards"]) {
                        FlashcardItem fc;
                        fc.id = cj.value("id", std::uint64_t{ 0 });
                        fc.questionUtf8 = cj.value("q", "");
                        fc.answerUtf8 = cj.value("a", "");
                        fc.sourcePageIndex = cj.value("page", std::size_t{ 0 });
                        fc.repetitions = cj.value("reps", 0);
                        fc.intervalDays = cj.value("intervalDays", 0);
                        fc.easeFactor = cj.value("ease", 2.5);
                        fc.dueEpochSec = cj.value("due", std::uint64_t{ 0 });
                        if (!fc.questionUtf8.empty()) bundle.studyDeck->cards.push_back(std::move(fc));
                    }
                }
            } catch (...) {}
        } else {
            bundle.studyDeck->cards.clear();
        }
    }
    if (const auto* css = doc.FindExtensionPayload("custom_css.txt")) {
        bundle.customCssUtf8 = *css;
    }
}

bool StudioProfileStore::SaveUiPreferences(const std::wstring& path, const StudioUiPreferences& prefs, std::string* outErr) {
    json j = {
        {"themeProfileId", prefs.themeProfileIdUtf8},
        {"toolbarOrientation", prefs.toolbarOrientation},
        {"customCssSnippet", prefs.customCssSnippetUtf8},
        {"totalStudySeconds", prefs.totalStudySeconds},
        {"hardwareGpuAcceleration", prefs.hardwareGpuAcceleration},
        {"snapShapesEnabled", prefs.snapShapesEnabled},
        {"scribbleEraseEnabled", prefs.scribbleEraseEnabled},
        {"showExperimentalFeatures", prefs.showExperimentalFeatures},
        {"autoSync", {
            {"enabled", prefs.autoSyncEnabled},
            {"folder", prefs.autoSyncFolderUtf8},
            {"encrypted", prefs.autoSyncEncrypted}
        }},
        {"llm", {
            {"enabled", prefs.llmConfig.enabled},
            {"endpointUrl", prefs.llmConfig.endpointUrlUtf8},
            {"modelName", prefs.llmConfig.modelNameUtf8},
            {"timeoutMs", prefs.llmConfig.timeoutMs}
            // Note: optional apiKey is intentionally NOT persisted in plain JSON on disk for privacy
        }},
        {"whisper", {
            {"exePath", prefs.whisperExePathUtf8},
            {"modelPath", prefs.whisperModelPathUtf8}
        }},
        {"accessibility", {
            {"readingRulerEnabled", prefs.accessibility.readingRulerEnabled},
            {"readingRulerHeightPx", prefs.accessibility.readingRulerHeightPx},
            {"readingRulerMaskOpacity", prefs.accessibility.readingRulerMaskOpacity},
            {"dyslexiaFriendlyMode", prefs.accessibility.dyslexiaFriendlyMode},
            {"paperTint", static_cast<int>(prefs.accessibility.paperTint)},
            {"colorBlindMode", static_cast<int>(prefs.accessibility.colorBlindMode)},
            {"uiFontScalePct", prefs.accessibility.uiFontScalePct},
            {"enhancedFocusRing", prefs.accessibility.enhancedFocusRing},
            {"ttsRate", prefs.accessibility.ttsRate},
            {"ttsVolume", prefs.accessibility.ttsVolume},
            {"ttsVoiceName", prefs.accessibility.ttsVoiceNameUtf8}
        }}
    };
    return WriteUtf8File(fs::path(path), j.dump(2), outErr);
}

bool StudioProfileStore::LoadUiPreferences(const std::wstring& path, StudioUiPreferences& prefs, std::string* outErr) {
    std::string raw;
    if (!ReadUtf8File(fs::path(path), raw, outErr)) return false;
    try {
        const json j = json::parse(raw);
        prefs.themeProfileIdUtf8 = j.value("themeProfileId", "porcelana-claro");
        prefs.toolbarOrientation = std::clamp(j.value("toolbarOrientation", 0), 0, 2);
        prefs.customCssSnippetUtf8 = j.value("customCssSnippet", "");
        prefs.totalStudySeconds = j.value("totalStudySeconds", std::uint64_t{ 0 });
        prefs.hardwareGpuAcceleration = j.value("hardwareGpuAcceleration", false);
        prefs.snapShapesEnabled = j.value("snapShapesEnabled", false);
        prefs.scribbleEraseEnabled = j.value("scribbleEraseEnabled", false);
        prefs.showExperimentalFeatures = j.value("showExperimentalFeatures", false);
        if (j.contains("autoSync") && j["autoSync"].is_object()) {
            const auto& aj = j["autoSync"];
            prefs.autoSyncEnabled = aj.value("enabled", false);
            prefs.autoSyncFolderUtf8 = aj.value("folder", "");
            prefs.autoSyncEncrypted = aj.value("encrypted", false);
        }
        if (j.contains("llm") && j["llm"].is_object()) {
            const auto& lj = j["llm"];
            prefs.llmConfig.enabled = lj.value("enabled", false);
            prefs.llmConfig.endpointUrlUtf8 = lj.value("endpointUrl", "http://127.0.0.1:11434/api/chat");
            prefs.llmConfig.modelNameUtf8 = lj.value("modelName", "llama3.2");
            prefs.llmConfig.timeoutMs = std::clamp(lj.value("timeoutMs", 15000), 2000, 120000);
        }
        if (j.contains("whisper") && j["whisper"].is_object()) {
            const auto& wj = j["whisper"];
            prefs.whisperExePathUtf8 = wj.value("exePath", "");
            prefs.whisperModelPathUtf8 = wj.value("modelPath", "");
        }
        if (j.contains("accessibility") && j["accessibility"].is_object()) {
            const auto& aj = j["accessibility"];
            prefs.accessibility.readingRulerEnabled = aj.value("readingRulerEnabled", false);
            prefs.accessibility.readingRulerHeightPx = std::clamp(aj.value("readingRulerHeightPx", 76), 44, 180);
            prefs.accessibility.readingRulerMaskOpacity = std::clamp(aj.value("readingRulerMaskOpacity", 0.42), 0.18, 0.78);
            prefs.accessibility.dyslexiaFriendlyMode = aj.value("dyslexiaFriendlyMode", false);
            prefs.accessibility.paperTint = static_cast<PaperTintPreset>(std::clamp(aj.value("paperTint", 0), 0, 4));
            prefs.accessibility.colorBlindMode = static_cast<ColorBlindPaletteMode>(std::clamp(aj.value("colorBlindMode", 0), 0, 3));
            prefs.accessibility.uiFontScalePct = std::clamp(aj.value("uiFontScalePct", 100), 100, 150);
            prefs.accessibility.enhancedFocusRing = aj.value("enhancedFocusRing", false);
            prefs.accessibility.ttsRate = std::clamp(aj.value("ttsRate", 0), -10, 10);
            prefs.accessibility.ttsVolume = std::clamp(aj.value("ttsVolume", 95), 0, 100);
            prefs.accessibility.ttsVoiceNameUtf8 = aj.value("ttsVoiceName", "");
        }
        return true;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

bool StudioProfileStore::SaveAllProfileState(
    const std::wstring& rootDir,
    const LibraryContentViewsManager& library,
    const ReusableElementLibrary& snippets,
    const KeymapProfile& keymap,
    const KnowledgeVault& vault,
    const StudioUiPreferences& prefs,
    std::string* outErr)
{
    const fs::path root(rootDir);
    std::error_code ec;
    fs::create_directories(root, ec);

    bool ok = true;
    ok = library.SaveToFile((root / L"library_index.json").wstring(), outErr) && ok;
    ok = snippets.SaveToFile((root / L"snippets.json").wstring(), outErr) && ok;
    ok = WriteUtf8File(root / L"keymap.json", keymap.ToJson().dump(2), outErr) && ok;
    ok = SaveUiPreferences((root / L"ui_settings.json").wstring(), prefs, outErr) && ok;
    if (vault.TotalNoteCount() > 0) {
        ok = vault.SaveVaultToDirectory((root / L"vault").wstring(), outErr) && ok;
    }
    return ok;
}

bool StudioProfileStore::LoadAllProfileState(
    const std::wstring& rootDir,
    LibraryContentViewsManager& library,
    ReusableElementLibrary& snippets,
    KeymapProfile& keymap,
    KnowledgeVault& vault,
    StudioUiPreferences& prefs,
    std::string* outErr)
{
    (void)outErr;
    const fs::path root(rootDir);
    if (!fs::exists(root)) return false;

    bool loadedAny = false;
    if (fs::exists(root / L"library_index.json")) {
        loadedAny = library.LoadFromFile((root / L"library_index.json").wstring(), nullptr) || loadedAny;
    }
    if (fs::exists(root / L"snippets.json")) {
        loadedAny = snippets.LoadFromFile((root / L"snippets.json").wstring(), nullptr) || loadedAny;
    }
    if (fs::exists(root / L"keymap.json")) {
        std::string raw;
        if (ReadUtf8File(root / L"keymap.json", raw, nullptr)) {
            try {
                keymap = KeymapProfile::FromJson(json::parse(raw));
                loadedAny = true;
            } catch (...) {}
        }
    }
    if (fs::exists(root / L"ui_settings.json")) {
        loadedAny = LoadUiPreferences((root / L"ui_settings.json").wstring(), prefs, nullptr) || loadedAny;
    }
    if (fs::exists(root / L"vault" / L"cuadernu_vault_meta.json")) {
        loadedAny = vault.LoadVaultFromDirectory((root / L"vault").wstring(), nullptr) || loadedAny;
    }
    return loadedAny;
}

} // namespace DocStudio
