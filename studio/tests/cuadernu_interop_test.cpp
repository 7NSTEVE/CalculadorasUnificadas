#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../app/canvas_manipulation.hpp"
#include "../app/studio_cli_dispatch.hpp"
#include "../circuit/code_mcu_control.hpp"
#include "../core/audio_timeline.hpp"
#include "../core/command_stack.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "../core/image_processing.hpp"
#include "../core/ink_engine.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/zip_container.hpp"
#include "../interop/binary_parsers.hpp"
#include "../interop/notebook_migration_engine.hpp"
#include "../math/engineering_cas.hpp"
#include "../pdf/interop_openxml.hpp"
#include "../pdf/pdf_parser.hpp"
#include "../pdf/pdf_writer.hpp"
#include "../tinta_core/archivo.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace DocStudio;

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool cond, const char* expr, int line) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::cerr << "[FAIL] line " << line << ": " << expr << "\n";
    }
}

#define EXPECT(c) Check((c), #c, __LINE__)

void AppendVarint(std::vector<std::uint8_t>& out, std::uint64_t v) {
    do {
        std::uint8_t b = static_cast<std::uint8_t>(v & 0x7Fu);
        v >>= 7;
        if (v) b |= 0x80u;
        out.push_back(b);
    } while (v);
}

void AppendPbBytesField(std::vector<std::uint8_t>& out, std::uint32_t fieldNum, const std::vector<std::uint8_t>& payload) {
    AppendVarint(out, (static_cast<std::uint64_t>(fieldNum) << 3) | 2u);
    AppendVarint(out, payload.size());
    out.insert(out.end(), payload.begin(), payload.end());
}

void AppendPbPackedFloats(std::vector<std::uint8_t>& out, std::uint32_t fieldNum, const std::vector<float>& vals) {
    std::vector<std::uint8_t> raw(vals.size() * 4u);
    for (std::size_t i = 0; i < vals.size(); ++i) {
        std::memcpy(raw.data() + i * 4u, &vals[i], 4u);
    }
    AppendPbBytesField(out, fieldNum, raw);
}

std::vector<std::uint8_t> MakeTinyValidPng() {
    // 1x1 blue PNG with valid IHDR + IDAT + IEND
    static const std::uint8_t kPng[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53,
        0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41,
        0x54, 0x08, 0xD7, 0x63, 0x60, 0x60, 0xF8, 0x0F,
        0x00, 0x01, 0x04, 0x01, 0x00, 0x18, 0xDD, 0x8D,
        0xB4, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E,
        0x44, 0xAE, 0x42, 0x60, 0x82
    };
    return std::vector<std::uint8_t>(std::begin(kPng), std::end(kPng));
}

std::vector<std::uint8_t> MakeTinyValidWav() {
    AudioTimelineSession s;
    s.StartRecording(16000);
    std::vector<std::int16_t> tone(1600, 0);
    for (std::size_t i = 0; i < tone.size(); ++i) {
        tone[i] = static_cast<std::int16_t>(std::sin(static_cast<double>(i) * 0.12) * 8000.0);
    }
    s.AppendSamples(tone.data(), tone.size());
    s.StopRecording();
    std::vector<std::uint8_t> wav;
    s.ExportWavBytes(wav);
    return wav;
}

// Build a genuine iPadOS bplist00 binary property list for Notability testing
std::vector<std::uint8_t> BuildNotabilityBplistFixture() {
    // Object table layout (1-byte offsets, 1-byte refs):
    // 0: root dict { 1("$objects") -> 2(array) }
    // 1: ASCII "$objects"
    // 2: array [ 3(dict curves), 12("Apunte de Notability") ]
    // 3: dict { 4("curvespoints")->5(data), 6("curvesnumpoints")->7(data), 8("curveswidths")->9(data), 10("curvescolors")->11(data) }
    // 4..11: keys & raw binary arrays
    // 12: ASCII "Apunte de Notability"
    std::vector<std::uint8_t> buf = { 'b','p','l','i','s','t','0','0' };
    std::vector<std::uint32_t> offsets(13, 0);

    auto pushAscii = [&](std::size_t idx, const std::string& s) {
        offsets[idx] = static_cast<std::uint32_t>(buf.size());
        if (s.size() < 15) {
            buf.push_back(static_cast<std::uint8_t>(0x50u | s.size()));
        } else {
            buf.push_back(0x5Fu);
            buf.push_back(0x10u);
            buf.push_back(static_cast<std::uint8_t>(s.size()));
        }
        buf.insert(buf.end(), s.begin(), s.end());
    };
    auto pushData = [&](std::size_t idx, const std::vector<std::uint8_t>& d) {
        offsets[idx] = static_cast<std::uint32_t>(buf.size());
        if (d.size() < 15) {
            buf.push_back(static_cast<std::uint8_t>(0x40u | d.size()));
        } else {
            buf.push_back(0x4Fu);
            buf.push_back(0x10u);
            buf.push_back(static_cast<std::uint8_t>(d.size()));
        }
        buf.insert(buf.end(), d.begin(), d.end());
    };

    // Obj 0: root dict (1 entry: 1 -> 2)
    offsets[0] = static_cast<std::uint32_t>(buf.size());
    buf.push_back(0xD1u); buf.push_back(1); buf.push_back(2);

    pushAscii(1, "$objects");

    // Obj 2: array [3, 12]
    offsets[2] = static_cast<std::uint32_t>(buf.size());
    buf.push_back(0xA2u); buf.push_back(3); buf.push_back(12);

    // Obj 3: dict (4 entries: 4->5, 6->7, 8->9, 10->11)
    offsets[3] = static_cast<std::uint32_t>(buf.size());
    buf.push_back(0xD4u);
    buf.push_back(4); buf.push_back(6); buf.push_back(8); buf.push_back(10);
    buf.push_back(5); buf.push_back(7); buf.push_back(9); buf.push_back(11);

    pushAscii(4, "curvespoints");
    std::vector<std::uint8_t> ptsRaw;
    const float coords[6] = { 40.0f, 50.0f, 120.0f, 80.0f, 200.0f, 60.0f };
    ptsRaw.resize(sizeof(coords));
    std::memcpy(ptsRaw.data(), coords, sizeof(coords));
    pushData(5, ptsRaw);

    pushAscii(6, "curvesnumpoints");
    std::uint32_t nPts = 3;
    std::vector<std::uint8_t> npRaw(4);
    std::memcpy(npRaw.data(), &nPts, 4);
    pushData(7, npRaw);

    pushAscii(8, "curveswidths");
    float wVal = 3.0f;
    std::vector<std::uint8_t> wRaw(4);
    std::memcpy(wRaw.data(), &wVal, 4);
    pushData(9, wRaw);

    pushAscii(10, "curvescolors");
    std::vector<std::uint8_t> colRaw = { 25, 90, 210, 255 };
    pushData(11, colRaw);

    pushAscii(12, "Apunte de Notability");

    // Offset table (2-byte BE offsets so any size < 65535 works)
    const std::uint64_t offTableOffset = buf.size();
    for (std::uint32_t o : offsets) {
        buf.push_back(static_cast<std::uint8_t>((o >> 8) & 0xFFu));
        buf.push_back(static_cast<std::uint8_t>(o & 0xFFu));
    }

    // 32-byte trailer
    for (int i = 0; i < 6; ++i) buf.push_back(0);
    buf.push_back(2); // offsetIntSize = 2
    buf.push_back(1); // objectRefSize = 1
    auto pushBe64 = [&](std::uint64_t v) {
        for (int s = 56; s >= 0; s -= 8) buf.push_back(static_cast<std::uint8_t>((v >> s) & 0xFFu));
    };
    pushBe64(13);             // numObjects
    pushBe64(0);              // topObject
    pushBe64(offTableOffset); // offsetTableOffset
    return buf;
}

// Build a genuine uncompressed Microsoft MSCF CAB archive (.onepkg) with 2 files
std::vector<std::uint8_t> BuildMicrosoftCabFixture(
    const std::vector<std::pair<std::string, std::vector<std::uint8_t>>>& files)
{
    std::vector<std::uint8_t> concat;
    for (const auto& kv : files) {
        concat.insert(concat.end(), kv.second.begin(), kv.second.end());
    }

    // Compute CFFILE table size
    std::size_t cfFilesBytes = 0;
    for (const auto& kv : files) cfFilesBytes += 16u + kv.first.size() + 1u;

    const std::uint32_t coffFiles = 36u + 8u; // CFHEADER(36) + 1 CFFOLDER(8)
    const std::uint32_t coffCabStart = static_cast<std::uint32_t>(coffFiles + cfFilesBytes);
    const std::uint32_t totalSize = static_cast<std::uint32_t>(coffCabStart + 8u + concat.size());

    std::vector<std::uint8_t> cab;
    auto pushU16 = [&](std::uint16_t v) { cab.push_back(v & 0xFFu); cab.push_back((v >> 8) & 0xFFu); };
    auto pushU32 = [&](std::uint32_t v) {
        cab.push_back(v & 0xFFu); cab.push_back((v >> 8) & 0xFFu);
        cab.push_back((v >> 16) & 0xFFu); cab.push_back((v >> 24) & 0xFFu);
    };

    // CFHEADER (36 bytes)
    cab.insert(cab.end(), { 'M','S','C','F' });
    pushU32(0); // reserved1
    pushU32(totalSize);
    pushU32(0); // reserved2
    pushU32(coffFiles);
    pushU32(0); // reserved3
    cab.push_back(3); cab.push_back(1); // version 1.3
    pushU16(1); // cFolders
    pushU16(static_cast<std::uint16_t>(files.size())); // cFiles
    pushU16(0); // flags
    pushU16(1); // setID
    pushU16(0); // iCabinet

    // CFFOLDER (8 bytes)
    pushU32(coffCabStart);
    pushU16(1); // cCFData
    pushU16(0); // typeCompress = NONE

    // CFFILE entries
    std::uint32_t uoff = 0;
    for (const auto& kv : files) {
        pushU32(static_cast<std::uint32_t>(kv.second.size()));
        pushU32(uoff);
        pushU16(0); // iFolder
        pushU16(0); // date
        pushU16(0); // time
        pushU16(0); // attribs
        cab.insert(cab.end(), kv.first.begin(), kv.first.end());
        cab.push_back(0);
        uoff += static_cast<std::uint32_t>(kv.second.size());
    }

    // CFDATA (8 bytes + uncompressed bytes)
    pushU32(0); // csum
    pushU16(static_cast<std::uint16_t>(concat.size())); // cbData
    pushU16(static_cast<std::uint16_t>(concat.size())); // cbUncomp
    cab.insert(cab.end(), concat.begin(), concat.end());
    return cab;
}

// Build a synthetic OneNote .one section containing UTF-16LE text + InkML + PNG + WAV
std::vector<std::uint8_t> BuildOneNoteSectionFixture(const std::string& asciiHeading) {
    std::vector<std::uint8_t> raw;
    // OneNote section header marker
    const std::uint8_t hdr[16] = { 0xE4,0x52,0x5C,0x7B,0x8C,0xD8,0xA7,0x4D,0xAE,0xB1,0x53,0x78,0xD0,0x29,0x96,0xD3 };
    raw.insert(raw.end(), std::begin(hdr), std::end(hdr));
    // UTF-16LE text run
    for (char ch : asciiHeading) {
        raw.push_back(static_cast<std::uint8_t>(ch));
        raw.push_back(0x00);
    }
    raw.push_back(0); raw.push_back(0);
    // InkML trace
    const std::string inkml = "<trace>25 40, 85 95, 165 65, 210 110</trace>";
    raw.insert(raw.end(), inkml.begin(), inkml.end());
    // Embedded PNG
    const auto png = MakeTinyValidPng();
    raw.insert(raw.end(), png.begin(), png.end());
    // Embedded WAV
    const auto wav = MakeTinyValidWav();
    raw.insert(raw.end(), wav.begin(), wav.end());
    return raw;
}

void TestGoodnotesMigration() {
    ZipArchive zip;
    // Embedded PDF
    DocumentModel bgDoc;
    bgDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    std::vector<std::uint8_t> bgPdf;
    EXPECT(PdfWriter::ExportDocumentToBytes(bgDoc, bgPdf));
    zip.SetEntry("attachments/fondo.pdf", bgPdf);

    // Embedded protobuf stroke message (nested inside wrapper field 5)
    std::vector<std::uint8_t> strokeMsg;
    AppendPbPackedFloats(strokeMsg, 1, { 30.0f, 80.0f, 150.0f, 210.0f });
    AppendPbPackedFloats(strokeMsg, 2, { 45.0f, 90.0f, 70.0f, 125.0f });
    AppendPbPackedFloats(strokeMsg, 3, { 0.4f, 0.7f, 0.8f, 0.5f });
    std::vector<std::uint8_t> notePb;
    AppendPbBytesField(notePb, 5, strokeMsg);
    zip.SetEntry("notes/page_001.pb", notePb);

    // Embedded PNG + WAV
    zip.SetEntry("attachments/diagrama.png", MakeTinyValidPng());
    zip.SetEntry("attachments/clase.wav", MakeTinyValidWav());

    std::vector<std::uint8_t> gnBytes;
    EXPECT(zip.Serialize(gnBytes, true));

    const auto pre = Interop::NotebookMigrationEngine::AnalyzeCompatibilityBytes(gnBytes.data(), gnBytes.size(), "calculo.goodnotes");
    EXPECT(pre.format == Interop::SourceFormat::Goodnotes6);
    EXPECT(pre.editableTier == Interop::CompatLevelStatus::Full);
    EXPECT(pre.editableStrokeCount >= 1);

    DocumentModel outDoc;
    Interop::MigrationResult res{};
    EXPECT(Interop::NotebookMigrationEngine::ImportAndMigrateBytes(gnBytes.data(), gnBytes.size(), "calculo.goodnotes", outDoc, res));
    EXPECT(res.ok);
    EXPECT(res.report.hasBackgroundPdf);
    EXPECT(res.report.editableStrokeCount >= 1);
    EXPECT(res.report.imageCount >= 1);
    EXPECT(res.report.audioClipCount >= 1);
    EXPECT(!res.primaryAudioWavBytes.empty());
    EXPECT(!outDoc.Pages().empty());

    bool foundInk = false;
    bool foundImg = false;
    for (const auto& pg : outDoc.Pages()) {
        for (const auto& o : pg.objects) {
            if (o.kind == ObjectKind::InkStroke && o.inkPoints.size() == 4) foundInk = true;
            if (o.kind == ObjectKind::Image) foundImg = true;
        }
    }
    EXPECT(foundInk);
    EXPECT(foundImg);
}

void TestNotabilityMigration() {
    ZipArchive zip;
    zip.SetEntry("Note/Session.plist", BuildNotabilityBplistFixture());
    zip.SetEntry("Note/Recordings/voice1.wav", MakeTinyValidWav());
    std::vector<std::uint8_t> noteBytes;
    EXPECT(zip.Serialize(noteBytes, true));

    DocumentModel outDoc;
    Interop::MigrationResult res{};
    EXPECT(Interop::NotebookMigrationEngine::ImportAndMigrateBytes(noteBytes.data(), noteBytes.size(), "fisica.note", outDoc, res));
    EXPECT(res.ok);
    EXPECT(res.report.format == Interop::SourceFormat::Notability);
    EXPECT(res.report.editableStrokeCount >= 1);
    EXPECT(res.report.textBoxCount >= 1);
    EXPECT(res.report.audioClipCount >= 1);
    EXPECT(!res.primaryAudioWavBytes.empty());

    bool matchedCoord = false;
    for (const auto& o : outDoc.Pages()[0].objects) {
        if (o.kind == ObjectKind::InkStroke && o.inkPoints.size() == 3 && std::abs(o.inkPoints[1].x - 120.0) < 0.5) {
            matchedCoord = true;
        }
    }
    EXPECT(matchedCoord);
}

void TestOneNoteSectionAndPackageMigration() {
    const auto sec1 = BuildOneNoteSectionFixture("Circuitos Electricos II Analisis Nodal");
    const auto sec2 = BuildOneNoteSectionFixture("Transformada de Laplace y Polos");

    // 1. Standalone .one section
    DocumentModel docOne;
    Interop::MigrationResult resOne{};
    EXPECT(Interop::NotebookMigrationEngine::ImportAndMigrateBytes(sec1.data(), sec1.size(), "circuitos.one", docOne, resOne));
    EXPECT(resOne.ok);
    EXPECT(resOne.report.format == Interop::SourceFormat::OneNoteSection);
    EXPECT(resOne.report.editableStrokeCount >= 1);
    EXPECT(resOne.report.textBoxCount >= 1);
    EXPECT(resOne.report.imageCount >= 1);
    EXPECT(!resOne.primaryAudioWavBytes.empty());

    // 2. Multi-section .onepkg CAB archive
    const auto cabBytes = BuildMicrosoftCabFixture({
        { "Circuitos.one", sec1 },
        { "Laplace.one", sec2 }
    });
    DocumentModel docPkg;
    Interop::MigrationResult resPkg{};
    EXPECT(Interop::NotebookMigrationEngine::ImportAndMigrateBytes(cabBytes.data(), cabBytes.size(), "semestre.onepkg", docPkg, resPkg));
    EXPECT(resPkg.ok);
    EXPECT(resPkg.report.format == Interop::SourceFormat::OneNotePackage);
    EXPECT(docPkg.Pages().size() >= 2);
    EXPECT(resPkg.notebookSectionsUtf8.size() == 2);
    EXPECT(resPkg.report.editableStrokeCount >= 2);
}

void TestTintaLegacyMigration() {
    tinta::Documento tdoc{};
    tdoc.id = "cuaderno-uuid-1234";
    tdoc.titulo = "Fisica Clasica";

    tinta::Cuaderno cuad{};
    cuad.titulo = "Fisica Clasica";
    tinta::Seccion sec{};
    sec.titulo = "Mecanica";
    tinta::Pagina pg{};
    pg.titulo = "Pagina 1";
    pg.ancho = 612.0f;
    pg.alto = 792.0f;
    pg.papel.pauta = tinta::Pauta::Rayado;

    tinta::Trazo tr{};
    tr.id_colaboracion = "t1";
    tr.color_argb = 0xFF1D4ED8u;
    tr.ancho_base = 2.5f;
    tinta::Muestra m1{}; m1.posicion = { 40.0f, 50.0f }; m1.presion = 0.5f;
    tinta::Muestra m2{}; m2.posicion = { 90.0f, 80.0f }; m2.presion = 0.8f;
    tinta::Muestra m3{}; m3.posicion = { 160.0f, 70.0f }; m3.presion = 0.6f;
    tr.muestras = { m1, m2, m3 };
    pg.trazos.push_back(tr);

    tinta::CuadroTexto txt{};
    txt.id = "tx1";
    txt.marco = { 50.0f, 140.0f, 260.0f, 40.0f };
    txt.texto = "Segunda Ley de Newton: F = m * a";
    pg.textos.push_back(txt);

    sec.paginas.push_back(pg);
    cuad.secciones.push_back(sec);
    tdoc.cuadernos.push_back(cuad);

    const std::vector<std::uint8_t> bytes = tinta::Serializar(tdoc);
    EXPECT(!bytes.empty());

    DocumentModel outDoc;
    Interop::MigrationResult res{};
    EXPECT(Interop::NotebookMigrationEngine::ImportAndMigrateBytes(
        bytes.data(), bytes.size(), "fisica.tinta", outDoc, res));
    EXPECT(res.ok);
    EXPECT(res.report.format == Interop::SourceFormat::CuadernuTinta);
    EXPECT(outDoc.DocumentUuid() == "cuaderno-uuid-1234");
    EXPECT(outDoc.Pages().size() == 1);
    EXPECT(outDoc.Pages()[0].background == PaperPattern::Ruled);
    EXPECT(res.report.editableStrokeCount == 1);
    EXPECT(res.report.textBoxCount == 1);
}

void TestDocxAndPptxHighFidelityRoundTrip() {
    DocumentModel src;
    src.ResetToBlank(DocumentMode::PaginatedPdf);
    src.SetTitle("Informe Ingenieria");

    AnnotationObject tb{};
    tb.kind = ObjectKind::TextBox;
    tb.bounds = { 50.0, 40.0, 340.0, 48.0 };
    tb.textUtf8 = "Introduccion al Analisis de Senales";
    tb.fontSizePt = 16.0;
    tb.bold = true;
    src.AddPageObject(0, tb);

    AnnotationObject mb{};
    mb.kind = ObjectKind::MathBlock;
    mb.bounds = { 50.0, 110.0, 320.0, 68.0 };
    mb.formulaExprUtf8 = "(x + 1) / 2";
    mb.evaluatedResultUtf8 = "0.5*x + 0.5";
    src.AddPageObject(0, mb);

    AnnotationObject tbl{};
    tbl.kind = ObjectKind::TableBlock;
    tbl.bounds = { 50.0, 200.0, 360.0, 110.0 };
    tbl.tableJsonUtf8 = R"json({"rows":[["Param","Valor"],["R","1000"],["V","12"]]})json";
    src.AddPageObject(0, tbl);

    AnnotationObject ink{};
    ink.kind = ObjectKind::InkStroke;
    ink.strokeColor = DocColor::RGBA(26, 115, 232, 255);
    ink.strokeWidth = 2.4;
    ink.inkPoints = { { 60.0, 340.0, 0.6f, 0, 0, 0 }, { 140.0, 380.0, 0.7f, 0, 0, 10 }, { 240.0, 350.0, 0.6f, 0, 0, 20 } };
    ink.RecomputeBounds();
    src.AddPageObject(0, ink);

    // 1. DOCX round-trip
    std::vector<std::uint8_t> docxBytes;
    EXPECT(OpenXmlInterop::ExportToDocxBytes(src, { MakeTinyValidPng() }, docxBytes));
    DocumentModel docxIn;
    Interop::MigrationResult docxRes{};
    EXPECT(Interop::NotebookMigrationEngine::ImportAndMigrateBytes(docxBytes.data(), docxBytes.size(), "informe.docx", docxIn, docxRes));
    EXPECT(docxRes.ok);
    EXPECT(docxRes.report.format == Interop::SourceFormat::WordDocx);
    EXPECT(docxRes.report.tableCount >= 1);
    EXPECT(docxRes.report.mathBlockCount >= 1);

    // 2. PPTX round-trip including vector <a:custGeom> editable InkStroke!
    std::vector<std::uint8_t> pptxBytes;
    EXPECT(OpenXmlInterop::ExportToPptxBytes(src, { MakeTinyValidPng() }, pptxBytes));
    DocumentModel pptxIn;
    Interop::MigrationResult pptxRes{};
    EXPECT(Interop::NotebookMigrationEngine::ImportAndMigrateBytes(pptxBytes.data(), pptxBytes.size(), "diapos.pptx", pptxIn, pptxRes));
    EXPECT(pptxRes.ok);
    EXPECT(pptxRes.report.format == Interop::SourceFormat::PowerPointPptx);
    EXPECT(pptxRes.report.editableStrokeCount >= 1);
    EXPECT(pptxRes.report.tableCount >= 1);
    EXPECT(pptxRes.report.mathBlockCount >= 1);
}

void TestNegativeCorruptInputs() {
    DocumentModel d;
    Interop::MigrationResult r{};
    // 1. Empty buffer
    EXPECT(!Interop::NotebookMigrationEngine::ImportAndMigrateBytes(nullptr, 0, "empty.goodnotes", d, r));
    // 2. Unknown extension
    const std::uint8_t garbage[8] = { 1,2,3,4,5,6,7,8 };
    EXPECT(!Interop::NotebookMigrationEngine::ImportAndMigrateBytes(garbage, sizeof(garbage), "foo.xyz", d, r));
    // 3. Corrupt .goodnotes ZIP
    EXPECT(!Interop::NotebookMigrationEngine::ImportAndMigrateBytes(garbage, sizeof(garbage), "bad.goodnotes", d, r));
    // 4. Corrupt .tinta magic
    EXPECT(!Interop::NotebookMigrationEngine::ImportAndMigrateBytes(garbage, sizeof(garbage), "bad.tinta", d, r));
    // 5. Corrupt MSCF CAB with bogus folder count
    std::vector<std::uint8_t> badCab(40, 0);
    badCab[0] = 'M'; badCab[1] = 'S'; badCab[2] = 'C'; badCab[3] = 'F';
    badCab[26] = 0xFF; badCab[27] = 0xFF;
    std::vector<Interop::Binary::CabFileEntry> cabOut;
    EXPECT(!Interop::Binary::ExtractCabArchive(badCab.data(), badCab.size(), cabOut));
    // 6. Corrupt bplist00 with out-of-bounds offset table
    std::vector<std::uint8_t> badPlist(48, 0xFF);
    std::memcpy(badPlist.data(), "bplist00", 8);
    Interop::Binary::PlistValue pv{};
    EXPECT(!Interop::Binary::ParseBinaryPlist(badPlist.data(), badPlist.size(), pv));
}

void TestTrackAToDCoreEngines() {
    // 1. Partial/Pixel Eraser + Undo/Redo
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::InfiniteCanvas);
    CommandStack stack;
    AnnotationObject st{};
    st.kind = ObjectKind::InkStroke;
    st.strokeWidth = 2.5;
    for (int i = 0; i <= 20; ++i) {
        st.inkPoints.push_back({ static_cast<double>(i) * 10.0, 50.0, 0.7f, 0, 0, static_cast<std::uint64_t>(i * 5) });
    }
    st.RecomputeBounds();
    EXPECT(stack.Execute(doc, std::make_unique<AddObjectCommand>(ObjectTarget{ true, 0 }, st)));
    EXPECT(doc.CanvasObjects().size() == 1);
    const AnnotationObject origStroke = doc.CanvasObjects()[0];

    std::vector<AnnotationObject> splits;
    EXPECT(InkEngine::EraseStrokePartial(origStroke, { 100.0, 50.0 }, 20.0, splits));
    EXPECT(splits.size() == 2);
    EXPECT(stack.Execute(doc, std::make_unique<ReplaceStrokeWithSplitsCommand>(ObjectTarget{ true, 0 }, origStroke, splits)));
    EXPECT(doc.CanvasObjects().size() == 2);
    EXPECT(stack.Undo(doc));
    EXPECT(doc.CanvasObjects().size() == 1);
    EXPECT(stack.Redo(doc));
    EXPECT(doc.CanvasObjects().size() == 2);

    // 2. Snap-to-grid + Protractor + Compass + Magnetic connectors
    const DocPoint sn = InkEngine::SnapPointToGrid({ 23.0, 47.0 }, PaperPattern::Grid, 24.0);
    EXPECT(std::abs(sn.x - 24.0) < 0.01 && std::abs(sn.y - 48.0) < 0.01);

    RulerState comp{};
    comp.visible = true;
    comp.mode = RulerMode::Compass;
    comp.centerX = 100.0;
    comp.centerY = 100.0;
    comp.compassRadiusPt = 50.0;
    // Near compass arc (dist=56 vs radius=50) snaps cleanly onto r=50 circle at (150,100)
    bool compassSnapped = false;
    const DocPoint onCircle = InkEngine::ProjectPointOntoRuler({ 156.0, 100.0 }, comp, 22.0, &compassSnapped);
    EXPECT(compassSnapped && std::abs(onCircle.x - 150.0) < 0.1 && std::abs(onCircle.y - 100.0) < 0.1);
    // Far from compass arc stays unsnapped
    bool farSnapped = false;
    const DocPoint farPt = InkEngine::ProjectPointOntoRuler({ 300.0, 100.0 }, comp, 22.0, &farSnapped);
    EXPECT(!farSnapped && std::abs(farPt.x - 300.0) < 0.1);

    // Magnetic connector tracking
    AnnotationObject boxA{}, boxB{}, conn{};
    boxA.kind = ObjectKind::TextBox; boxA.bounds = { 10.0, 10.0, 80.0, 40.0 }; boxA.textUtf8 = "A";
    boxB.kind = ObjectKind::TextBox; boxB.bounds = { 210.0, 10.0, 80.0, 40.0 }; boxB.textUtf8 = "B";
    doc.AddCanvasObject(boxA);
    doc.AddCanvasObject(boxB);
    const std::uint64_t idA = doc.CanvasObjects()[doc.CanvasObjects().size() - 2].id;
    const std::uint64_t idB = doc.CanvasObjects().back().id;
    conn.kind = ObjectKind::Shape;
    conn.shapeKind = ShapeKind::Connector;
    conn.connectorFromId = idA;
    conn.connectorToId = idB;
    conn.shapeStart = { 50.0, 30.0 };
    conn.shapeEnd = { 250.0, 30.0 };
    conn.RecomputeBounds();
    doc.AddCanvasObject(conn);
    const std::uint64_t connId = doc.CanvasObjects().back().id;
    CanvasManip::TranslateAnnotationObject(*doc.FindCanvasObjectMutable(idB), 40.0, 60.0);
    InkEngine::UpdateMagneticConnectors(doc, ObjectTarget{ true, 0 }, idB);
    const auto* movedConn = doc.FindCanvasObject(connId);
    EXPECT(movedConn != nullptr && std::abs(movedConn->shapeEnd.x - 290.0) < 1.0 && std::abs(movedConn->shapeEnd.y - 90.0) < 1.0);

    // 3. Group / Ungroup + Duplicate + 3D surface rotation
    EXPECT(CanvasManip::GroupSelectedObjects(doc, stack, ObjectTarget{ true, 0 }, { idA, idB }, false));
    EXPECT(doc.FindCanvasObject(idA)->groupId != 0 && doc.FindCanvasObject(idA)->groupId == doc.FindCanvasObject(idB)->groupId);
    const auto dups = CanvasManip::DuplicateSelectedObjects(doc, stack, ObjectTarget{ true, 0 }, { idA });
    EXPECT(dups.size() == 1);

    AnnotationObject p3d{};
    p3d.kind = ObjectKind::PlotBlock;
    p3d.plotConfigUtf8 = R"json({"is3D":true,"expr":"sin(x^2+y^2)","yawDeg":42.0,"pitchDeg":28.0})json";
    EXPECT(CanvasManip::IsPlot3DObject(p3d));
    EXPECT(CanvasManip::UpdatePlot3DRotation(p3d, 95.0, 35.0));
    EXPECT(p3d.plotConfigUtf8.find("95") != std::string::npos);

    // 4. Collapsed page ranges & Double-Page Spread
    DocumentModel paged;
    paged.ResetToBlank(DocumentMode::PaginatedPdf);
    for (int i = 0; i < 3; ++i) {
        PageModel pm{};
        pm.pageId = paged.AllocatePageId();
        paged.MutablePages().push_back(std::move(pm));
    }
    paged.RebuildLayout();
    EXPECT(paged.Layout().PageCount() == 4);
    paged.ToggleCollapsedRange(1, 2);
    EXPECT(paged.IsPageCollapsed(1));
    EXPECT(paged.Layout().PlacementAt(1)->collapsedHeader);
    EXPECT(paged.Layout().PlacementAt(1)->heightWorld <= 40.0);
    EXPECT(paged.Layout().PlacementAt(2)->collapsed);
    paged.ToggleCollapsedRange(1, 2);
    paged.SetDoublePageSpread(true);
    EXPECT(paged.Layout().IsDoublePageSpread());
    EXPECT(paged.Layout().PlacementAt(0)->leftWorld < paged.Layout().PlacementAt(1)->leftWorld);

    // 5. Persistent Cross-Session Undo/Redo
    const auto tmpDir = std::filesystem::temp_directory_path() / L"cuadernu_interop_test_tmp";
    std::filesystem::create_directories(tmpDir);
    const std::wstring undoPath = (tmpDir / L"session_undo.json").wstring();
    DocumentModel udoc;
    udoc.ResetToBlank(DocumentMode::InfiniteCanvas);
    CommandStack ustack;
    AnnotationObject n1{};
    n1.kind = ObjectKind::TextBox; n1.bounds = { 20, 20, 120, 40 }; n1.textUtf8 = "Paso 1";
    EXPECT(ustack.Execute(udoc, std::make_unique<AddObjectCommand>(ObjectTarget{ true, 0 }, n1)));
    EXPECT(udoc.CanvasObjects().size() == 1);
    EXPECT(ustack.SaveSessionHistoryToFile(undoPath, udoc));
    CommandStack freshStack;
    EXPECT(freshStack.LoadSessionHistoryFromFile(undoPath, udoc));
    EXPECT(freshStack.CanUndo());
    EXPECT(freshStack.Undo(udoc));
    EXPECT(udoc.CanvasObjects().empty());

    // 6. Non-blocking Continuous Mic Recording + Playback + HTML5 export
    AudioTimelineSession audio;
    EXPECT(audio.StartContinuousMicRecording(16000));
    std::this_thread::sleep_for(std::chrono::milliseconds(160));
    const auto recRep = audio.StopContinuousMicRecording();
    EXPECT(recRep.capturedMs > 0);
    audio.AddBookmark("Inicio demostracion", 100);
    EXPECT(audio.Bookmarks().size() == 1);
    const std::string htmlPath = (tmpDir / "synced_tut.html").string();
    EXPECT(audio.ExportSyncedTutorialHtml5(doc, htmlPath));

    // 7. Image Processing (SVG rasterizer, whiteboard lighting, remove paper bg, perspective)
    const std::string svg = R"(<svg width="100" height="80"><rect x="10" y="10" width="80" height="60" fill="#1d4ed8"/><circle cx="50" cy="40" r="20" fill="#ffffff"/></svg>)";
    ImageProc::RgbaImage svgImg{};
    EXPECT(ImageProc::RasterizeSimpleSvgToRgba(svg, svgImg, 128));
    EXPECT(svgImg.Valid() && svgImg.width == 128);
    ImageProc::EnhanceWhiteboardLighting(svgImg);
    ImageProc::RemovePaperBackground(svgImg, 220);
    // Center white circle should now have transparent alpha=0
    const std::size_t midIdx = (static_cast<std::size_t>(svgImg.height / 2) * svgImg.width + svgImg.width / 2) * 4u;
    EXPECT(svgImg.rgba[midIdx + 3] == 0u);
    const std::array<tinta::Punto, 4> quad = {
        tinta::Punto{ 0.0f, 0.0f },
        tinta::Punto{ static_cast<float>(svgImg.width - 1), 0.0f },
        tinta::Punto{ static_cast<float>(svgImg.width - 1), static_cast<float>(svgImg.height - 1) },
        tinta::Punto{ 0.0f, static_cast<float>(svgImg.height - 1) }
    };
    const ImageProc::RgbaImage rectImg = ImageProc::RectifyPerspectiveQuad(svgImg, quad, 64, 48);
    EXPECT(rectImg.Valid() && rectImg.width == 64 && rectImg.height == 48);

    // 8. PDF Parser Upgrades (/Outlines, /Link, /AcroForm Widget, /EmbeddedFiles, textBoxes)
    auto buildValidSyntheticPdf = []() -> std::vector<std::uint8_t> {
        std::string out = "%PDF-1.4\n";
        std::vector<std::size_t> off(12, 0);
        auto addObj = [&](int num, const std::string& body) {
            off[static_cast<std::size_t>(num)] = out.size();
            out += std::to_string(num) + " 0 obj\n" + body + "\nendobj\n";
        };
        addObj(1, "<< /Type /Catalog /Pages 2 0 R /Outlines 6 0 R /AcroForm << /Fields [ 8 0 R ] >> /Names << /EmbeddedFiles << /Names [ (datos.csv) 9 0 R ] >> >> >>");
        addObj(2, "<< /Type /Pages /Kids [ 3 0 R ] /Count 1 >>");
        addObj(3, "<< /Type /Page /Parent 2 0 R /MediaBox [ 0 0 600 800 ] /Contents 4 0 R /Annots [ 7 0 R 8 0 R ] >>");
        const std::string strm = "BT /F1 12 Tf 72 700 Td (Hola CuadernU) Tj ET";
        addObj(4, "<< /Length " + std::to_string(strm.size()) + " >>\nstream\n" + strm + "\nendstream");
        addObj(5, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
        addObj(6, "<< /Type /Outlines /First 10 0 R /Last 10 0 R /Count 1 >>");
        addObj(7, "<< /Type /Annot /Subtype /Link /Rect [ 50 680 220 710 ] /A << /URI (https://example.org) >> >>");
        addObj(8, "<< /Type /Annot /Subtype /Widget /FT /Btn /T (Aceptado) /V /Yes /Rect [ 50 620 70 640 ] >>");
        addObj(9, "<< /Type /Filespec /F (datos.csv) /EF << /F 11 0 R >> >>");
        addObj(10, "<< /Title (Capitulo 1) /Parent 6 0 R /Dest [ 3 0 R /Fit ] >>");
        addObj(11, "<< /Length 5 >>\nstream\n1,2,3\nendstream");
        const std::size_t xrefPos = out.size();
        out += "xref\n0 12\n0000000000 65535 f \n";
        for (int i = 1; i <= 11; ++i) {
            char buf[32]{};
            std::snprintf(buf, sizeof(buf), "%010zu 00000 n \n", off[static_cast<std::size_t>(i)]);
            out += buf;
        }
        out += "trailer\n<< /Size 12 /Root 1 0 R >>\nstartxref\n" + std::to_string(xrefPos) + "\n%%EOF\n";
        return std::vector<std::uint8_t>(out.begin(), out.end());
    };
    const std::vector<std::uint8_t> pdfBytes = buildValidSyntheticPdf();
    PdfDocument parsedPdf;
    EXPECT(parsedPdf.LoadFromBytes(pdfBytes));
    DocumentModel pdfModel;
    EXPECT(parsedPdf.PopulateDocumentModel(pdfModel, "synthetic.pdf"));
    EXPECT(!pdfModel.Bookmarks().empty() && pdfModel.Bookmarks()[0].titleUtf8 == "Capitulo 1");
    EXPECT(!pdfModel.Attachments().empty() && pdfModel.Attachments()[0].filenameUtf8 == "datos.csv");
    EXPECT(!pdfModel.Pages()[0].pdfLinks.empty() && pdfModel.Pages()[0].pdfLinks[0].uriUtf8 == "https://example.org");
    EXPECT(!pdfModel.Pages()[0].formFields.empty() && pdfModel.Pages()[0].formFields[0].checked);
    EXPECT(!pdfModel.Pages()[0].textBoxes.empty());

    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
}

void TestHardeningRegressions() {
    // 1. Magic-bytes-first format detection (renamed .bin files)
    {
        const std::string fakePdf = "%PDF-1.7\n";
        EXPECT(Interop::NotebookMigrationEngine::DetectFormat("renamed.bin",
            reinterpret_cast<const std::uint8_t*>(fakePdf.data()), fakePdf.size()) == Interop::SourceFormat::PdfDocument);
        const std::uint8_t mscf[8] = { 'M','S','C','F',0,0,0,0 };
        EXPECT(Interop::NotebookMigrationEngine::DetectFormat("pkg.UNKNOWN", mscf, sizeof(mscf)) == Interop::SourceFormat::OneNotePackage);
        const std::uint8_t tintaSig[8] = { 'T','I','N','T','A',0,1,0 };
        EXPECT(Interop::NotebookMigrationEngine::DetectFormat("cuaderno.dat", tintaSig, sizeof(tintaSig)) == Interop::SourceFormat::CuadernuTinta);
    }

    // 2. Goodnotes 6 image + JSON inside notes/ folder + UTF-8 Spanish text in protobuf
    {
        ZipArchive z;
        const std::vector<std::uint8_t> tinyPng = { 0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A, 0,0,0,0,'I','E','N','D',0xAE,0x42,0x60,0x82 };
        z.SetEntry("notes/photo.png", tinyPng);
        z.SetTextEntry("notes/meta.json", R"json({"title":"Física III","bookmarks":[{"title":"Óptica","page":0}]})json");
        std::vector<std::uint8_t> pb;
        const std::string spanish = "Ecuaci\xC3\xB3n de Maxwell";
        AppendPbBytesField(pb, 9, std::vector<std::uint8_t>(spanish.begin(), spanish.end()));
        z.SetEntry("notes/p1.pb", pb);
        std::vector<std::uint8_t> gnBytes;
        EXPECT(z.Serialize(gnBytes, true));
        DocumentModel doc;
        Interop::MigrationResult res{};
        EXPECT(Interop::NotebookMigrationEngine::ImportAndMigrateBytes(gnBytes.data(), gnBytes.size(), "clase.goodnotes", doc, res));
        EXPECT(res.report.imageCount >= 1);
        EXPECT(res.report.bookmarkOrSectionCount >= 1);
        EXPECT(res.report.textBoxCount >= 1);
        EXPECT(doc.Title() == "F\xC3\xADsica III");
    }

    // 3. OneNote odd-aligned UTF-16LE + embedded JPEG + AudioClip assetKey/audioAssetKey .cudoc round-trip
    {
        std::vector<std::uint8_t> oneBytes;
        oneBytes.push_back(0xAA); // force odd alignment!
        const std::u16string txt = u"C\u00E1lculo \u03B1 + \u03B2 = \u03C0";
        for (char16_t ch : txt) {
            oneBytes.push_back(static_cast<std::uint8_t>(ch & 0xFFu));
            oneBytes.push_back(static_cast<std::uint8_t>((ch >> 8) & 0xFFu));
        }
        oneBytes.push_back(0); oneBytes.push_back(0);
        // Embed synthetic JPEG block (FF D8 FF E0 ... FF D9, >= 64 bytes)
        std::vector<std::uint8_t> jpg(72, 0x11);
        jpg[0] = 0xFF; jpg[1] = 0xD8; jpg[2] = 0xFF; jpg[3] = 0xE0;
        jpg[70] = 0xFF; jpg[71] = 0xD9;
        oneBytes.insert(oneBytes.end(), jpg.begin(), jpg.end());

        DocumentModel oneDoc;
        Interop::MigrationResult oneRes{};
        EXPECT(Interop::NotebookMigrationEngine::ImportAndMigrateBytes(oneBytes.data(), oneBytes.size(), "sec.one", oneDoc, oneRes));
        EXPECT(oneRes.report.textBoxCount >= 1);
        EXPECT(oneRes.report.imageCount >= 1);

        // Verify AudioClip round-trip keeps audioAssetKey AND assetKey synchronized
        AnnotationObject clip{};
        clip.kind = ObjectKind::AudioClip;
        clip.bounds = { 10, 10, 200, 40 };
        clip.assetKey = "audio_key_42";
        oneDoc.AddPageObject(0, clip);
        std::vector<std::uint8_t> cudocBytes;
        EXPECT(CudocStorage::SerializeToBytes(oneDoc, cudocBytes));
        DocumentModel reloaded;
        EXPECT(CudocStorage::DeserializeFromBytes(cudocBytes.data(), cudocBytes.size(), reloaded));
        bool foundClip = false;
        for (const auto& o : reloaded.Pages()[0].objects) {
            if (o.kind == ObjectKind::AudioClip) {
                foundClip = (o.audioAssetKey == "audio_key_42" && o.assetKey == "audio_key_42");
            }
        }
        EXPECT(foundClip);
    }

    // 4. .tinta extended Pauta mapping (Milimetrico, Polar, Smith) + tp.medios migration
    {
        tinta::Documento td{};
        td.id = "tinta-pauta-test";
        td.titulo = "Papeles Tinta";
        tinta::Cuaderno cu{};
        cu.titulo = "Ing";
        tinta::Seccion sec{};
        sec.titulo = "RF";
        tinta::Pagina p0{}; p0.papel.pauta = tinta::Pauta::Milimetrico;
        tinta::Medio md{};
        md.id = "m1"; md.hash = "sha256_img1"; md.nombre_original = "circuito.png";
        md.tipo = tinta::TipoMedio::Imagen; md.marco = { 20, 30, 180, 120 };
        p0.medios.push_back(md);
        tinta::Pagina p1{}; p1.papel.pauta = tinta::Pauta::Smith;
        tinta::Pagina p2{}; p2.papel.pauta = tinta::Pauta::Polar;
        sec.paginas.push_back(p0); sec.paginas.push_back(p1); sec.paginas.push_back(p2);
        cu.secciones.push_back(sec);
        td.cuadernos.push_back(cu);
        const auto raw = tinta::Serializar(td);
        DocumentModel doc;
        Interop::MigrationResult res{};
        EXPECT(Interop::NotebookMigrationEngine::ImportAndMigrateBytes(raw.data(), raw.size(), "rf.tinta", doc, res));
        EXPECT(doc.Pages().size() == 3);
        EXPECT(doc.Pages()[0].background == PaperPattern::Millimeter);
        EXPECT(doc.Pages()[1].background == PaperPattern::SmithChart);
        EXPECT(doc.Pages()[2].background == PaperPattern::Polar);
        EXPECT(res.report.imageCount == 1);
    }

    // 5. Persistent Undo/Redo across reload + new live edits (regression test for hadSnapCmds bug!)
    {
        const auto tmpHist = std::filesystem::temp_directory_path() / "cuadernu_hist_regress.json";
        DocumentModel d;
        d.ResetToBlank(DocumentMode::InfiniteCanvas);
        CommandStack st;
        AnnotationObject a{}; a.kind = ObjectKind::TextBox; a.bounds = { 10, 10, 100, 40 }; a.textUtf8 = "Paso1";
        st.Execute(d, std::make_unique<AddObjectCommand>(ObjectTarget{ true, 0 }, a));
        EXPECT(st.SaveSessionHistoryToFile(tmpHist.wstring(), d));

        // Reload history into fresh stack, then make a NEW live edit and save AGAIN
        CommandStack st2;
        EXPECT(st2.LoadSessionHistoryFromFile(tmpHist.wstring(), d));
        AnnotationObject b{}; b.kind = ObjectKind::TextBox; b.bounds = { 10, 60, 100, 40 }; b.textUtf8 = "Paso2";
        st2.Execute(d, std::make_unique<AddObjectCommand>(ObjectTarget{ true, 0 }, b));
        EXPECT(d.CanvasObjects().size() == 2);
        EXPECT(st2.SaveSessionHistoryToFile(tmpHist.wstring(), d));

        // Reload a third time and verify BOTH Paso2 AND Paso1 undo cleanly!
        CommandStack st3;
        EXPECT(st3.LoadSessionHistoryFromFile(tmpHist.wstring(), d));
        EXPECT(st3.Undo(d));
        EXPECT(d.CanvasObjects().size() == 1);
        EXPECT(st3.Undo(d));
        EXPECT(d.CanvasObjects().empty());
        std::error_code ec;
        std::filesystem::remove(tmpHist, ec);
    }

    // 6. PageLayout mixed-height doublePageSpread + 6 consecutive collapsed pages
    {
        DocumentModel d;
        d.ResetToBlank(DocumentMode::PaginatedPdf);
        d.MutablePages().clear();
        const double heights[8] = { 900, 400, 500, 500, 500, 500, 500, 750 };
        for (int i = 0; i < 8; ++i) {
            PageModel pm{};
            pm.pageId = d.AllocatePageId();
            pm.widthPt = 500.0;
            pm.heightPt = heights[i];
            d.MutablePages().push_back(std::move(pm));
        }
        d.SetDoublePageSpread(true);
        d.ToggleCollapsedRange(2, 6); // 5 collapsed pages in a row
        // Hit-test tall left page (page 0) below the shorter right page (page 1, height 400)
        const auto* pl0 = d.Layout().PlacementAt(0);
        const auto* pl7 = d.Layout().PlacementAt(7);
        EXPECT(pl0 != nullptr && pl7 != nullptr);
        std::size_t hitPg = 99;
        double lx = 0, ly = 0;
        EXPECT(d.Layout().HitTestWorldPoint(pl0->leftWorld + 100.0, pl0->topWorld + 750.0, hitPg, lx, ly));
        EXPECT(hitPg == 0);
        EXPECT(d.Layout().HitTestWorldPoint(pl7->leftWorld + 100.0, pl7->topWorld + 100.0, hitPg, lx, ly));
        EXPECT(hitPg == 7);
        std::size_t vf = 99, vl = 0;
        EXPECT(d.Layout().ComputeVisibleRange(pl0->topWorld + 700.0, pl0->topWorld + 850.0, vf, vl, 0));
        EXPECT(vf == 0);
    }

    // 7. PPTX <a:tblGrid><a:gridCol .../> + non-duplicated TextBox + OMML matrix/nary parsing
    {
        DocumentModel src;
        src.ResetToBlank(DocumentMode::PaginatedPdf);
        AnnotationObject tb{}; tb.kind = ObjectKind::TextBox; tb.bounds = { 50, 50, 200, 40 }; tb.textUtf8 = "UnicoTextoSlide";
        src.AddPageObject(0, tb);
        AnnotationObject tbl{}; tbl.kind = ObjectKind::TableBlock; tbl.bounds = { 50, 120, 400, 120 };
        tbl.tableJsonUtf8 = R"json({"rows":[["A","B"],["1","2"]]})json";
        src.AddPageObject(0, tbl);
        std::vector<std::uint8_t> pptx;
        EXPECT(OpenXmlInterop::ExportToPptxBytes(src, {}, pptx));
        ZipArchive z;
        EXPECT(z.Deserialize(pptx.data(), pptx.size()));
        const auto* s1 = z.FindEntry("ppt/slides/slide1.xml");
        EXPECT(s1 != nullptr);
        const std::string sXml(reinterpret_cast<const char*>(s1->data()), s1->size());
        EXPECT(sXml.find("<a:gridCol w=") != std::string::npos);
        // Count occurrences of "UnicoTextoSlide": must appear EXACTLY ONCE (no duplicate slideText blob!)
        const std::size_t p1 = sXml.find("UnicoTextoSlide");
        EXPECT(p1 != std::string::npos);
        EXPECT(sXml.find("UnicoTextoSlide", p1 + 1) == std::string::npos);
    }

    // 8. WAL snapshot recovery end-to-end
    {
        const auto tmpBase = std::filesystem::temp_directory_path() / "cuadernu_wal_snap_test.cudoc";
        const auto tmpWal = std::filesystem::path(tmpBase.wstring() + L"-wal");
        DocumentModel baseDoc;
        baseDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        baseDoc.SetTitle("AntesCrash");
        EXPECT(CudocStorage::SaveToFile(tmpBase.wstring(), baseDoc));

        // Mutate in memory and write snapshot WAL
        baseDoc.SetTitle("DespuesCrash");
        AnnotationObject note{}; note.kind = ObjectKind::TextBox; note.bounds = { 20, 20, 160, 40 }; note.textUtf8 = "Recuperado";
        baseDoc.AddCanvasObject(note);
        std::vector<std::uint8_t> snapBytes;
        EXPECT(CudocStorage::SerializeToBytes(baseDoc, snapBytes));
        static constexpr char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string b64;
        for (std::size_t i = 0; i < snapBytes.size(); i += 3u) {
            const std::uint32_t b0 = snapBytes[i];
            const std::uint32_t b1 = (i + 1u < snapBytes.size()) ? snapBytes[i + 1u] : 0u;
            const std::uint32_t b2 = (i + 2u < snapBytes.size()) ? snapBytes[i + 2u] : 0u;
            const std::uint32_t t = (b0 << 16) | (b1 << 8) | b2;
            b64.push_back(kB64[(t >> 18) & 0x3Fu]);
            b64.push_back(kB64[(t >> 12) & 0x3Fu]);
            b64.push_back((i + 1u < snapBytes.size()) ? kB64[(t >> 6) & 0x3Fu] : '=');
            b64.push_back((i + 2u < snapBytes.size()) ? kB64[t & 0x3Fu] : '=');
        }
        WalRecord wr{};
        wr.opTag = "snapshot";
        wr.payloadJson = "{\"cudocB64\":\"" + b64 + "\"}";
        EXPECT(CudocWal::AppendRecord(tmpWal.wstring(), wr));

        DocumentModel recovered;
        std::size_t replayed = 0;
        EXPECT(CudocWal::RecoverDocument(tmpBase.wstring(), tmpWal.wstring(), recovered, &replayed));
        EXPECT(replayed >= 1);
        EXPECT(recovered.Title() == "DespuesCrash");
        EXPECT(recovered.CanvasObjects().size() == 1);
        std::error_code ec;
        std::filesystem::remove(tmpBase, ec);
        std::filesystem::remove(tmpWal, ec);
    }

    // 9. Statics FBD + GUM Uncertainty + Control Block PID integration checks
    {
        Cas::FbdModel fbd{};
        fbd.supports.push_back({ "A", "RA", Cas::FbdSupportKind::Pin, 0.0, 0.0, 90.0 });
        fbd.supports.push_back({ "B", "RB", Cas::FbdSupportKind::RollerX, 4.0, 0.0, 90.0 });
        fbd.forces.push_back({ "F1", "P", 2.0, 0.0, 100.0, -90.0, false, "N" });
        const auto s = Cas::SolveFbdEquilibrium(fbd);
        EXPECT(s.ok && s.inEquilibrium && s.reactions.size() == 3);

        std::unordered_map<std::string, Cas::UncertainValue> vars;
        vars["V"] = { 12.0, 0.1, {}, "V", 3 };
        vars["R"] = { 1000.0, 10.0, {}, "Ohm", 3 };
        const auto u = Cas::PropagateUncertainty("V/R", vars, 1000);
        EXPECT(u.ok && std::abs(u.mean - 0.012) < 1e-6);

        Circuit::ControlBlockDiagram diag{};
        Circuit::ControlBlockNode n1{ 1, Circuit::ControlNodeKind::Input, "R" };
        Circuit::ControlBlockNode n2{ 2, Circuit::ControlNodeKind::SumJunction, "E" };
        Circuit::ControlBlockNode n3{ 3, Circuit::ControlNodeKind::Pid, "PID" };
        n3.kp = 2.0; n3.ki = 1.0; n3.kd = 0.1;
        Circuit::ControlBlockNode n4{ 4, Circuit::ControlNodeKind::TransferFunction, "G" };
        n4.tf = { Circuit::Poly1D{ { 1.0 } }, Circuit::Poly1D{ { 1.0, 0.5 } } };
        Circuit::ControlBlockNode n5{ 5, Circuit::ControlNodeKind::Output, "Y" };
        diag.nodes = { n1, n2, n3, n4, n5 };
        diag.edges = { { 1, 1, 2, +1, {} }, { 2, 2, 3, +1, {} }, { 3, 3, 4, +1, {} }, { 4, 4, 5, +1, {} }, { 5, 4, 2, -1, {} } };
        const auto ca = Circuit::AnalyzeControlBlockDiagram(diag, 2.0, 40);
        EXPECT(ca.ok && ca.isStableRouth && !ca.stepOutput.empty());
    }
}

} // namespace

int main() {
    TestGoodnotesMigration();
    TestNotabilityMigration();
    TestOneNoteSectionAndPackageMigration();
    TestTintaLegacyMigration();
    TestDocxAndPptxHighFidelityRoundTrip();
    TestNegativeCorruptInputs();
    TestTrackAToDCoreEngines();
    TestHardeningRegressions();

    std::cout << "CuadernuInteropTest: " << g_pass << " passed, " << g_fail << " failed.\n";
    return (g_fail == 0) ? 0 : 1;
}
