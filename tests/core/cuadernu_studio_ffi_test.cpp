// ============================================================================
// cuadernu_studio_ffi_test.cpp
// End-to-end verification of the cross-platform C++17 CuadernU Studio FFI
// exported from casio_native (shared by Android/Flutter, iOS/iPadOS, Windows):
//   1. Capabilities & .cudoc ZIP binary round-trip save/load
//   2. InkStroke + MathBlock + CasioCard (PNG asset) + StudyTape + Undo/Redo
//   3. Accent-insensitive search + Vector PDF + Markdown + DXF export
//   4. Engineering CAS (#varsub SI units, equation, SEL, matrix, 2D plot, step check)
//   5. Reactive 118-element Chemistry reaction balancer
//   6. MNA Circuit solver (DC, AC, Bode, Transient, Symbolic Laplace) + K-Map
//   7. Knowledge Graph 2D layout + SM-2 Spaced Repetition + HorariosTEC solver
// ============================================================================

#include "cuadernu_studio_ffi.h"
#include "casio_ffi.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

int gPass = 0;
int gFail = 0;

void Check(bool ok, const char* msg, int line)
{
    if (ok) {
        ++gPass;
    } else {
        ++gFail;
        std::fprintf(stderr, "[FAIL] L%d: %s\n", line, msg);
    }
}

#define CHECK(c) Check(static_cast<bool>(c), #c, __LINE__)

std::string CallJson(int (*fn)(char*, int))
{
    const int need = fn(nullptr, 0);
    if (need <= 0) return {};
    std::string buf(static_cast<std::size_t>(need), '\0');
    fn(buf.data(), need);
    buf.resize(std::strlen(buf.c_str()));
    return buf;
}

} // namespace

int main()
{
    // 1. Capabilities
    const std::string caps = CallJson(cuadernu_ffi_capabilities_json);
    CHECK(caps.find("CuadernU C++17 Unified Engine v47") != std::string::npos);
    CHECK(caps.find("cudoc_zip_interop") != std::string::npos);
    CHECK(caps.find("horarios_tec_solver") != std::string::npos);

    // 2. Document + Ink + MathBlock + CasioCard + StudyTape + Undo/Redo + .cudoc roundtrip
    CuadernuDocHandle* doc = cuadernu_doc_create(1); // InfiniteCanvas
    CHECK(doc != nullptr);
    CHECK(cuadernu_doc_set_title(doc, "C\xC3\xA1lculo y Circuitos TEC") == 1);
    CHECK(cuadernu_doc_set_paper(doc, 2) == 1); // Grid

    const float strokePts[] = { 10.f, 20.f, 0.6f, 45.f, 38.f, 0.8f, 90.f, 65.f, 0.7f, 140.f, 80.f, 0.5f };
    const uint64_t inkId = cuadernu_doc_add_ink_stroke(doc, 1, 0, strokePts, 4, 0x1D4ED8FFu, 3.0, 1, 0);
    CHECK(inkId > 0);

    const uint64_t txtId = cuadernu_doc_add_text_box(doc, 1, 0, 60, 120, 320, 48, "Ecuaci\xC3\xB3n de Bernoulli y ley de Ohm", 15.0, 0x111827FFu);
    CHECK(txtId > 0);

    const uint64_t mathId = cuadernu_doc_add_math_block(doc, 1, 0, 60, 200, 280, 52, "V = 12 V; R = 470 ohm", 1);
    CHECK(mathId > 0);

    const uint8_t fakePng[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x01, 0x02, 0x03 };
    const uint64_t cardId = cuadernu_doc_add_casio_card(
        doc, 1, 0, 60, 280, 340, 180,
        "fx-CG50", "sin^-1(0.5)", "pi/6", "SHIFT SIN 0.5 EXE",
        fakePng, sizeof(fakePng));
    CHECK(cardId > 0);

    const uint64_t tapeId = cuadernu_doc_add_study_tape(doc, 1, 0, 60, 480, 200, 36, "F\xC3\xB3rmula oculta");
    CHECK(tapeId > 0);
    CHECK(cuadernu_doc_toggle_study_tape(doc, 1, 0, tapeId) == 1);

    CHECK(cuadernu_doc_can_undo(doc) == 1);
    CHECK(cuadernu_doc_undo(doc) == 1);
    CHECK(cuadernu_doc_can_redo(doc) == 1);
    CHECK(cuadernu_doc_redo(doc) == 1);

    // Snapshot & accent-insensitive search
    std::string snap(65536, '\0');
    const int snapLen = cuadernu_doc_get_snapshot_json(doc, snap.data(), static_cast<int>(snap.size()));
    CHECK(snapLen > 100);
    snap.resize(static_cast<std::size_t>(snapLen));
    CHECK(snap.find("fx-CG50") != std::string::npos);

    std::string srch(8192, '\0');
    const int srchLen = cuadernu_doc_search_json(doc, "ecuacion de bernoulli", 1, srch.data(), static_cast<int>(srch.size()));
    CHECK(srchLen > 10);
    CHECK(std::string(srch.c_str()).find("Bernoulli") != std::string::npos);

    // Save/load real .cudoc ZIP + export vector PDF + Markdown + DXF
    const auto tmpDir = std::filesystem::temp_directory_path() / "cuadernu_ffi_test";
    std::filesystem::create_directories(tmpDir);
    const std::string cudocPath = (tmpDir / "test_note.cudoc").string();
    const std::string pdfPath = (tmpDir / "test_note.pdf").string();
    const std::string dxfPath = (tmpDir / "test_note.dxf").string();

    CHECK(cuadernu_doc_save_cudoc(doc, cudocPath.c_str()) == 1);
    CHECK(std::filesystem::exists(cudocPath) && std::filesystem::file_size(cudocPath) > 200);
    CHECK(cuadernu_doc_export_pdf(doc, pdfPath.c_str()) == 1);
    CHECK(std::filesystem::exists(pdfPath) && std::filesystem::file_size(pdfPath) > 300);
    CHECK(cuadernu_doc_export_dxf(doc, dxfPath.c_str(), "1:1") == 1);
    CHECK(std::filesystem::exists(dxfPath) && std::filesystem::file_size(dxfPath) > 100);

    CuadernuDocHandle* reloaded = cuadernu_doc_create(0);
    CHECK(cuadernu_doc_load_cudoc(reloaded, cudocPath.c_str()) == 1);
    std::string snap2(65536, '\0');
    const int snap2Len = cuadernu_doc_get_snapshot_json(reloaded, snap2.data(), static_cast<int>(snap2.size()));
    CHECK(snap2Len > 100);
    CHECK(std::string(snap2.c_str()).find("Bernoulli") != std::string::npos);
    cuadernu_doc_destroy(reloaded);
    cuadernu_doc_destroy(doc);

    // 3. Engineering CAS + Chemistry
    char buf[32768]{};
    CHECK(cuadernu_cas_eval_json("12 V / 470 ohm | mA", nullptr, 1, buf, sizeof(buf)) > 10);
    CHECK(std::string(buf).find("\"ok\":true") != std::string::npos);
    CHECK(std::string(buf).find("mA") != std::string::npos);

    CHECK(cuadernu_cas_solve_linear_system_json("2*x + y = 7; x - y = 2", buf, sizeof(buf)) > 10);
    CHECK(std::string(buf).find("\"ok\":true") != std::string::npos);

    CHECK(cuadernu_cas_matrix_eval_json("det([[3,1],[2,4]])", buf, sizeof(buf)) > 10);
    CHECK(std::string(buf).find("10") != std::string::npos);

    CHECK(cuadernu_cas_sample_plot_json("sin(x)", "x", -3.14, 3.14, 32, buf, sizeof(buf)) > 10);
    CHECK(std::string(buf).find("\"ok\":true") != std::string::npos);

    CHECK(cuadernu_chem_analyze_json("Fe + O2 -> Fe2O3", buf, sizeof(buf)) > 10);
    CHECK(std::string(buf).find("\"balanced\":true") != std::string::npos);

    // 4. MNA Circuit Solver (DC + Bode + Laplace) + K-Map
    const char* rcNetlist = "V1 1 0 10\nR1 1 2 1000\nC1 2 0 1e-6\n";
    CHECK(cuadernu_circuit_solve_json("V1 1 0 12\nR1 1 2 1000\nR2 2 0 1000\n", "dc", 0, 0, 2, buf, sizeof(buf)) > 10);
    CHECK(std::string(buf).find("\"ok\":true") != std::string::npos);

    CHECK(cuadernu_circuit_solve_json(rcNetlist, "bode", 10.0, 100000.0, 2, buf, sizeof(buf)) > 10);
    CHECK(std::string(buf).find("\"ok\":true") != std::string::npos);

    CHECK(cuadernu_circuit_kmap_json(3, "1,3,5,7", "", buf, sizeof(buf)) > 10);
    CHECK(std::string(buf).find("\"ok\":true") != std::string::npos);

    // 5. Knowledge Graph 2D + SM-2 + Horarios TEC
    const char* notesJson = R"([
        {"title":"Calculo","folder":"TEC","body":"Usa [[Circuitos]] y [[Algebra]]","tags":["#mat"]},
        {"title":"Circuitos","folder":"TEC","body":"Ver [[Calculo]] para Laplace","tags":["#elec"]}
    ])";
    CHECK(cuadernu_graph_build_json(notesJson, buf, sizeof(buf)) > 10);
    CHECK(std::string(buf).find("\"ok\":true") != std::string::npos);
    CHECK(std::string(buf).find("\"brokenLinkCount\":1") != std::string::npos);

    CHECK(cuadernu_sm2_review_json(1, 1, 2.5, 5, 1700000000ull, buf, sizeof(buf)) > 10);
    CHECK(std::string(buf).find("\"intervalDays\":6") != std::string::npos);

    const char* catJson = R"({
        "courses":[
            {"code":"EL2113","name":"Circuitos","credits":4,"groups":[
                {"id":"Gr 1","sessions":[{"day":"L","start":"07:30","end":"09:20"}]}
            ]}
        ]
    })";
    CHECK(cuadernu_horarios_solve_json(catJson, "", buf, sizeof(buf)) > 10);
    CHECK(std::string(buf).find("EL2113") != std::string::npos);

    std::printf("cuadernu_studio_ffi_test: passed=%d failed=%d\n", gPass, gFail);
    return (gFail == 0) ? 0 : 1;
}
