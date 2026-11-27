// Que no dañe ni mienta (revisión de código del 27-nov-2026).
//
// Prueba con trazos, documentos, render y PDF reales (nada de datos que el propio código
// fabrica para pasar) que:
//   1. Con los valores por defecto nada convierte ni borra la letra: zigzag de resistencia,
//      una «l» y un «0» se quedan como tinta.
//   2. «Mantener para ajustar» solo actúa con el lápiz quieto medio segundo, y tachar, cuando
//      se enciende, borra tinta pero nunca imágenes.
//   3. El resaltador da el color de una sola capa en pantalla y es una sola ruta en el PDF.
//   4. Lo experimental no aparece en Explorar ni en los menús, y la base salta sus vistas rotas.
//   5. Sin micrófono no se graba nada, y el extracto sin IA no se marca como verificado.

#include <windows.h>
#include <objbase.h>

#include "../app/studio_actions.hpp"
#include "../app/studio_funciones_experimentales.hpp"
#include "../core/audio_timeline.hpp"
#include "../core/command_stack.hpp"
#include "../core/document_model.hpp"
#include "../core/ink_engine.hpp"
#include "../core/knowledge_graph.hpp"
#include "../math/cas_ai_pedagogy.hpp"
#include "../pdf/pdf_writer.hpp"
#include "../render/d2d_renderer.hpp"

#include <zlib.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace DocStudio;

namespace {

int g_total = 0;
int g_failed = 0;

void Check(bool cond, const char* what) {
    ++g_total;
    if (!cond) {
        ++g_failed;
        std::cerr << "[FALLA] " << what << "\n";
    }
}

InkPoint P(double x, double y, std::uint64_t ms) { return { x, y, 0.7f, 0.0f, 0.0f, ms }; }

// Zigzag como el símbolo de una resistencia: 5 picos de 20 de alto, cada 5 de ancho
std::vector<InkPoint> Zigzag(double x0, double y0) {
    std::vector<InkPoint> pts;
    for (int i = 0; i <= 10; ++i) pts.push_back(P(x0 + i * 5.0, y0 + ((i % 2 == 0) ? 0.0 : 20.0), i * 12ull));
    return pts;
}

// Una «l»: trazo recto vertical
std::vector<InkPoint> LetraL(double x0, double y0) {
    std::vector<InkPoint> pts;
    for (int i = 0; i <= 30; ++i) pts.push_back(P(x0, y0 + i * 2.0, i * 10ull));
    return pts;
}

// Un «0»: elipse cerrada
std::vector<InkPoint> Cero(double cx, double cy) {
    std::vector<InkPoint> pts;
    for (int i = 0; i <= 40; ++i) {
        const double t = 6.283185307 * i / 40.0;
        pts.push_back(P(cx + 10.0 * std::cos(t), cy + 14.0 * std::sin(t), i * 10ull));
    }
    return pts;
}

AnnotationObject Draft(std::vector<InkPoint> pts) {
    AnnotationObject o;
    o.kind = ObjectKind::InkStroke;
    o.strokeWidth = 2.0;
    o.strokeColor = DocColor::RGBA(20, 20, 20, 255);
    o.inkPoints = std::move(pts);
    o.RecomputeBounds();
    return o;
}

const AnnotationObject* FindById(const DocumentModel& doc, std::uint64_t id) {
    for (const auto& o : doc.CanvasObjects()) {
        if (o.id == id) return &o;
    }
    return nullptr;
}

// Termina un trazo igual que la app al levantar el lápiz
void Commit(DocumentModel& doc, CommandStack& stack, AnnotationObject draft, bool snap, bool scribble) {
    std::vector<std::uint64_t> sel;
    std::uint64_t prim = 0;
    std::string status;
    AppActions::HandleStrokeCompletedAction(doc, stack, ObjectTarget{ true, 0 }, draft, false, snap, 0.65,
                                            sel, prim, status, scribble);
}

// El PDF comprime el contenido de cada página (FlateDecode): se descomprime cada stream para
// poder leer los operadores de dibujo.
std::string PdfContentText(const std::vector<std::uint8_t>& pdf) {
    const std::string s(pdf.begin(), pdf.end());
    std::string out;
    std::size_t pos = 0;
    while ((pos = s.find("stream", pos)) != std::string::npos) {
        if (pos >= 3 && s.compare(pos - 3, 3, "end") == 0) { pos += 6; continue; }
        std::size_t start = pos + 6;
        if (start < s.size() && s[start] == '\r') ++start;
        if (start < s.size() && s[start] == '\n') ++start;
        const std::size_t end = s.find("endstream", start);
        if (end == std::string::npos) break;
        z_stream zs{};
        std::string dec;
        int ret = Z_DATA_ERROR;
        if (inflateInit(&zs) == Z_OK) {
            zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(s.data() + start));
            zs.avail_in = static_cast<uInt>(end - start);
            std::vector<char> buf(1 << 16);
            do {
                zs.next_out = reinterpret_cast<Bytef*>(buf.data());
                zs.avail_out = static_cast<uInt>(buf.size());
                ret = inflate(&zs, Z_NO_FLUSH);
                dec.append(buf.data(), buf.size() - zs.avail_out);
            } while (ret == Z_OK);
            inflateEnd(&zs);
        }
        if (ret == Z_STREAM_END) out += dec; else out.append(s, start, end - start);
        out += "\n";
        pos = end + 9;
    }
    return out;
}

std::size_t CountOf(const std::string& s, const std::string& needle) {
    std::size_t n = 0;
    for (std::size_t pos = s.find(needle); pos != std::string::npos; pos = s.find(needle, pos + needle.size())) ++n;
    return n;
}

void PruebaLetraIntacta() {
    Check(!InkEngine::kSnapShapesDefault, "las formas automáticas empiezan apagadas");
    Check(!InkEngine::kScribbleEraseDefault, "tachar para borrar empieza apagado");
    Check(InkEngine::kHoldToSnapMs >= 400, "la pausa para ajustar es de al menos 0,4 s");
    Check(!InkEngine::ShouldSnapHeldShape(false, 5000), "con formas apagadas nunca se ajusta");
    Check(!InkEngine::ShouldSnapHeldShape(true, 120), "un trazo normal (sin pausa) no se ajusta");
    Check(InkEngine::ShouldSnapHeldShape(true, 600), "con pausa real sí se ajusta");

    // Sin esta revisión, estos trazos se habrían convertido o habrían borrado cosas
    Check(InkEngine::IsScribbleEraseGesture(Zigzag(100.0, 100.0)), "el zigzag de prueba parece un tachón");
    Check(InkEngine::RecognizeHeldShape(LetraL(300.0, 100.0)).recognized, "la «l» de prueba parece una línea");

    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::InfiniteCanvas);
    CommandStack stack;
    AnnotationObject img;
    img.kind = ObjectKind::Image;
    img.bounds = { 90.0, 90.0, 80.0, 50.0 };
    doc.AddCanvasObject(img);
    const std::uint64_t imgId = doc.CanvasObjects().back().id;
    doc.AddCanvasObject(Draft({ P(95.0, 110.0, 0), P(130.0, 110.0, 20), P(160.0, 110.0, 40) }));
    const std::uint64_t lineId = doc.CanvasObjects().back().id;

    const bool snapDefault = InkEngine::ShouldSnapHeldShape(InkEngine::kSnapShapesDefault, 120);
    const std::size_t before = doc.CanvasObjects().size();
    Commit(doc, stack, Draft(Zigzag(100.0, 100.0)), snapDefault, InkEngine::kScribbleEraseDefault);
    Commit(doc, stack, Draft(LetraL(300.0, 100.0)), snapDefault, InkEngine::kScribbleEraseDefault);
    Commit(doc, stack, Draft(Cero(400.0, 120.0)), snapDefault, InkEngine::kScribbleEraseDefault);
    Check(doc.CanvasObjects().size() == before + 3, "los tres trazos se agregan y no se borra nada");
    Check(FindById(doc, imgId) != nullptr && FindById(doc, lineId) != nullptr, "la imagen y la tinta de debajo siguen ahí");
    std::size_t inkCount = 0;
    for (std::size_t i = before; i < doc.CanvasObjects().size(); ++i) {
        if (doc.CanvasObjects()[i].kind == ObjectKind::InkStroke) ++inkCount;
    }
    Check(inkCount == 3, "zigzag, «l» y «0» se quedan como tinta");
}

void PruebaMantenerYTachar() {
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::InfiniteCanvas);
    CommandStack stack;
    Commit(doc, stack, Draft(LetraL(300.0, 100.0)), InkEngine::ShouldSnapHeldShape(true, 700), false);
    Check(!doc.CanvasObjects().empty() && doc.CanvasObjects().back().kind == ObjectKind::Shape,
          "con formas encendidas y pausa de 0,7 s la «l» sí se vuelve línea");

    AnnotationObject img;
    img.kind = ObjectKind::Image;
    img.bounds = { 90.0, 90.0, 80.0, 50.0 };
    doc.AddCanvasObject(img);
    const std::uint64_t imgId = doc.CanvasObjects().back().id;
    doc.AddCanvasObject(Draft({ P(95.0, 110.0, 0), P(130.0, 110.0, 20), P(160.0, 110.0, 40) }));
    const std::uint64_t lineId = doc.CanvasObjects().back().id;
    Commit(doc, stack, Draft(Zigzag(100.0, 100.0)), false, true);
    Check(FindById(doc, lineId) == nullptr, "con tachar encendido, la tinta tachada se borra");
    Check(FindById(doc, imgId) != nullptr, "con tachar encendido, la imagen no se borra");
}

void PruebaResaltador() {
    AnnotationObject hl;
    hl.kind = ObjectKind::InkStroke;
    hl.isHighlighter = true;
    hl.penStyle = InkPenStyle::Highlighter;
    hl.strokeWidth = 14.0;
    hl.strokeColor = DocColor::RGBA(245, 184, 10, 255);
    for (int i = 0; i <= 200; ++i) hl.inkPoints.push_back({ 100.0 + i, 100.0, 0.8f, 0.0f, 0.0f, static_cast<std::uint64_t>(i * 4) });
    hl.RecomputeBounds();

    // Pantalla: render real fuera de pantalla
    DocumentModel cd;
    cd.ResetToBlank(DocumentMode::InfiniteCanvas);
    cd.SetCanvasBackground(PaperPattern::Plain);
    cd.AddCanvasObject(hl);
    D2DRenderer d2d;
    RasterImageBgra32 img;
    const bool rendered = d2d.RenderInfiniteCanvasOffscreen(cd, { 0.0, 0.0, 400.0, 200.0 }, 400, 200, img);
    Check(rendered && img.Valid(), "el lienzo se dibuja fuera de pantalla");
    if (rendered && img.Valid()) {
        auto blue = [&](int x, int y) {
            return static_cast<int>(img.pixels[(static_cast<std::size_t>(y) * img.width + x) * 4]);
        };
        const int bg = blue(200, 180);
        const int mid = blue(200, 100);
        const double a = 98.0 / 255.0; // opacidad del resaltador
        const double expected = bg * (1.0 - a) + 10.0 * a;
        std::cerr << "  resaltador: fondo B=" << bg << ", centro B=" << mid << ", esperado ~" << expected << "\n";
        Check(std::abs(mid - expected) <= 16.0, "el centro del resaltador tiene el color de una sola capa");
    }

    // PDF: una sola ruta por trazo
    DocumentModel pd;
    pd.ResetToBlank(DocumentMode::PaginatedPdf);
    std::vector<std::uint8_t> base, withHl;
    Check(PdfWriter::ExportDocumentToBytes(pd, base), "se exporta el PDF sin resaltador");
    pd.AddPageObject(0, hl);
    Check(PdfWriter::ExportDocumentToBytes(pd, withHl), "se exporta el PDF con resaltador");
    const std::string baseText = PdfContentText(base);
    const std::string hlText = PdfContentText(withHl);
    const std::size_t extra = CountOf(hlText, " l S\n") - CountOf(baseText, " l S\n");
    std::cerr << "  resaltador en PDF: " << extra << " trazado(s) nuevos (antes eran 200)\n";
    Check(extra == 1, "el resaltador es una sola ruta en el PDF");
    Check(CountOf(hlText, "/GSAlpha gs") >= 1, "el resaltador lleva la transparencia del PDF");
}

void PruebaExperimental() {
    using namespace CmdIds;
    Check(Experimental::IsExperimentalCommand(CMD_HANDWRITING_SPELLING), "la ortografía manuscrita es experimental");
    Check(Experimental::IsExperimentalCommand(CMD_FLEXCIL_GESTURE_MODE), "el modo gestos Flexcil es experimental");
    Check(Experimental::IsExperimentalCommand(CMD_LAN_QR), "el QR de red local es experimental");
    Check(!Experimental::IsExperimentalCommand(CMD_TOOL_PEN), "el lápiz no es experimental");
    Check(!Experimental::IsExperimentalCommand(CMD_EXPORT_PDF), "exportar PDF no es experimental");
    Check(!Experimental::IsExperimentalCommand(CMD_TOGGLE_EXPERIMENTAL), "el interruptor de experimentales no se esconde a sí mismo");

    CommandPaletteRegistry pal;
    pal.RegisterDefaultCommands();
    std::size_t expBefore = 0;
    for (const auto& e : pal.All()) {
        if (Experimental::IsExperimentalCommand(e.commandId)) ++expBefore;
    }
    const std::size_t totalBefore = pal.All().size();
    Experimental::RemoveFromPalette(pal);
    bool anyLeft = false, hasToggle = false;
    for (const auto& e : pal.All()) {
        if (Experimental::IsExperimentalCommand(e.commandId)) anyLeft = true;
        if (e.commandId == CMD_TOGGLE_EXPERIMENTAL) hasToggle = true;
    }
    Check(expBefore > 0, "Explorar tenía comandos experimentales antes de filtrar");
    Check(!anyLeft && pal.All().size() == totalBefore - expBefore, "Explorar queda sin comandos experimentales");
    Check(hasToggle, "Explorar ofrece «Mostrar u ocultar funciones experimentales»");

    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, CMD_TOOL_PEN, L"Lápiz");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, CMD_HANDWRITING_SPELLING, L"Ortografía manuscrita");
    HMENU sub = CreatePopupMenu();
    AppendMenuW(sub, MF_STRING, CMD_LAN_QR, L"QR");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(sub), L"Red local");
    Experimental::PruneMenu(m);
    Check(GetMenuItemCount(m) == 1 && GetMenuItemID(m, 0) == CMD_TOOL_PEN,
          "el menú queda solo con lo no experimental, sin submenús vacíos ni separadores sobrantes");
    DestroyMenu(m);

    Check(Experimental::NextDbView(2, false) == 6, "la base salta Kanban, Calendario y Cronograma");
    Check(Experimental::NextDbView(2, true) == 3, "con experimentales visibles, la base muestra Kanban");
    Check(Experimental::NextDbView(8, false) == 0, "después de Mapa vuelve a Tabla");
}

void PruebaSinDatosFalsos() {
    AudioTimelineSession audio;
    audio.SetSimulateNoCaptureDevice(true);
    std::string status;
    Check(!audio.StartContinuousMicRecording(16000, &status), "sin micrófono no empieza a grabar");
    Check(!audio.IsContinuousRecording(), "sin micrófono no queda grabando");
    Check(status.find("micr") != std::string::npos, "el mensaje dice que no hay micrófono");
    Check(audio.DurationMs() == 0, "sin micrófono no hay audio inventado");

    DocumentModel sd;
    sd.ResetToBlank(DocumentMode::PaginatedPdf);
    AnnotationObject tb;
    tb.kind = ObjectKind::TextBox;
    tb.bounds = { 40.0, 40.0, 220.0, 40.0 };
    tb.textUtf8 = "Ley de Fourier: q = -k dT/dx";
    sd.AddPageObject(0, tb);
    const auto art = Cas::SummarizeDocumentNonDestructive(sd, 8);
    Check(art.ok, "el extracto toma la nota");
    Check(art.audit.verificationStatus == Cas::VerificationStatus::Unverified, "el extracto sin IA no se marca como verificado");
    Check(art.summaryBlock.textUtf8.rfind("Extracto sin IA", 0) == 0, "el extracto dice que no usa IA");
}

} // namespace

int main() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    PruebaLetraIntacta();
    PruebaMantenerYTachar();
    PruebaResaltador();
    PruebaExperimental();
    PruebaSinDatosFalsos();
    if (SUCCEEDED(hr)) CoUninitialize();
    std::cout << "[InkSafetyHonestyTest] checks=" << (g_total - g_failed) << "/" << g_total
              << " failed=" << g_failed << "\n";
    return g_failed == 0 ? 0 : 1;
}
