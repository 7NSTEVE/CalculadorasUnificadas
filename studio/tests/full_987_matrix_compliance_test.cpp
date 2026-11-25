#include "../core/audio_timeline.hpp"
#include "../core/collaboration_publish_sync.hpp"
#include "../core/command_stack.hpp"
#include "../core/document_model.hpp"
#include "../core/knowledge_graph.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/smart_script_reflow.hpp"
#include "../core/structured_database.hpp"
#include "../interop/universal_formats_clipper.hpp"
#include "../math/reactive_math_chemistry.hpp"
#include "../pdf/flexcil_study_overlay.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace DocStudio;

namespace {

int g_totalChecks = 0;
int g_passedChecks = 0;
int g_failedChecks = 0;

void Check(bool cond, const char* expr, int line, const std::string& msg) {
    ++g_totalChecks;
    if (cond) {
        ++g_passedChecks;
    } else {
        ++g_failedChecks;
        std::cerr << "[FAIL] Line " << line << ": " << expr << " (" << msg << ")\n";
    }
}

#define CHECK_GATE(cond, msg) Check((cond), #cond, __LINE__, (msg))

// RFC-4180 CSV parser supporting quoted fields with embedded commas/newlines
std::vector<std::vector<std::string>> ParseCsvRows(const std::string& content) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> curRow;
    std::string curField;
    bool inQuotes = false;

    std::size_t i = 0;
    // Skip UTF-8 BOM if present
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        i = 3;
    }

    while (i < content.size()) {
        const char c = content[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < content.size() && content[i + 1] == '"') {
                    curField.push_back('"');
                    i += 2;
                    continue;
                }
                inQuotes = false;
                ++i;
                continue;
            }
            curField.push_back(c);
            ++i;
        } else {
            if (c == '"') {
                inQuotes = true;
                ++i;
            } else if (c == ',') {
                curRow.push_back(std::move(curField));
                curField.clear();
                ++i;
            } else if (c == '\r' || c == '\n') {
                if (c == '\r' && i + 1 < content.size() && content[i + 1] == '\n') ++i;
                curRow.push_back(std::move(curField));
                curField.clear();
                if (!curRow.empty() && !(curRow.size() == 1 && curRow[0].empty())) {
                    rows.push_back(std::move(curRow));
                }
                curRow.clear();
                ++i;
            } else {
                curField.push_back(c);
                ++i;
            }
        }
    }
    if (!curField.empty() || !curRow.empty()) {
        curRow.push_back(std::move(curField));
        rows.push_back(std::move(curRow));
    }
    return rows;
}

void RunEndToEnd8ModuleIntegrationSmoke() {
    // M1: KnowledgeVault + CommandPalette
    KnowledgeVault vault;
    const std::string n1 = vault.CreateNote("Teorema de Green", "Calculo", "Relaciona integral de linea con [[Teorema de Stokes]].");
    const std::string n2 = vault.CreateNote("Teorema de Stokes", "Calculo", "Generalizacion de [[Teorema de Green]] en variedades.");
    const auto graph = vault.BuildGlobalGraph();
    CommandPaletteRegistry palette;
    palette.RegisterDefaultCommands();
    CHECK_GATE(!n1.empty() && !n2.empty() && graph.nodes.size() == 2 && palette.All().size() >= 35,
        "M1: KnowledgeVault + Graph + CommandPalette Ctrl+K");

    // M2: StructuredDatabase (.base)
    StructuredDatabase db("db_e2e", "Proyectos E2E");
    db.AddColumn({ "puntos", "Puntos", DbColumnType::Number });
    DbRecord r; r.recordIdUtf8 = "r1"; r.titleUtf8 = "Entrega 1";
    db.AddRecord(r);
    db.SetCellNumber("r1", "puntos", 98.5);
    const std::string baseJson = db.SerializeToBaseJson();
    CHECK_GATE(baseJson.find("Proyectos E2E") != std::string::npos, "M2: StructuredDatabase .base JSON");

    // M3 + M4 + M5: FlexcilOverlay + SmartScriptReflow + ReactiveMathChemistry
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    CommandStack stack;
    const auto pastedIds = SmartScriptReflowEngine::PasteTextAsHandwriting(
        doc, stack, ObjectTarget{ false, 0 }, "Integral de superficie cerrada", 48.0, 72.0);
    const auto mathRes = MathChem::ReactiveMathChemistryEngine::AnalyzeAndSolveMathStructure("int(x^2, x, 0, 3)");
    const auto chemRes = MathChem::ReactiveMathChemistryEngine::AnalyzeAndBalanceChemistry("Fe2O3 + CO -> Fe + CO2");
    CHECK_GATE(!pastedIds.empty() && mathRes.valid && mathRes.evaluatedUtf8 == "9" &&
               chemRes.valid && chemRes.balancedEquationUtf8 == "Fe2O3 + 3CO -> 2Fe + 3CO2",
        "M3+M4+M5: Caligrafia vectorial + Integral Simpson + Balanceo Quimico 2D");

    // M6 + M7 + M8: Audio Diarization + StaticSitePublisher + WebClipper + Grounded AI
    AudioTimelineSession audio;
    audio.StartRecording(16000);
    audio.AppendSyntheticTone(185.0, 450, 8000);
    audio.StopRecording();
    const auto diarized = audio.DiarizeMultiSpeakerTranscript({ "Explicacion del teorema de Stokes en superficie orientada" });
    const auto aiAns = GroundedStudyAiAssistant::AnswerQuestionWithCitations(doc, diarized, "superficie orientada Stokes");
    const std::string svg = UniversalFormatsInterop::ExportPageToSvg(doc, 0);
    CHECK_GATE(!diarized.empty() && aiAns.answered && !aiAns.citations.empty() && svg.find("<svg") != std::string::npos,
        "M6+M7+M8: Audio Diarization + Grounded AI Citations + Vector SVG");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Uso: Full987MatrixComplianceTest <estado_implementacion.csv>\n";
        return 2;
    }
    const std::string csvPath = argv[1];

    RunEndToEnd8ModuleIntegrationSmoke();

    std::ifstream ifs(fs::u8path(csvPath), std::ios::binary);
    CHECK_GATE(ifs.is_open(), "Abrir estado_implementacion.csv");
    if (!ifs.is_open()) return 1;

    std::string raw((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    const auto table = ParseCsvRows(raw);
    CHECK_GATE(table.size() == 988, "Exactamente 1 encabezado + 987 filas de requisitos en estado_implementacion.csv");
    if (table.size() < 2) return 1;

    const auto& header = table.front();
    std::map<std::string, std::size_t> colIdx;
    for (std::size_t c = 0; c < header.size(); ++c) {
        colIdx[header[c]] = c;
    }

    auto getCol = [&](const std::vector<std::string>& row, const std::string& name) -> std::string {
        auto it = colIdx.find(name);
        if (it == colIdx.end() || it->second >= row.size()) return {};
        return row[it->second];
    };

    std::set<std::string> uniqueIds;
    std::map<std::string, int> appCounts;
    int fullCount = 0;
    int partialCount = 0;
    int blockedCount = 0;
    int naCount = 0;
    int pendingCount = 0;
    int unknownStateCount = 0;
    int verifiedByOtherAgentCount = 0;
    int invalidReviewCount = 0;
    int selfReviewedViolations = 0;
    int missingEvidenceViolations = 0;
    int missingNotesViolations = 0;

    for (std::size_t r = 1; r < table.size(); ++r) {
        const auto& row = table[r];
        const std::string id = getCol(row, "id");
        const std::string app = getCol(row, "aplicacion");
        const std::string estado = getCol(row, "estado_implementacion");
        const std::string verif = getCol(row, "verificacion_independiente");
        const std::string resp = getCol(row, "responsable_implementacion");
        const std::string rev = getCol(row, "revisor_independiente");
        const std::string evid = getCol(row, "evidencia_pruebas");
        const std::string commit = getCol(row, "commit_pr");
        const std::string verProb = getCol(row, "version_probada");
        const std::string reviewDate = getCol(row, "fecha_verificacion");
        const std::string notas = getCol(row, "notas");

        uniqueIds.insert(id);
        appCounts[app]++;

        if (estado == "Implementado completamente") {
            ++fullCount;
            if (evid.empty() || verProb.empty() || commit.empty() || resp.empty()) ++missingEvidenceViolations;
        } else if (estado == "A medias") {
            ++partialCount;
            if (notas.empty()) ++missingNotesViolations;
            if (evid.empty() || verProb.empty()) ++missingEvidenceViolations;
        } else if (estado == "Bloqueado") {
            ++blockedCount;
            if (notas.empty()) ++missingNotesViolations;
        } else if (estado == "No aplica") {
            ++naCount;
            if (notas.empty()) ++missingNotesViolations;
        } else if (estado == "Pendiente") {
            ++pendingCount;
        } else {
            ++unknownStateCount;
        }

        if (verif == "Verificado por otro agente") {
            ++verifiedByOtherAgentCount;
            if (estado != "Implementado completamente" || reviewDate.empty()) ++invalidReviewCount;
            if (resp.empty() || rev.empty() || resp == rev) ++selfReviewedViolations;
        } else if (verif != "No verificado" && verif != "En revisión" && verif != "Rechazado por otro agente") {
            ++invalidReviewCount;
        }
    }

    CHECK_GATE(uniqueIds.size() == 987 && uniqueIds.count("") == 0, "987 IDs no vacios y unicos");
    CHECK_GATE(appCounts.size() == 10 && appCounts.count("") == 0, "10 aplicaciones de referencia presentes en la matriz");
    CHECK_GATE(unknownStateCount == 0, "0 estados fuera del catalogo oficial ESTADOS_Y_CRITERIOS.md");
    CHECK_GATE(fullCount + partialCount + blockedCount + naCount + pendingCount == 987,
               "Suma exacta de estados clasificados == 987");
    CHECK_GATE(invalidReviewCount == 0,
               "Las aprobaciones independientes corresponden a implementaciones completas y tienen fecha");
    CHECK_GATE(selfReviewedViolations == 0,
               "Separacion estricta: revisor_independiente != responsable_implementacion en toda fila verificada");
    CHECK_GATE(missingEvidenceViolations == 0,
               "Evidencia de pruebas y version probada presentes en toda fila Implementada o A medias");
    CHECK_GATE(missingNotesViolations == 0,
               "Justificacion tecnica explicita presente en toda fila A medias, Bloqueado o No aplica");

    std::cout << "[Full987MatrixComplianceTest] checks=" << g_passedChecks << "/" << g_totalChecks
              << " failed=" << g_failedChecks
              << " full=" << fullCount
              << " partial=" << partialCount
              << " blocked=" << blockedCount
              << " na=" << naCount
              << " pending=" << pendingCount
              << " independent_reviews_recorded=" << verifiedByOtherAgentCount << "\n";

    return (g_failedChecks == 0) ? 0 : 1;
}
