#pragma once
// ============================================================================
// cuadernu_studio_ffi.h
// Cross-platform pure C ABI exposing the unified C++17 CuadernU Studio engine
// (DocumentModel, .cudoc ZIP storage, CommandStack, InkEngine, EngineeringCAS,
// ReactiveMathChemistry, MnaSolver + K-Map + Laplace, KnowledgeVault + Graph,
// StructuredDatabase, SM-2 Flashcards, PDF/DOCX/PPTX/CSV/XLSX/DXF/Markdown
// interop, and HorariosTEC solver) to Android/Flutter (dart:ffi), iOS/iPadOS
// (Swift via Puente.h), Windows, and Linux.
//
// Zero C++ ABI across boundary; all string/JSON outputs follow the standard
// buffer-sizing convention:
//   - returns >= 0: UTF-8 bytes written (excluding NUL) if buffer sufficed,
//     or required buffer size (including NUL) if out_buf == NULL or out_size
//     was too small.
//   - returns < 0: invalid arguments or error.
// ============================================================================

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(CASIO_NATIVE_SHARED)
#define CUADERNU_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define CUADERNU_EXPORT __attribute__((visibility("default")))
#else
#define CUADERNU_EXPORT
#endif

typedef struct CuadernuDocHandle CuadernuDocHandle;

// Engine identity & supported portable subsystem flags (JSON)
CUADERNU_EXPORT int cuadernu_ffi_capabilities_json(char* out_json, int out_size);

// ============================================================================
// 1. Unified .cudoc Document + Canvas/Pages + Ink + Undo/Redo + Search + I/O
// ============================================================================
// mode: 0 = PaginatedPdf (Notebook Pages), 1 = InfiniteCanvas
CUADERNU_EXPORT CuadernuDocHandle* cuadernu_doc_create(int mode);
CUADERNU_EXPORT void cuadernu_doc_destroy(CuadernuDocHandle* doc);

CUADERNU_EXPORT int cuadernu_doc_set_title(CuadernuDocHandle* doc, const char* title_utf8);
CUADERNU_EXPORT int cuadernu_doc_set_mode(CuadernuDocHandle* doc, int mode);
CUADERNU_EXPORT int cuadernu_doc_set_paper(CuadernuDocHandle* doc, int paper_pattern);
CUADERNU_EXPORT int cuadernu_doc_add_page(CuadernuDocHandle* doc, int paper_pattern);
CUADERNU_EXPORT int cuadernu_doc_page_count(const CuadernuDocHandle* doc);

// xy_pressure_triples: [x0, y0, p0, x1, y1, p1, ...] (3 * point_count floats)
// rgba: packed 0xRRGGBBAA
CUADERNU_EXPORT uint64_t cuadernu_doc_add_ink_stroke(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    const float* xy_pressure_triples,
    int point_count,
    uint32_t rgba,
    double stroke_width,
    int pen_style,
    int is_highlighter);

CUADERNU_EXPORT uint64_t cuadernu_doc_add_text_box(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    double x, double y, double w, double h,
    const char* text_utf8,
    double font_size_pt,
    uint32_t rgba);

CUADERNU_EXPORT uint64_t cuadernu_doc_add_shape(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    int shape_kind,
    double x, double y, double w, double h,
    uint32_t stroke_rgba,
    uint32_t fill_rgba,
    double stroke_width);

CUADERNU_EXPORT uint64_t cuadernu_doc_add_math_block(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    double x, double y, double w, double h,
    const char* expr_utf8,
    int angle_degrees);

// Embeds a live calculator capture (PNG bytes + model + expression + result) as a CalcSessionCard
CUADERNU_EXPORT uint64_t cuadernu_doc_add_casio_card(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    double x, double y, double w, double h,
    const char* calc_model_utf8,
    const char* expr_utf8,
    const char* result_utf8,
    const char* key_seq_utf8,
    const uint8_t* png_bytes,
    size_t png_size);

CUADERNU_EXPORT uint64_t cuadernu_doc_add_sticky_note(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    double x, double y,
    const char* text_utf8,
    uint32_t fill_rgba);

CUADERNU_EXPORT uint64_t cuadernu_doc_add_study_tape(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    double x, double y, double w, double h,
    const char* label_utf8);

CUADERNU_EXPORT int cuadernu_doc_toggle_study_tape(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    uint64_t object_id);

CUADERNU_EXPORT int cuadernu_doc_erase_at(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    double x, double y, double radius);

CUADERNU_EXPORT int cuadernu_doc_remove_object(
    CuadernuDocHandle* doc,
    int on_canvas,
    int page_index,
    uint64_t object_id);

CUADERNU_EXPORT int cuadernu_doc_undo(CuadernuDocHandle* doc);
CUADERNU_EXPORT int cuadernu_doc_redo(CuadernuDocHandle* doc);
CUADERNU_EXPORT int cuadernu_doc_can_undo(const CuadernuDocHandle* doc);
CUADERNU_EXPORT int cuadernu_doc_can_redo(const CuadernuDocHandle* doc);

// Full JSON snapshot of document state (pages, canvasObjects, layers, undo flags)
CUADERNU_EXPORT int cuadernu_doc_get_snapshot_json(
    const CuadernuDocHandle* doc,
    char* out_json,
    int out_size);

// Retrieve embedded binary asset (e.g. PNG for CalcSessionCard / Image)
CUADERNU_EXPORT int64_t cuadernu_doc_get_asset_bytes(
    const CuadernuDocHandle* doc,
    const char* asset_key_utf8,
    uint8_t* out_buf,
    size_t out_size);

// Accent-insensitive full-text search across pages, OCR/PDF text, MathBlocks, and canvas cards
CUADERNU_EXPORT int cuadernu_doc_search_json(
    const CuadernuDocHandle* doc,
    const char* query_utf8,
    int accent_insensitive,
    char* out_json,
    int out_size);

// Evaluate all reactive '=' / MathBlock DAG formulas across the document
CUADERNU_EXPORT int cuadernu_doc_evaluate_math_dag_json(
    CuadernuDocHandle* doc,
    char* out_json,
    int out_size);

// Real binary-compatible .cudoc ZIP persistence (interoperable with Windows CuadernU.exe)
CUADERNU_EXPORT int cuadernu_doc_save_cudoc(CuadernuDocHandle* doc, const char* path_utf8);
CUADERNU_EXPORT int cuadernu_doc_load_cudoc(CuadernuDocHandle* doc, const char* path_utf8);

// Multi-format Export & Import (PDF vector, DOCX, PPTX, CSV, XLSX, DXF, Markdown)
CUADERNU_EXPORT int cuadernu_doc_export_pdf(const CuadernuDocHandle* doc, const char* path_utf8);
CUADERNU_EXPORT int cuadernu_doc_export_markdown(const CuadernuDocHandle* doc, char* out_md, int out_size);
CUADERNU_EXPORT int cuadernu_doc_export_dxf(const CuadernuDocHandle* doc, const char* path_utf8, const char* scale_ratio_utf8);
CUADERNU_EXPORT int cuadernu_doc_import_file_json(
    CuadernuDocHandle* doc,
    const char* path_utf8,
    int target_infinite_canvas,
    char* out_report_json,
    int out_size);

// Harvest SM-2 flashcards directly from the current document (StudyTape, MathBlocks, CalcCards, Q:/A:)
CUADERNU_EXPORT int cuadernu_doc_build_flashcards_json(
    const CuadernuDocHandle* doc,
    char* out_json,
    int out_size);

// ============================================================================
// 2. Engineering CAS (#varsub, SI/Imperial units, Phasors, Matrices, Plots)
//    & Reactive 118-Element Chemistry Balancer
// ============================================================================
CUADERNU_EXPORT int cuadernu_cas_eval_json(
    const char* expr_utf8,
    const char* env_json_utf8,
    int angle_degrees,
    char* out_json,
    int out_size);

CUADERNU_EXPORT int cuadernu_cas_solve_equation_json(
    const char* equation_utf8,
    const char* var_name_utf8,
    char* out_json,
    int out_size);

CUADERNU_EXPORT int cuadernu_cas_solve_linear_system_json(
    const char* equations_utf8,
    char* out_json,
    int out_size);

CUADERNU_EXPORT int cuadernu_cas_matrix_eval_json(
    const char* expr_utf8,
    char* out_json,
    int out_size);

CUADERNU_EXPORT int cuadernu_cas_sample_plot_json(
    const char* expr_utf8,
    const char* var_name_utf8,
    double x_min,
    double x_max,
    int num_samples,
    char* out_json,
    int out_size);

CUADERNU_EXPORT int cuadernu_cas_check_step_json(
    const char* prev_expr_utf8,
    const char* next_expr_utf8,
    const char* var_name_utf8,
    char* out_json,
    int out_size);

CUADERNU_EXPORT int cuadernu_chem_analyze_json(
    const char* formula_or_reaction_utf8,
    char* out_json,
    int out_size);

// ============================================================================
// 3. MNA Electronic Circuit Lab (DC / AC / Bode / Transient / Laplace / K-Map)
// ============================================================================
// mode_utf8: "dc", "ac", "bode", "transient", "laplace"
CUADERNU_EXPORT int cuadernu_circuit_solve_json(
    const char* netlist_utf8,
    const char* mode_utf8,
    double param1,
    double param2,
    int probe_node,
    char* out_json,
    int out_size);

CUADERNU_EXPORT int cuadernu_circuit_kmap_json(
    int num_vars,
    const char* minterms_csv_utf8,
    const char* dontcares_csv_utf8,
    char* out_json,
    int out_size);

// ============================================================================
// 4. Knowledge Graph ([[wikilinks]] + 2D Layout), SM-2 & Horarios TEC
// ============================================================================
// notes_json_utf8: [{"id":"...","title":"...","folder":"...","body":"...","tags":["..."]}]
CUADERNU_EXPORT int cuadernu_graph_build_json(
    const char* notes_json_utf8,
    char* out_json,
    int out_size);

CUADERNU_EXPORT int cuadernu_sm2_review_json(
    int repetitions,
    int interval_days,
    double ease_factor,
    int quality_0_to_5,
    uint64_t now_epoch_sec,
    char* out_json,
    int out_size);

CUADERNU_EXPORT int cuadernu_horarios_solve_json(
    const char* catalog_json_utf8,
    const char* busy_slots_json_utf8,
    char* out_json,
    int out_size);

#ifdef __cplusplus
}
#endif
