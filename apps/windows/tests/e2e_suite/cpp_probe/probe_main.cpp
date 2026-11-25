#include "bridge/calc_bridge.hpp"
#include "bridge/lab_report_exporter.hpp"
#include "circuit/mna_solver.hpp"
#include "core/cudoc_storage.hpp"
#include "core/document_model.hpp"
#include "core/notebook_vault.hpp"
#include "core/utf_conv.hpp"
#include "core/zip_container.hpp"
#include "math/engineering_cas.hpp"
#include "pdf/interop_openxml.hpp"
#include "pdf/pdf_parser.hpp"
#include "pdf/pdf_signer.hpp"

#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool ReadFileBytes(const std::string& pathUtf8, std::vector<std::uint8_t>& out, std::string* outErr) {
    out.clear();
    std::error_code ec;
    const std::filesystem::path p = std::filesystem::u8path(pathUtf8);
    std::ifstream ifs(p, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        if (outErr) *outErr = "Failed to open file: " + pathUtf8;
        return false;
    }
    const std::ifstream::pos_type sz = ifs.tellg();
    if (sz < 0) {
        if (outErr) *outErr = "Failed to determine size: " + pathUtf8;
        return false;
    }
    out.resize(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    if (!out.empty() && !ifs.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()))) {
        if (outErr) *outErr = "Failed reading file bytes: " + pathUtf8;
        return false;
    }
    (void)ec;
    return true;
}

bool WriteTextFile(const std::string& pathUtf8, const std::string& text, std::string* outErr) {
    const std::filesystem::path p = std::filesystem::u8path(pathUtf8);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        if (outErr) *outErr = "Failed to open output file: " + pathUtf8;
        return false;
    }
    ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
    ofs.flush();
    if (!ofs.good()) {
        if (outErr) *outErr = "Failed writing output file: " + pathUtf8;
        return false;
    }
    return true;
}

std::string TrimWhitespace(const std::string& s) {
    std::size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    std::size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

DocStudio::DocumentModel BuildProbeReportDoc(const std::string& exprText) {
    using namespace DocStudio;
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::InfiniteCanvas);
    doc.SetTitle("ProbeCalculationReport");
    const auto casRes = Cas::EvaluateExpression(exprText);
    AnnotationObject obj;
    obj.id = doc.AllocateObjectId();
    obj.zOrder = doc.AllocateZOrder();
    obj.kind = ObjectKind::MathBlock;
    obj.layerId = doc.ActiveLayerId();
    obj.bounds = { 20.0, 20.0, 340.0, 90.0 };
    obj.formulaExprUtf8 = exprText;
    obj.textUtf8 = exprText;
    obj.evaluatedResultUtf8 = casRes.ok ? casRes.formattedResultUtf8 : "";
    doc.AddCanvasObject(std::move(obj));
    return doc;
}

int RunSelfTest() {
    using namespace DocStudio;

    // 1. Engineering CAS unit expression + dimensional check
    const auto casRes = Cas::EvaluateExpression("V_out = 4.7 kOhm * 2 mA | V");
    if (!casRes.ok || std::abs(casRes.result.Mag() - 9.4) > 1e-4) {
        std::cerr << "[PROBE FAIL] CAS unit evaluation failed: " << casRes.errorUtf8 << "\n";
        return 1;
    }

    // 2. Deterministic CheckAlgebraicStep (Check My Work oracle)
    const auto stepOk = Cas::CheckAlgebraicStep("(x + 1)^2", "x^2 + 2*x + 1", "x");
    const auto stepBad = Cas::CheckAlgebraicStep("(x + 1)^2", "x^2 + 1", "x");
    if (!stepOk.equivalent || stepBad.equivalent) {
        std::cerr << "[PROBE FAIL] CheckAlgebraicStep parity failed\n";
        return 2;
    }

    // 3. MNA DC divider + SVG / DXF / KiCad exports
    const std::string netlist = "V1 1 0 12.0\nR1 1 2 1000.0\nR2 2 0 2000.0\n";
    std::vector<Circuit::CircuitComponent> comps;
    std::string parseErr;
    if (!Circuit::ParseNetlistText(netlist, comps, &parseErr)) {
        std::cerr << "[PROBE FAIL] ParseNetlistText failed: " << parseErr << "\n";
        return 3;
    }
    const auto sol = Circuit::SolveMnaCircuit(comps, 0.0);
    if (!sol.ok || sol.nodeVoltages.find(2) == sol.nodeVoltages.end() ||
        std::abs(sol.nodeVoltages.at(2).real() - 8.0) > 1e-3) {
        std::cerr << "[PROBE FAIL] MNA voltage divider failed\n";
        return 4;
    }
    const std::string svg = Circuit::ExportCircuitToSvg(comps, &sol);
    const std::string dxf = Circuit::ExportCircuitToDxf(comps, &sol);
    const std::string kicad = Circuit::ExportCircuitToKiCadNetlist(comps, "ProbeCircuit");
    if (svg.find("<svg") == std::string::npos ||
        dxf.find("ENTITIES") == std::string::npos ||
        kicad.find(".end") == std::string::npos ||
        kicad.find("N1") == std::string::npos) {
        std::cerr << "[PROBE FAIL] Circuit CAD exporter check failed\n";
        return 5;
    }

    // 4. CalcBridge keystroke & phasor formatting
    const auto keyPlan = CalcBridge::TranslateExpressionToCasioKeys("det([[2, 1], [1, 3]])", "fx-991CW");
    const std::string phasor = CalcBridge::FormatPhasorAllFormsUtf8({3.0, 4.0}, "V");
    if (!keyPlan.ok || phasor.find("Polar: 5") == std::string::npos) {
        std::cerr << "[PROBE FAIL] CalcBridge probe failed\n";
        return 6;
    }

    // 5. .cudoc round-trip serialization
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::InfiniteCanvas);
    doc.SetTitle("ProbeNotebook");
    std::vector<std::uint8_t> bytes;
    std::string serErr;
    if (!CudocStorage::SerializeToBytes(doc, bytes, &serErr) || bytes.empty()) {
        std::cerr << "[PROBE FAIL] CudocStorage serialize failed: " << serErr << "\n";
        return 7;
    }
    DocumentModel loaded;
    if (!CudocStorage::DeserializeFromBytes(bytes.data(), bytes.size(), loaded, &serErr) ||
        loaded.Title() != "ProbeNotebook") {
        std::cerr << "[PROBE FAIL] CudocStorage deserialize failed: " << serErr << "\n";
        return 8;
    }

    std::cout << "[PROBE PASS] CAS=" << casRes.formattedResultUtf8
              << " MNA_V2=" << sol.nodeVoltages.at(2).real()
              << " CUDOC_BYTES=" << bytes.size() << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    using namespace DocStudio;

    if (argc <= 1) {
        return RunSelfTest();
    }

    const std::string cmd = argv[1];

    if (cmd == "--cas" && argc >= 3) {
        const auto res = Cas::EvaluateExpression(argv[2]);
        if (!res.ok) {
            std::cerr << res.errorUtf8 << "\n";
            return 1;
        }
        std::cout << res.formattedResultUtf8 << "\n";
        return 0;
    }

    if (cmd == "--step" && argc >= 4) {
        const auto res = Cas::CheckAlgebraicStep(argv[2], argv[3], "x");
        std::cout << res.statusCode << "|" << (res.equivalent ? "1" : "0") << "|" << res.messageUtf8 << "\n";
        return res.equivalent ? 0 : 2;
    }

    if (cmd == "--mna" && argc >= 3) {
        const auto sol = Circuit::SolveMnaNetlist(argv[2], 0.0);
        if (!sol.ok) {
            std::cerr << sol.errorUtf8 << "\n";
            return 1;
        }
        std::cout << sol.summaryUtf8 << "\n";
        return 0;
    }

    if (cmd == "--bridge" && argc >= 3) {
        const std::string model = (argc >= 4) ? argv[3] : "fx-991CW";
        const auto plan = CalcBridge::TranslateExpressionToCasioKeys(argv[2], model);
        if (!plan.ok) {
            std::cerr << "CalcBridge translation failed\n";
            return 1;
        }
        std::cout << "BRIDGE|" << plan.appModeUtf8 << "|" << plan.keySequenceUtf8
                  << "|" << plan.evaluatedUtf8 << "\n";
        return 0;
    }

    if (cmd == "--check-zip" && argc >= 3) {
        std::vector<std::uint8_t> bytes;
        std::string err;
        if (!ReadFileBytes(argv[2], bytes, &err)) {
            std::cerr << err << "\n";
            return 1;
        }
        ZipArchive za;
        if (!za.Deserialize(bytes.data(), bytes.size(), &err)) {
            std::cerr << err << "\n";
            return 1;
        }
        std::cout << "ZIP_OK|entries=" << za.ListEntries().size() << "\n";
        return 0;
    }

    if ((cmd == "--check-cudoc" || cmd == "--cudoc") && argc >= 3) {
        DocumentModel doc;
        std::string err;
        if (!CudocStorage::LoadFromFile(Utf8ToWide(argv[2]), doc, &err)) {
            std::cerr << err << "\n";
            return 1;
        }
        std::cout << "CUDOC_OK|pages=" << doc.Pages().size()
                  << "|canvas=" << doc.CanvasObjects().size() << "\n";
        return 0;
    }

    if (cmd == "--check-cudoc-recovery" && argc >= 3) {
        DocumentModel doc;
        bool usedBak = false;
        std::string err;
        if (!CudocStorage::LoadFromFileWithRecovery(Utf8ToWide(argv[2]), doc, &usedBak, &err)) {
            std::cerr << err << "\n";
            return 1;
        }
        std::cout << "OK|usedBak=" << (usedBak ? "1" : "0") << "\n";
        return 0;
    }

    if (cmd == "--check-pdf" && argc >= 3) {
        bool requireText = false;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "--require-text") requireText = true;
        }
        PdfDocument pdf;
        std::string err;
        if (!pdf.LoadFromFile(Utf8ToWide(argv[2]), &err)) {
            std::cerr << err << "\n";
            return 1;
        }
        std::string allText;
        for (const auto& pg : pdf.Pages()) {
            allText += pg.extractedTextUtf8;
        }
        const std::string trimmed = TrimWhitespace(allText);
        if (requireText && trimmed.empty()) {
            std::cerr << "PDF loaded but extracted text is empty (--require-text)\n";
            return 1;
        }
        std::cout << "PDF_OK|pages=" << pdf.PageCount()
                  << "|textBytes=" << trimmed.size() << "\n";
        return 0;
    }

    if (cmd == "--verify-sig" && argc >= 3) {
        const auto sig = PdfSigner::VerifyPdfFile(Utf8ToWide(argv[2]));
        const bool modified = sig.hasSignature && (!sig.byteRangeValid || !sig.cryptographicValid);
        std::cout << "SIG|" << (sig.cryptographicValid ? "1" : "0")
                  << "|" << (sig.byteRangeValid ? "1" : "0")
                  << "|" << (modified ? "1" : "0")
                  << "|" << sig.diagnosticUtf8 << "\n";
        const bool ok = sig.hasSignature && sig.cryptographicValid && sig.byteRangeValid;
        return ok ? 0 : 2;
    }

    if (cmd == "--check-docx" && argc >= 3) {
        DocumentModel doc;
        std::string err;
        if (!OpenXmlInterop::ImportFromDocxFile(Utf8ToWide(argv[2]), doc, &err)) {
            std::cerr << err << "\n";
            return 1;
        }
        std::cout << "DOCX_OK|pages=" << doc.Pages().size() << "\n";
        return 0;
    }

    if (cmd == "--check-pptx" && argc >= 3) {
        DocumentModel doc;
        std::string err;
        if (!OpenXmlInterop::ImportFromPptxFile(Utf8ToWide(argv[2]), doc, &err)) {
            std::cerr << err << "\n";
            return 1;
        }
        std::cout << "PPTX_OK|pages=" << doc.Pages().size() << "\n";
        return 0;
    }

    if (cmd == "--check-vault" && argc >= 3) {
        NotebookVault vault;
        std::string err;
        if (!vault.LoadFromFile(Utf8ToWide(argv[2]), &err)) {
            std::cerr << err << "\n";
            return 1;
        }
        std::cout << "VAULT_OK|notebooks=" << vault.Notebooks().size() << "\n";
        return 0;
    }

    if (cmd == "--mna-svg" && argc >= 4) {
        std::vector<Circuit::CircuitComponent> comps;
        std::string err;
        if (!Circuit::ParseNetlistText(argv[2], comps, &err)) {
            std::cerr << err << "\n";
            return 1;
        }
        const auto sol = Circuit::SolveMnaCircuit(comps, 0.0);
        if (!sol.ok) {
            std::cerr << sol.errorUtf8 << "\n";
            return 1;
        }
        const std::string svg = Circuit::ExportCircuitToSvg(comps, &sol);
        if (!WriteTextFile(argv[3], svg, &err)) {
            std::cerr << err << "\n";
            return 1;
        }
        std::cout << "SVG_OK|bytes=" << svg.size() << "\n";
        return 0;
    }

    if (cmd == "--export-report-html" && argc >= 4) {
        const DocumentModel doc = BuildProbeReportDoc(argv[2]);
        const std::string html = Bridge::ExportCalculationReportHtml(doc);
        std::string err;
        if (!WriteTextFile(argv[3], html, &err)) {
            std::cerr << err << "\n";
            return 1;
        }
        std::cout << "HTML_OK|bytes=" << html.size() << "\n";
        return 0;
    }

    if (cmd == "--export-report-md" && argc >= 4) {
        const DocumentModel doc = BuildProbeReportDoc(argv[2]);
        const std::string md = Bridge::ExportCalculationReportMarkdown(doc);
        std::string err;
        if (!WriteTextFile(argv[3], md, &err)) {
            std::cerr << err << "\n";
            return 1;
        }
        std::cout << "MD_OK|bytes=" << md.size() << "\n";
        return 0;
    }

    if (cmd == "--fit-csv" && argc >= 3) {
        std::vector<std::uint8_t> bytes;
        std::string err;
        if (!ReadFileBytes(argv[2], bytes, &err)) {
            std::cerr << err << "\n";
            return 1;
        }
        const std::string csvText(bytes.begin(), bytes.end());
        const auto reg = Cas::FitCsvRegression(csvText, 1);
        if (!reg.ok) {
            std::cerr << "FitCsvRegression failed\n";
            return 1;
        }
        std::cout << "FIT_OK|" << reg.equationUtf8 << "|R2=" << reg.rSquared << "\n";
        return 0;
    }

    if (cmd == "--check-my-work" && argc >= 4) {
        const std::vector<std::string> lines{ argv[2], argv[3] };
        const auto rep = Cas::CheckMyWork(lines);
        std::cout << "CMW|" << (rep.allValid ? "1" : "0")
                  << "|" << rep.firstErrorLineIndex
                  << "|" << rep.summaryUtf8 << "\n";
        return rep.allValid ? 0 : 2;
    }

    std::cerr << "Invalid or incomplete cpp_probe arguments: " << cmd << "\n";
    return 64;
}
