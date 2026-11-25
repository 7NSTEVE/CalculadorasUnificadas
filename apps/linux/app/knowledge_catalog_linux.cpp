#include "knowledge_catalog_linux.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace CasioKnowledgeLinux {

namespace {

std::string JsonString(const nlohmann::json& j, const char* key, const std::string& def = "") {
    if (!j.is_object() || !j.contains(key)) return def;
    const auto& v = j.at(key);
    return v.is_string() ? v.get<std::string>() : def;
}

int JsonInt(const nlohmann::json& j, const char* key, int def = 0) {
    if (!j.is_object() || !j.contains(key)) return def;
    const auto& v = j.at(key);
    return v.is_number_integer() ? v.get<int>() : def;
}

bool JsonBool(const nlohmann::json& j, const char* key, bool def = false) {
    if (!j.is_object() || !j.contains(key)) return def;
    const auto& v = j.at(key);
    return v.is_boolean() ? v.get<bool>() : def;
}

std::vector<int> JsonIntVec(const nlohmann::json& j, const char* key) {
    std::vector<int> out;
    if (!j.is_object() || !j.contains(key)) return out;
    const auto& arr = j.at(key);
    if (!arr.is_array()) return out;
    for (const auto& item : arr) {
        if (item.is_number_integer()) out.push_back(item.get<int>());
    }
    return out;
}

KeyModeFunc ParseModeFunc(const nlohmann::json& j, const char* key) {
    KeyModeFunc f{};
    if (!j.is_object() || !j.contains(key)) return f;
    const auto& obj = j.at(key);
    f.available    = JsonBool(obj, "available", false);
    f.label        = JsonString(obj, "label");
    f.category     = JsonString(obj, "category");
    f.summary      = JsonString(obj, "summary");
    f.latexFormula = JsonString(obj, "latexFormula");
    f.latexExample = JsonString(obj, "latexExample");
    f.tipOrWarning = JsonString(obj, "tipOrWarning");
    return f;
}

int ScoreFieldsAgainstTokens(const std::vector<std::string>& tokens,
                             const std::string& primaryNorm,
                             const std::string& secondaryNorm) {
    if (tokens.empty()) return 10;
    int score = 0;
    for (const auto& tk : tokens) {
        bool inPrim = (primaryNorm.find(tk) != std::string::npos);
        bool inSec  = (secondaryNorm.find(tk) != std::string::npos);
        if (!inPrim && !inSec) return 0; // Require all query tokens to match
        if (inPrim) score += 25;
        if (inSec) score += 10;
    }
    return score;
}

bool ModelMatchesFilter(int entryCalcIdx, int modelFilter) {
    if (modelFilter < 0) return true;
    return entryCalcIdx == modelFilter;
}

} // namespace

std::string NormalizeSearchText(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        // Handle common UTF-8 Spanish Latin-1 diacritics (0xC3 xx)
        if (c == 0xC3 && i + 1 < input.size()) {
            unsigned char n = static_cast<unsigned char>(input[i + 1]);
            char mapped = 0;
            switch (n) {
            case 0xA1: case 0x81: case 0xA0: case 0x80: case 0xA4: case 0x84: mapped = 'a'; break;
            case 0xA9: case 0x89: case 0xA8: case 0x88: case 0xAB: case 0x8B: mapped = 'e'; break;
            case 0xAD: case 0x8D: case 0xAC: case 0x8C: case 0xAF: case 0x8F: mapped = 'i'; break;
            case 0xB3: case 0x93: case 0xB2: case 0x92: case 0xB6: case 0x96: mapped = 'o'; break;
            case 0xBA: case 0x9A: case 0xB9: case 0x99: case 0xBC: case 0x9C: mapped = 'u'; break;
            case 0xB1: case 0x91: mapped = 'n'; break;
            default: break;
            }
            if (mapped != 0) {
                out.push_back(mapped);
                ++i;
                continue;
            }
        }
        if (c < 0x80) {
            if (std::isalnum(c) || c == '+' || c == '-' || c == '=' || c == '/' || c == '^') {
                out.push_back(static_cast<char>(std::tolower(c)));
            } else {
                out.push_back(' ');
            }
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

std::vector<std::string> TokenizeSearchQuery(const std::string& query) {
    std::string norm = NormalizeSearchText(query);
    std::vector<std::string> tokens;
    std::istringstream iss(norm);
    std::string word;
    while (iss >> word) {
        if (word.size() >= 1) tokens.push_back(word);
    }
    return tokens;
}

std::string CalcModelDisplayName(int calcIdx) {
    switch (calcIdx) {
    case 0: return "fx-570LA X (Blanca)";
    case 1: return "fx-570VN PLUS (Gris)";
    case 2: return "fx-CG50 PRIZM";
    case 3: return "fx-991CW (Negra)";
    default: return "Todas";
    }
}

const std::vector<OfficialPdfManual>& KnowledgeCatalog::OfficialManuals() const noexcept {
    static const std::vector<OfficialPdfManual> kManuals = {
        {"fx-570CW_991CW_ES.pdf", "991CW: Manual Completo (ES)", "Guía del Usuario Oficial fx-570CW / fx-991CW (Español, 158 pág.)", "Negra (fx-991CW)", "Usuario / Hardware & Software (ES)", 1, 8},
        {"fx-570CW_991CW_EN.pdf", "991CW: User's Guide (EN)", "Official User's Guide fx-570CW / fx-991CW (English, 151 pp.)", "Negra (fx-991CW)", "User's Guide / Hardware & Software (EN)", 1, 8},
        {"fx-570_991LA_X_ES.pdf", "Blanca: Hardware LA X (ES)", "Guía del Usuario Oficial fx-570LA X / fx-991LA X (Español, 49 pág.)", "Blanca (ClassWiz)", "Hardware / Usuario (ES)", 1, 1},
        {"fx-570_991EX_ES.pdf", "Blanca: Hardware EX (ES)", "Guía del Usuario Oficial fx-570EX / fx-991EX (Español, 48 pág.)", "Blanca (ClassWiz)", "Hardware / Usuario (ES)", 1, 1},
        {"fx-570_991EX_EN.pdf", "Blanca: Hardware EX (EN)", "Official User's Guide fx-570EX / fx-991EX (English, 46 pp.)", "Blanca (ClassWiz)", "Hardware / User (EN)", 1, 1},
        {"ClassWiz_Emulator_Software_Users_Guide_ES.pdf", "ClassWiz: Software Emulador", "Guía del Usuario Software ClassWiz Emulator Subscription v2.0 (88 pág.)", "Blanca/Negra (ClassWiz)", "Software Emulador", 4, 1 | 8},
        {"CASIO_Activation_Guide_ES.pdf", "ClassWiz: Guía Activación", "Guía Oficial de Activación y Licencia Emulador CASIO (112 pág.)", "Blanca/Negra (ClassWiz)", "Software Licencia", 4, 1 | 2 | 8},
        {"fx-570_991ES_PLUS_ES.pdf", "Gris: Hardware ES+ (ES)", "Guía del Usuario Oficial fx-570ES PLUS / fx-991ES PLUS (Español, 48 pág.)", "Gris (ES/VN PLUS)", "Hardware / Usuario (ES)", 2, 2},
        {"fx-570_991ES_PLUS_EN.pdf", "Gris: Hardware ES+ (EN)", "Official User's Guide fx-570ES PLUS / fx-991ES PLUS (English, 46 pp.)", "Gris (ES/VN PLUS)", "Hardware / User (EN)", 2, 2},
        {"fx-570VN_PLUS_Hardware_Users_Guide.pdf", "Gris: Hardware VN PLUS", "Manual Oficial Completo fx-570VN PLUS Natural-V.P.A.M. (62 pág.)", "Gris (ES/VN PLUS)", "Hardware / Usuario", 2, 2},
        {"fx-CG50_Soft_v370_ES.pdf", "CG50: Manual Completo (ES)", "Guía del Usuario Completa Software y Funciones fx-CG50 OS v3.70 (Español, 678 pág.)", "Graficadora (fx-CG50)", "Software / Funciones (ES)", 3, 4},
        {"fx-CG50_Soft_v370_EN.pdf", "CG50: Complete Guide (EN)", "Complete Software & Operations User's Guide fx-CG50 OS v3.70 (English, 677 pp.)", "Graficadora (fx-CG50)", "Software / Functions (EN)", 3, 4},
        {"fx-CG50_Hard_ES.pdf", "CG50: Guía Hardware (ES)", "Guía del Usuario de Hardware fx-CG50 (Español, 17 pág.)", "Graficadora (fx-CG50)", "Hardware Físico (ES)", 3, 4},
        {"fx-CG50_Hard_EN.pdf", "CG50: Hardware Guide (EN)", "Hardware User's Guide fx-CG50 PRIZM (English, 17 pp.)", "Graficadora (fx-CG50)", "Physical Hardware (EN)", 3, 4},
        {"fx-CG_Manager_PLUS_Software_Users_Guide_EN.pdf", "CG50: Emulador Manager+", "Software User's Guide fx-CG Manager PLUS Subscription (45 pp.)", "Graficadora (fx-CG50)", "Software Emulador", 4, 4},
        {"fx-CG_Manager_PLUS_Activation_Guide.pdf", "CG50: Activación Manager+", "Guía Oficial de Activación fx-CG Manager PLUS (270 pág.)", "Graficadora (fx-CG50)", "Software Licencia", 4, 4},
    };
    return kManuals;
}

void KnowledgeCatalog::InitCuratedGuides() {
    curated_ = {
        {
            "Fracción + Decimal + Alternancia S<=>D",
            "Básicas", "Básico",
            "$\\frac{3}{4} + \\frac{1}{8} = \\frac{7}{8} = 0.875$",
            "[a/b] -> 3 -> [Abajo] -> 4 -> [Derecha] -> [+] -> [a/b] -> 1 -> [Abajo] -> 8 -> [=] -> [S<=>D]",
            "7/8  <->  0.875",
            "MthIO (Natural-V.P.A.M.) preserva resultados racionales exactos; S<=>D alterna entre fracción y decimal sin reescribir.",
            "fx-570_991LA_X_ES.pdf", 12, 0,
            {12, 42, 5, 35, 4, 43, 12, 40, 5, 30, 49, 28}
        },
        {
            "Integral Definida Numérica (Gauss-Kronrod)",
            "Cálculo", "Intermedio",
            "$\\int_{0}^{3} (x^{2} + 2x)\\,dx = 18$",
            "[INT] -> [x] -> [x^2] -> [+] -> 2 -> [x] -> [Derecha] -> 0 -> [Derecha] -> 3 -> [=]",
            "18",
            "Evalúa integrales definidas de funciones reales continuas en el intervalo [a, b] mediante cuadratura adaptativa.",
            "fx-570_991LA_X_ES.pdf", 17, 0,
            {10, 11, 14, 43, 41, 11, 4, 45, 4, 42, 49}
        },
        {
            "Derivada Numérica en un Punto f'(a)",
            "Cálculo", "Intermedio",
            "$\\left.\\frac{d}{dx}(x^{3})\\right|_{x=2} = 12$",
            "[SHIFT] -> [INT] (d/dx) -> [x] -> [x^n] -> 3 -> [Derecha] -> [Derecha] -> 2 -> [=]",
            "12",
            "Diferenciación numérica centrada alrededor de x=2 con paso óptimo automático.",
            "fx-570_991LA_X_ES.pdf", 17, 0,
            {0, 10, 11, 15, 42, 4, 4, 41, 49}
        },
        {
            "Sumatoria Sigma con Índice Variable",
            "Cálculo", "Intermedio",
            "$\\sum_{x=1}^{5} x^{2} = 1 + 4 + 9 + 16 + 25 = 55$",
            "[SHIFT] -> [x] (Sigma) -> [x] -> [x^2] -> [Derecha] -> 1 -> [Derecha] -> 5 -> [=]",
            "55",
            "Evalúa sumatorias finitas de términos algebraicos sin necesidad de expandir término a término.",
            "fx-570_991ES_PLUS_ES.pdf", 17, 0,
            {0, 11, 11, 14, 4, 40, 4, 36, 49}
        },
        {
            "Operación Rápida de Trigonometría (Grados)",
            "Trigonometría", "Básico",
            "$\\sin(30^{\\circ}) + \\cos(60^{\\circ}) = 1$",
            "[sin] -> 3 -> 0 -> [)] -> [+] -> [cos] -> 6 -> 0 -> [)] -> [=]",
            "1",
            "Verifica el indicador 'D' en la fila superior LCD antes de evaluar ángulos en grados sexagesimales.",
            "fx-570_991LA_X_ES.pdf", 16, 0,
            {21, 42, 45, 27, 43, 22, 37, 45, 27, 49}
        },
        {
            "Multiplicación y Operaciones con Raíces Exactas",
            "Álgebra", "Básico",
            "$\\sqrt{8} + \\sqrt{18} = 5\\sqrt{2} \\approx 7.0710678$",
            "[sqrt] -> 8 -> [Derecha] -> [+] -> [sqrt] -> 1 -> 8 -> [=]",
            "5√2",
            "ClassWiz y ES PLUS simplifican radicales cuadráticos de forma exacta automáticamente en MthIO.",
            "fx-570_991LA_X_ES.pdf", 13, 0,
            {13, 31, 4, 43, 13, 40, 31, 49}
        },
        {
            "Notación Científica y Constante Pi (Gris ES PLUS)",
            "Científica", "Básico",
            "$2\\pi \\times 10^{3} \\approx 6283.1853$",
            "2 -> [SHIFT] -> [EXP] (Pi) -> [x] -> 1 -> [EXP] -> 3 -> [=]",
            "2000π / 6283.185",
            "La tecla x10^x integra el exponente al literal numérico, evitando errores de paréntesis al dividir.",
            "fx-570_991ES_PLUS_ES.pdf", 12, 1,
            {41, 0, 47, 38, 40, 47, 42, 49}
        },
        {
            "Operación Completa Base en fx-991CW (ClassWiz II)",
            "Básicas", "Básico",
            "$12 \\times (3 + 4) = 84$",
            "1 -> 2 -> [x] -> [(] -> 3 -> [+] -> 4 -> [)] -> [EXE]",
            "84",
            "En ClassWiz II fx-991CW la tecla igual fue reemplazada por EXE y el catálogo unificado reside en CATALOG / TOOLS.",
            "fx-570CW_991CW_ES.pdf", 10, 3,
            {38, 39, 36, 26, 40, 41, 33, 27, 47}
        }
    };
}

bool KnowledgeCatalog::Load(const CasioLinux::AssetPaths& paths, std::string* outDiag) {
    namespace fs = std::filesystem;
    keys_.clear();
    indicators_.clear();
    procedures_.clear();
    diagnostics_.clear();
    errors_.clear();
    tsvPages_.clear();
    InitCuratedGuides();

    std::string jsonPath = paths.catalogJsonPath;
    if (jsonPath.empty() && !paths.rootDir.empty()) {
        jsonPath = (fs::path(paths.rootDir) / "catalog" / "casio_knowledge_catalog.json").string();
    }

    int loadedJsonSections = 0;
    if (!jsonPath.empty()) {
        std::ifstream jf(jsonPath);
        if (jf.is_open()) {
            try {
                nlohmann::json root;
                jf >> root;

                if (root.contains("keys") && root["keys"].is_array()) {
                    for (const auto& jk : root["keys"]) {
                        KeyEntry ke{};
                        ke.calcIdx = JsonInt(jk, "calcIdx", 0);
                        ke.keyId   = JsonInt(jk, "keyId", 0);
                        ke.keyName = JsonString(jk, "keyName");
                        ke.pdfFile = JsonString(jk, "pdfFile");
                        ke.pdfPage = JsonInt(jk, "pdfPage", 1);
                        ke.demoSeq = JsonIntVec(jk, "demoSeq");
                        ke.normal  = ParseModeFunc(jk, "normal");
                        ke.shift   = ParseModeFunc(jk, "shift");
                        ke.alpha   = ParseModeFunc(jk, "alpha");
                        keys_.push_back(std::move(ke));
                    }
                    loadedJsonSections++;
                }

                if (root.contains("indicators") && root["indicators"].is_array()) {
                    for (const auto& ji : root["indicators"]) {
                        IndicatorEntry ie{};
                        ie.calcIdx     = JsonInt(ji, "calcIdx", 0);
                        ie.spriteIndex = JsonInt(ji, "spriteIndex", 0);
                        ie.symbol      = JsonString(ji, "symbol");
                        ie.title       = JsonString(ji, "title");
                        ie.meaning     = JsonString(ji, "meaning");
                        ie.whenAppears = JsonString(ji, "whenAppears");
                        ie.relatedKeys = JsonString(ji, "relatedKeys");
                        ie.pdfFile     = JsonString(ji, "pdfFile");
                        ie.pdfPage     = JsonInt(ji, "pdfPage", 1);
                        indicators_.push_back(std::move(ie));
                    }
                    loadedJsonSections++;
                }

                if (root.contains("procedures") && root["procedures"].is_array()) {
                    for (const auto& jp : root["procedures"]) {
                        ProcedureEntry pe{};
                        pe.id           = JsonString(jp, "id");
                        pe.category     = JsonString(jp, "category");
                        pe.title        = JsonString(jp, "title");
                        pe.goalSummary  = JsonString(jp, "goalSummary");
                        pe.latexFormula = JsonString(jp, "latexFormula");
                        pe.commonErrors = JsonString(jp, "commonErrors");
                        pe.tags         = JsonString(jp, "tags");
                        if (jp.contains("models") && jp["models"].is_array()) {
                            for (const auto& jm : jp["models"]) {
                                ProcModelVariant mv{};
                                mv.calcIdx      = JsonInt(jm, "calcIdx", 0);
                                mv.supported    = JsonBool(jm, "supported", false);
                                mv.keys         = JsonIntVec(jm, "keys");
                                mv.stepsSummary = JsonString(jm, "stepsSummary");
                                mv.expectedLcd  = JsonString(jm, "expectedLcd");
                                mv.pdfFile      = JsonString(jm, "pdfFile");
                                mv.pdfPage      = JsonInt(jm, "pdfPage", 1);
                                pe.models.push_back(std::move(mv));
                            }
                        }
                        procedures_.push_back(std::move(pe));
                    }
                    loadedJsonSections++;
                }

                if (root.contains("diagnostics") && root["diagnostics"].is_array()) {
                    for (const auto& jd : root["diagnostics"]) {
                        DiagnosticEntry de{};
                        de.id              = JsonString(jd, "id");
                        de.symptom         = JsonString(jd, "symptom");
                        de.category        = JsonString(jd, "category");
                        de.why             = JsonString(jd, "why");
                        de.latex           = JsonString(jd, "latex");
                        de.fix             = JsonString(jd, "fix");
                        de.quickFixCalcIdx = JsonInt(jd, "quickFixCalcIdx", -1);
                        de.quickFixSeq     = JsonIntVec(jd, "quickFixSeq");
                        diagnostics_.push_back(std::move(de));
                    }
                    loadedJsonSections++;
                }

                if (root.contains("errors") && root["errors"].is_array()) {
                    for (const auto& je : root["errors"]) {
                        ErrorEntry ee{};
                        ee.name       = JsonString(je, "name");
                        ee.appliesTo  = JsonString(je, "appliesTo");
                        ee.cause      = JsonString(je, "cause");
                        ee.resolution = JsonString(je, "resolution");
                        ee.pdfFile    = JsonString(je, "pdfFile");
                        ee.pdfPage    = JsonInt(je, "pdfPage", 1);
                        errors_.push_back(std::move(ee));
                    }
                    loadedJsonSections++;
                }
            } catch (const std::exception& ex) {
                if (outDiag) *outDiag = std::string("Error JSON catalog: ") + ex.what();
            }
        }
    }

    // Load 6-column TSV manual index if present
    fs::path tsvPath;
    if (!paths.manualsDir.empty()) tsvPath = fs::path(paths.manualsDir) / "indice_paginas_pdf.tsv";
    else if (!paths.rootDir.empty()) tsvPath = fs::path(paths.rootDir) / "manuales" / "indice_paginas_pdf.tsv";

    if (!tsvPath.empty()) {
        std::ifstream tf(tsvPath);
        std::string line;
        bool firstLine = true;
        while (std::getline(tf, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            if (firstLine) {
                firstLine = false;
                if (line.rfind("fileName\t", 0) == 0) continue;
            }
            std::vector<std::string> cols;
            size_t start = 0;
            while (true) {
                size_t tab = line.find('\t', start);
                if (tab == std::string::npos) {
                    cols.push_back(line.substr(start));
                    break;
                }
                cols.push_back(line.substr(start, tab - start));
                start = tab + 1;
            }
            if (cols.size() >= 6) {
                PdfTsvEntry row{};
                row.fileName = cols[0];
                row.modelCat = cols[1];
                row.docType  = cols[2];
                try { row.pageNo = std::max(1, std::stoi(cols[3])); } catch (...) { row.pageNo = 1; }
                row.heading  = cols[4];
                row.snippet  = cols[5];
                tsvPages_.push_back(std::move(row));
            }
        }
    }

    loaded_ = (loadedJsonSections > 0 || !curated_.empty());
    if (outDiag && outDiag->empty()) {
        std::ostringstream oss;
        oss << keys_.size() << " teclas, " << indicators_.size() << " indicadores, "
            << procedures_.size() << " procedimientos, " << tsvPages_.size() << " paginas TSV";
        *outDiag = oss.str();
    }
    return loaded_;
}

const KeyEntry* KnowledgeCatalog::FindKey(int calcIdx, int keyId) const {
    for (const auto& k : keys_) {
        if (k.calcIdx == calcIdx && k.keyId == keyId) return &k;
    }
    return nullptr;
}

const IndicatorEntry* KnowledgeCatalog::FindIndicator(int calcIdx, int spriteIndex) const {
    for (const auto& ind : indicators_) {
        if (ind.calcIdx == calcIdx && ind.spriteIndex == spriteIndex) return &ind;
    }
    return nullptr;
}

std::vector<SmartHelpItem> KnowledgeCatalog::Search(const std::string& query,
                                                    int categoryFilter,
                                                    int modelFilter,
                                                    size_t maxResults) const {
    const auto tokens = TokenizeSearchQuery(query);
    std::vector<SmartHelpItem> results;

    const bool wantGuides   = (categoryFilter < 0 || categoryFilter == 0);
    const bool wantProcs    = (categoryFilter < 0 || categoryFilter == 1 || categoryFilter == 0);
    const bool wantKeys     = (categoryFilter < 0 || categoryFilter == 2);
    const bool wantDiagErr  = (categoryFilter < 0 || categoryFilter == 3);
    const bool wantManuals  = (categoryFilter < 0 || categoryFilter == 4);

    if (wantGuides) {
        for (const auto& g : curated_) {
            if (!ModelMatchesFilter(g.calcIdx, modelFilter)) continue;
            int sc = ScoreFieldsAgainstTokens(
                tokens,
                NormalizeSearchText(g.title + " " + g.category + " " + g.difficulty),
                NormalizeSearchText(g.whyItWorks + " " + g.stepsSummary + " " + g.expectedResult));
            if (sc <= 0) continue;
            SmartHelpItem it{};
            it.kind         = HelpResultKind::Guide;
            it.title        = g.title;
            it.badge        = "GUÍA (" + CalcModelDisplayName(g.calcIdx) + ")";
            it.category     = g.category;
            it.summary      = g.whyItWorks;
            it.latexFormula = g.latexFormula;
            it.latexExample = g.expectedResult;
            it.stepsText    = g.stepsSummary;
            it.calcIdx      = g.calcIdx;
            it.demoSeq      = g.keys;
            it.pdfFile      = g.pdfFile;
            it.pdfPage      = g.pdfPage;
            it.score        = sc + 40;
            results.push_back(std::move(it));
        }
    }

    if (wantProcs) {
        for (const auto& p : procedures_) {
            const ProcModelVariant* chosen = nullptr;
            for (const auto& mv : p.models) {
                if (modelFilter >= 0 && mv.calcIdx != modelFilter) continue;
                if (!chosen || (mv.supported && !chosen->supported)) chosen = &mv;
            }
            if (!chosen && modelFilter == 3 && !p.models.empty()) {
                chosen = &p.models[0]; // Blanca ClassWiz steps are closest base for CW2
            }
            if (!chosen && modelFilter >= 0) continue;

            std::string stepBlob = chosen ? chosen->stepsSummary : "";
            int sc = ScoreFieldsAgainstTokens(
                tokens,
                NormalizeSearchText(p.title + " " + p.category + " " + p.tags),
                NormalizeSearchText(p.goalSummary + " " + p.commonErrors + " " + stepBlob));
            if (sc <= 0) continue;

            SmartHelpItem it{};
            it.kind         = HelpResultKind::Procedure;
            it.title        = p.title;
            it.badge        = "PROCEDIMIENTO · " + p.category;
            it.category     = p.category;
            it.summary      = p.goalSummary;
            it.latexFormula = p.latexFormula;
            it.latexExample = chosen ? ("Resultado esperado: " + chosen->expectedLcd) : "";
            it.tipOrWarning = p.commonErrors;
            it.stepsText    = chosen ? chosen->stepsSummary : "";
            it.calcIdx      = chosen ? chosen->calcIdx : 0;
            if (chosen && chosen->supported) it.demoSeq = chosen->keys;
            it.pdfFile      = chosen ? chosen->pdfFile : "";
            it.pdfPage      = chosen ? chosen->pdfPage : 1;
            it.score        = sc + 30;
            results.push_back(std::move(it));
        }
    }

    if (wantKeys) {
        for (const auto& k : keys_) {
            if (!ModelMatchesFilter(k.calcIdx, modelFilter)) continue;
            std::string prim = k.keyName + " " + k.normal.label + " " + k.shift.label + " " + k.alpha.label;
            std::string sec  = k.normal.summary + " " + k.shift.summary + " " + k.alpha.summary + " " +
                               k.normal.category + " " + k.shift.category;
            int sc = ScoreFieldsAgainstTokens(tokens, NormalizeSearchText(prim), NormalizeSearchText(sec));
            if (sc <= 0) continue;

            SmartHelpItem it{};
            it.kind = HelpResultKind::Key;
            it.title = "[" + k.keyName + "] — " + k.normal.label;
            it.badge = "TECLA · " + CalcModelDisplayName(k.calcIdx);
            it.category = k.normal.category;
            std::ostringstream sum;
            sum << k.normal.summary;
            if (k.shift.available) sum << "\nSHIFT: " << k.shift.label << " — " << k.shift.summary;
            if (k.alpha.available) sum << "\nALPHA: " << k.alpha.label << " — " << k.alpha.summary;
            it.summary      = sum.str();
            it.latexFormula = !k.shift.latexFormula.empty() && tokens.size() > 0 &&
                              NormalizeSearchText(k.shift.label).find(tokens[0]) != std::string::npos
                              ? k.shift.latexFormula
                              : k.normal.latexFormula;
            it.latexExample = !k.normal.latexExample.empty() ? k.normal.latexExample : k.shift.latexExample;
            it.tipOrWarning = !k.normal.tipOrWarning.empty() ? k.normal.tipOrWarning : k.shift.tipOrWarning;
            it.calcIdx      = k.calcIdx;
            it.demoSeq      = k.demoSeq.empty() ? std::vector<int>{k.keyId} : k.demoSeq;
            it.pdfFile      = k.pdfFile;
            it.pdfPage      = k.pdfPage;
            it.score        = sc + 15;
            results.push_back(std::move(it));
        }

        for (const auto& ind : indicators_) {
            if (!ModelMatchesFilter(ind.calcIdx, modelFilter)) continue;
            int sc = ScoreFieldsAgainstTokens(
                tokens,
                NormalizeSearchText(ind.symbol + " " + ind.title),
                NormalizeSearchText(ind.meaning + " " + ind.whenAppears + " " + ind.relatedKeys));
            if (sc <= 0) continue;

            SmartHelpItem it{};
            it.kind         = HelpResultKind::Indicator;
            it.title        = "Indicador [" + ind.symbol + "] " + ind.title;
            it.badge        = "INDICADOR LCD · " + CalcModelDisplayName(ind.calcIdx);
            it.category     = "Indicadores LCD";
            it.summary      = ind.meaning + "\nCuándo aparece: " + ind.whenAppears;
            it.tipOrWarning = "Teclas relacionadas: " + ind.relatedKeys;
            it.calcIdx      = ind.calcIdx;
            it.pdfFile      = ind.pdfFile;
            it.pdfPage      = ind.pdfPage;
            it.score        = sc + 12;
            results.push_back(std::move(it));
        }
    }

    if (wantDiagErr) {
        for (const auto& d : diagnostics_) {
            if (modelFilter >= 0 && d.quickFixCalcIdx >= 0 && d.quickFixCalcIdx != modelFilter) continue;
            int sc = ScoreFieldsAgainstTokens(
                tokens,
                NormalizeSearchText(d.symptom + " " + d.category),
                NormalizeSearchText(d.why + " " + d.fix));
            if (sc <= 0) continue;

            SmartHelpItem it{};
            it.kind         = HelpResultKind::Diagnostic;
            it.title        = d.symptom;
            it.badge        = "DIAGNÓSTICO · " + d.category;
            it.category     = d.category;
            it.summary      = d.why;
            it.latexFormula = d.latex;
            it.stepsText    = d.fix;
            it.calcIdx      = d.quickFixCalcIdx >= 0 ? d.quickFixCalcIdx : 0;
            it.demoSeq      = d.quickFixSeq;
            it.score        = sc + 20;
            results.push_back(std::move(it));
        }

        for (const auto& e : errors_) {
            int sc = ScoreFieldsAgainstTokens(
                tokens,
                NormalizeSearchText(e.name + " " + e.appliesTo),
                NormalizeSearchText(e.cause + " " + e.resolution));
            if (sc <= 0) continue;

            SmartHelpItem it{};
            it.kind         = HelpResultKind::Error;
            it.title        = e.name;
            it.badge        = "ERROR CASIO · " + e.appliesTo;
            it.category     = "Errores";
            it.summary      = "Causa: " + e.cause;
            it.tipOrWarning = "Solución: " + e.resolution;
            it.pdfFile      = e.pdfFile;
            it.pdfPage      = e.pdfPage;
            it.score        = sc + 18;
            results.push_back(std::move(it));
        }
    }

    if (wantManuals && (!tokens.empty() || categoryFilter == 4)) {
        size_t tsvAdded = 0;
        for (const auto& row : tsvPages_) {
            int sc = ScoreFieldsAgainstTokens(
                tokens,
                NormalizeSearchText(row.heading + " " + row.modelCat + " " + row.fileName),
                NormalizeSearchText(row.snippet));
            if (sc <= 0) continue;

            SmartHelpItem it{};
            it.kind     = HelpResultKind::ManualPage;
            it.title    = row.heading.empty() ? (row.fileName + " p." + std::to_string(row.pageNo)) : row.heading;
            it.badge    = "MANUAL p." + std::to_string(row.pageNo) + " · " + row.modelCat;
            it.category = row.docType;
            it.summary  = row.snippet;
            it.pdfFile  = row.fileName;
            it.pdfPage  = row.pageNo;
            it.score    = sc + (tokens.empty() ? 5 : 8);
            results.push_back(std::move(it));
            if (++tsvAdded >= 60) break;
        }
    }

    std::stable_sort(results.begin(), results.end(), [](const SmartHelpItem& a, const SmartHelpItem& b) {
        return a.score > b.score;
    });
    if (results.size() > maxResults) results.resize(maxResults);
    return results;
}

bool TryConvertKeySequenceBetweenModels(int srcCalcIdx, const std::vector<int>& srcKeys,
                                        int dstCalcIdx, std::vector<int>& outDstKeys) {
    outDstKeys.clear();
    if (srcCalcIdx == dstCalcIdx) {
        outDstKeys = srcKeys;
        return true;
    }
    const auto& srcDefs = CasioLinux::GetKeysForModel(static_cast<CasioLinux::CalcModelId>(srcCalcIdx));
    const auto& dstDefs = CasioLinux::GetKeysForModel(static_cast<CasioLinux::CalcModelId>(dstCalcIdx));
    if (srcDefs.empty() || dstDefs.empty()) return false;

    auto findDstByName = [&](const std::string& candidate) -> int {
        std::string normCand = NormalizeSearchText(candidate);
        for (const auto& dk : dstDefs) {
            if (dk.name == candidate || NormalizeSearchText(dk.name) == normCand) return dk.id;
        }
        return -1;
    };

    for (int sk : srcKeys) {
        std::string sName;
        for (const auto& sd : srcDefs) {
            if (sd.id == sk) { sName = sd.name; break; }
        }
        if (sName.empty()) return false;

        int mapped = findDstByName(sName);
        if (mapped < 0) {
            if (sName == "IGUAL" || sName == "EXE") {
                mapped = findDstByName("EXE");
                if (mapped < 0) mapped = findDstByName("IGUAL");
            } else if (sName == "MODE" || sName == "MENU") {
                mapped = findDstByName("MENU");
                if (mapped < 0) mapped = findDstByName("MODE");
            } else if (sName == "POT" || sName == "x^n") {
                mapped = findDstByName("x^n");
                if (mapped < 0) mapped = findDstByName("POT");
            } else if (sName == "RAIZ" || sName == "SQRT") {
                mapped = findDstByName("SQRT");
                if (mapped < 0) mapped = findDstByName("RAIZ");
            } else if (sName == "SD" || sName == "S<=>D") {
                mapped = findDstByName("SD");
                if (mapped < 0) mapped = findDstByName("S<=>D");
            } else if (sName == "NEG" || sName == "NEGATIVO" || sName == "(-)") {
                mapped = findDstByName("NEG");
                if (mapped < 0) mapped = findDstByName("NEGATIVO");
            } else if (sName == "DEL" || sName == "BACK") {
                mapped = findDstByName("DEL");
            } else if (sName == "AC" || sName == "AC/ON") {
                mapped = findDstByName("AC");
            }
        }
        if (mapped < 0) return false;
        outDstKeys.push_back(mapped);
    }
    return true;
}

LinearAndLatex ConvertKeyNamesToLinearAndLatex(const std::vector<std::string>& keyNames) {
    LinearAndLatex res{};
    std::ostringstream lin, tex;
    bool inShift = false;
    bool inAlpha = false;
    bool needCloseParen = false;

    auto closeFnIfNeeded = [&]() {
        if (needCloseParen) {
            lin << ")";
            tex << ")";
            needCloseParen = false;
        }
    };

    for (size_t i = 0; i < keyNames.size(); ++i) {
        const std::string& k = keyNames[i];
        if (k == "SHIFT") { inShift = true; inAlpha = false; continue; }
        if (k == "ALPHA") { inAlpha = true; inShift = false; continue; }

        if (k.size() == 1 && std::isdigit(static_cast<unsigned char>(k[0]))) {
            lin << k; tex << k;
        } else if (k == "PUNTO" || k == ".") {
            lin << "."; tex << ".";
        } else if (k == "MAS" || k == "+") {
            closeFnIfNeeded();
            lin << " + "; tex << " + ";
        } else if (k == "MENOS" || k == "-") {
            closeFnIfNeeded();
            lin << " - "; tex << " - ";
        } else if (k == "POR" || k == "*") {
            closeFnIfNeeded();
            lin << " * "; tex << " \\times ";
        } else if (k == "ENTRE" || k == "/") {
            closeFnIfNeeded();
            lin << " / "; tex << " \\div ";
        } else if (k == "IGUAL" || k == "EXE" || k == "=") {
            closeFnIfNeeded();
            lin << " = "; tex << " = ";
        } else if (k == "PAREN_A" || k == "(") {
            lin << "("; tex << "(";
        } else if (k == "PAREN_C" || k == ")") {
            lin << ")"; tex << ")";
        } else if (k == "SQRT" || k == "RAIZ") {
            lin << "sqrt("; tex << "\\sqrt{";
            // Peek simple digits for clean braces
            size_t j = i + 1;
            std::string inner;
            while (j < keyNames.size() && keyNames[j].size() == 1 &&
                   (std::isdigit(static_cast<unsigned char>(keyNames[j][0])) || keyNames[j] == "PUNTO")) {
                inner += keyNames[j];
                ++j;
            }
            if (!inner.empty()) {
                lin << inner << ")";
                tex << inner << "}";
                i = j - 1;
            } else {
                tex << "\\square}";
            }
        } else if (k == "X2" || k == "X^2") {
            lin << "^2"; tex << "^{2}";
        } else if (k == "POT" || k == "x^n") {
            lin << "^"; tex << "^";
        } else if (k == "FRAC" || k == "a/b") {
            lin << "/"; tex << "/";
        } else if (k == "SIN") {
            if (inShift) { lin << "arcsin("; tex << "\\sin^{-1}("; }
            else { lin << "sin("; tex << "\\sin("; }
            needCloseParen = true;
        } else if (k == "COS") {
            if (inShift) { lin << "arccos("; tex << "\\cos^{-1}("; }
            else { lin << "cos("; tex << "\\cos("; }
            needCloseParen = true;
        } else if (k == "TAN") {
            if (inShift) { lin << "arctan("; tex << "\\tan^{-1}("; }
            else { lin << "tan("; tex << "\\tan("; }
            needCloseParen = true;
        } else if (k == "LOG") {
            lin << "log("; tex << "\\log(";
            needCloseParen = true;
        } else if (k == "LN") {
            lin << "ln("; tex << "\\ln(";
            needCloseParen = true;
        } else if (k == "EXP") {
            if (inShift) { lin << "pi"; tex << "\\pi"; }
            else if (inAlpha) { lin << "e"; tex << "e"; }
            else { lin << "*10^"; tex << "\\times 10^"; }
        } else if (k == "ANS") {
            lin << "Ans"; tex << "\\mathrm{Ans}";
        } else if (k == "NEG" || k == "NEGATIVO" || k == "(-)") {
            lin << "-"; tex << "-";
        } else if (k == "X_THETA" || k == "X") {
            lin << "x"; tex << "x";
        } else if (k == "INTEGRAL") {
            if (inShift) { lin << "d/dx("; tex << "\\frac{d}{dx}("; }
            else { lin << "int("; tex << "\\int "; }
        } else if (k == "SD" || k == "S<=>D") {
            lin << " [S<=>D]"; tex << " \\Rightarrow ";
        } else if (k != "DERECHA" && k != "IZQUIERDA" && k != "ARRIBA" && k != "ABAJO" &&
                   k != "AC" && k != "DEL" && k != "OK") {
            lin << "[" << k << "]";
            tex << "\\mathrm{[" << k << "]}";
        }

        inShift = false;
        inAlpha = false;
    }

    closeFnIfNeeded();
    res.linearText = lin.str();
    std::string t = tex.str();
    res.latexDisplay = t.empty() ? "$0$" : ("$" + t + "$");
    return res;
}

} // namespace CasioKnowledgeLinux
