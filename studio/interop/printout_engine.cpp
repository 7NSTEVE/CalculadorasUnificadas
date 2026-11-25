#include "printout_engine.hpp"

#include "../core/cudoc_storage.hpp"
#include "../core/utf_conv.hpp"
#include "../pdf/interop_openxml.hpp"
#include "../pdf/pdf_parser.hpp"
#include "../pdf/pdf_writer.hpp"
#include "../pdf/table_interop.hpp"
#include "../pdf/winrt_adapters.hpp"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <sstream>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <objbase.h>
#include <oleauto.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#endif

namespace DocStudio::Printout {

namespace {

std::wstring GetLowerExtW(const std::wstring& path) {
    try {
        std::wstring ext = std::filesystem::path(path).extension().wstring();
        for (wchar_t& c : ext) c = static_cast<wchar_t>(std::towlower(c));
        return ext;
    } catch (...) {
        return {};
    }
}

std::string DetectSourceKind(const std::wstring& ext) {
    if (ext == L".pdf") return "pdf";
    if (ext == L".docx" || ext == L".doc") return "docx";
    if (ext == L".pptx" || ext == L".ppt") return "pptx";
    if (ext == L".xlsx" || ext == L".xls" || ext == L".csv") return "xlsx";
    return "unknown";
}

std::filesystem::path EnsurePrintoutTempDir() {
    std::error_code ec;
    auto dir = std::filesystem::temp_directory_path(ec) / L"CuadernU" / L"printout";
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::uint64_t NextTempSeq() noexcept {
    static std::atomic<std::uint64_t> sSeq{ 1 };
    return sSeq.fetch_add(1, std::memory_order_relaxed);
}

bool ReadFileBytes(const std::wstring& path, std::vector<std::uint8_t>& out) {
    std::ifstream ifs(std::filesystem::path(path), std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) return false;
    const auto sz = ifs.tellg();
    if (sz <= 0 || static_cast<std::uint64_t>(sz) > 512ull * 1024ull * 1024ull) return false;
    out.resize(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    return static_cast<bool>(ifs.read(reinterpret_cast<char*>(out.data()), sz));
}

std::size_t CountNonWhitespace(const std::string& s) noexcept {
    std::size_t count = 0;
    for (unsigned char c : s) {
        if (c > 0x20u) ++count;
    }
    return count;
}

std::vector<PdfTextRunBox> SynthesizeLineRunsFromPlainText(const std::string& text) {
    std::vector<PdfTextRunBox> runs;
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (CountNonWhitespace(line) > 0) {
            lines.push_back(std::move(line));
        }
    }
    if (lines.empty()) return runs;
    const double n = static_cast<double>(lines.size());
    const double stepY = std::clamp(0.84 / (std::max)(1.0, n), 0.018, 0.08);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        PdfTextRunBox rb{};
        rb.textUtf8 = std::move(lines[i]);
        const double y = std::clamp(0.06 + static_cast<double>(i) * stepY, 0.04, 0.94);
        rb.rectNorm = { 0.06, y, 0.88, (std::max)(0.022, stepY * 0.92) };
        runs.push_back(std::move(rb));
    }
    return runs;
}

#ifdef _WIN32

struct ScopedVariant {
    VARIANT v{};
    ScopedVariant() { VariantInit(&v); }
    ~ScopedVariant() { VariantClear(&v); }
    ScopedVariant(const ScopedVariant&) = delete;
    ScopedVariant& operator=(const ScopedVariant&) = delete;

    static VARIANT MakeBool(bool b) {
        VARIANT r{};
        VariantInit(&r);
        r.vt = VT_BOOL;
        r.boolVal = b ? VARIANT_TRUE : VARIANT_FALSE;
        return r;
    }
    static VARIANT MakeInt(long val) {
        VARIANT r{};
        VariantInit(&r);
        r.vt = VT_I4;
        r.lVal = val;
        return r;
    }
    static VARIANT MakeBstr(const std::wstring& s) {
        VARIANT r{};
        VariantInit(&r);
        r.vt = VT_BSTR;
        r.bstrVal = SysAllocStringLen(s.data(), static_cast<UINT>(s.size()));
        return r;
    }
};

HRESULT ComGetDispId(IDispatch* disp, const wchar_t* name, DISPID& outId) {
    if (!disp || !name) return E_POINTER;
    LPOLESTR oleName = const_cast<LPOLESTR>(name);
    return disp->GetIDsOfNames(IID_NULL, &oleName, 1, LOCALE_USER_DEFAULT, &outId);
}

// Passes arguments in normal left-to-right order; reverses internally for IDispatch::Invoke.
HRESULT ComInvoke(
    IDispatch* disp,
    WORD flags,
    const wchar_t* name,
    std::vector<VARIANT>& argsLeftToRight,
    VARIANT* outResult = nullptr)
{
    auto isComBusy = [](HRESULT h) noexcept {
        return h == RPC_E_CALL_REJECTED || h == RPC_E_SERVERCALL_RETRYLATER;
    };

    DISPID dispid = 0;
    HRESULT hr = E_FAIL;
    for (int attempt = 0; attempt < 15; ++attempt) {
        hr = ComGetDispId(disp, name, dispid);
        if (!isComBusy(hr)) break;
        Sleep(150);
    }
    if (FAILED(hr)) return hr;

    std::vector<VARIANT> revArgs(argsLeftToRight.rbegin(), argsLeftToRight.rend());
    DISPPARAMS dp{};
    dp.cArgs = static_cast<UINT>(revArgs.size());
    dp.rgvarg = revArgs.empty() ? nullptr : revArgs.data();

    DISPID dispidNamed = DISPID_PROPERTYPUT;
    if (flags & DISPATCH_PROPERTYPUT) {
        dp.cNamedArgs = 1;
        dp.rgdispidNamedArgs = &dispidNamed;
    }

    HRESULT invokeHr = E_FAIL;
    for (int attempt = 0; attempt < 15; ++attempt) {
        invokeHr = disp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, outResult, nullptr, nullptr);
        if (!isComBusy(invokeHr)) break;
        Sleep(150);
    }
    for (auto& a : argsLeftToRight) {
        VariantClear(&a);
    }
    argsLeftToRight.clear();
    return invokeHr;
}

bool RunProcessWithJobTimeout(
    std::wstring cmdLine,
    unsigned timeoutMs,
    DWORD* outExitCode = nullptr)
{
    HANDLE hJob = CreateJobObjectW(nullptr, nullptr);
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    const BOOL created = CreateProcessW(
        nullptr,
        cmdLine.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED,
        nullptr,
        nullptr,
        &si,
        &pi);
    if (!created) {
        if (hJob) CloseHandle(hJob);
        return false;
    }

    if (hJob) {
        AssignProcessToJobObject(hJob, pi.hProcess);
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    const DWORD waitRes = WaitForSingleObject(pi.hProcess, timeoutMs);
    if (waitRes != WAIT_OBJECT_0) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        if (hJob) CloseHandle(hJob);
        return false;
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    if (hJob) CloseHandle(hJob);
    if (outExitCode) *outExitCode = exitCode;
    return exitCode == 0;
}

bool TryConvertWordComInWorker(const std::wstring& srcCopyPath, const std::wstring& outPdfPath) {
    CLSID clsid{};
    if (FAILED(CLSIDFromProgID(L"Word.Application", &clsid))) return false;

    IDispatch* pApp = nullptr;
    if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, IID_IDispatch, reinterpret_cast<void**>(&pApp))) || !pApp) {
        return false;
    }

    bool ok = false;
    {
        std::vector<VARIANT> aVis = { ScopedVariant::MakeBool(false) };
        ComInvoke(pApp, DISPATCH_PROPERTYPUT, L"Visible", aVis);
        std::vector<VARIANT> aAlerts = { ScopedVariant::MakeInt(0) }; // wdAlertsNone
        ComInvoke(pApp, DISPATCH_PROPERTYPUT, L"DisplayAlerts", aAlerts);

        ScopedVariant vDocs;
        std::vector<VARIANT> noArgs;
        if (SUCCEEDED(ComInvoke(pApp, DISPATCH_PROPERTYGET, L"Documents", noArgs, &vDocs.v)) &&
            vDocs.v.vt == VT_DISPATCH && vDocs.v.pdispVal) {
            IDispatch* pDocs = vDocs.v.pdispVal;
            ScopedVariant vDoc;
            // Documents.Open(FileName, ConfirmConversions=False, ReadOnly=True, AddToRecentFiles=False)
            std::vector<VARIANT> openArgs = {
                ScopedVariant::MakeBstr(srcCopyPath),
                ScopedVariant::MakeBool(false),
                ScopedVariant::MakeBool(true),
                ScopedVariant::MakeBool(false)
            };
            if (SUCCEEDED(ComInvoke(pDocs, DISPATCH_METHOD, L"Open", openArgs, &vDoc.v)) &&
                vDoc.v.vt == VT_DISPATCH && vDoc.v.pdispVal) {
                IDispatch* pDoc = vDoc.v.pdispVal;
                // ExportAsFixedFormat(OutputFileName, ExportFormat=17 [wdExportFormatPDF])
                std::vector<VARIANT> expArgs = {
                    ScopedVariant::MakeBstr(outPdfPath),
                    ScopedVariant::MakeInt(17)
                };
                HRESULT hrExp = ComInvoke(pDoc, DISPATCH_METHOD, L"ExportAsFixedFormat", expArgs);
                if (FAILED(hrExp)) {
                    std::vector<VARIANT> saveArgs = {
                        ScopedVariant::MakeBstr(outPdfPath),
                        ScopedVariant::MakeInt(17)
                    };
                    hrExp = ComInvoke(pDoc, DISPATCH_METHOD, L"SaveAs2", saveArgs);
                }
                ok = SUCCEEDED(hrExp);

                std::vector<VARIANT> closeArgs = { ScopedVariant::MakeInt(0) }; // wdDoNotSaveChanges
                ComInvoke(pDoc, DISPATCH_METHOD, L"Close", closeArgs);
            }
        }
        std::vector<VARIANT> quitArgs = { ScopedVariant::MakeInt(0) };
        ComInvoke(pApp, DISPATCH_METHOD, L"Quit", quitArgs);
    }
    pApp->Release();
    std::error_code ec;
    return ok && std::filesystem::exists(outPdfPath, ec) && std::filesystem::file_size(outPdfPath, ec) > 64;
}

bool TryConvertPowerPointComInWorker(const std::wstring& srcCopyPath, const std::wstring& outPdfPath) {
    CLSID clsid{};
    if (FAILED(CLSIDFromProgID(L"PowerPoint.Application", &clsid))) return false;

    IDispatch* pApp = nullptr;
    if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, IID_IDispatch, reinterpret_cast<void**>(&pApp))) || !pApp) {
        return false;
    }

    bool ok = false;
    {
        std::vector<VARIANT> aAlerts = { ScopedVariant::MakeInt(1) }; // ppAlertsNone
        ComInvoke(pApp, DISPATCH_PROPERTYPUT, L"DisplayAlerts", aAlerts);

        ScopedVariant vPresList;
        std::vector<VARIANT> noArgs;
        if (SUCCEEDED(ComInvoke(pApp, DISPATCH_PROPERTYGET, L"Presentations", noArgs, &vPresList.v)) &&
            vPresList.v.vt == VT_DISPATCH && vPresList.v.pdispVal) {
            IDispatch* pPresList = vPresList.v.pdispVal;
            ScopedVariant vPres;
            // Presentations.Open(FileName, ReadOnly=-1 [msoTrue], Untitled=0 [msoFalse], WithWindow=0 [msoFalse])
            std::vector<VARIANT> openArgs = {
                ScopedVariant::MakeBstr(srcCopyPath),
                ScopedVariant::MakeInt(-1),
                ScopedVariant::MakeInt(0),
                ScopedVariant::MakeInt(0)
            };
            if (SUCCEEDED(ComInvoke(pPresList, DISPATCH_METHOD, L"Open", openArgs, &vPres.v)) &&
                vPres.v.vt == VT_DISPATCH && vPres.v.pdispVal) {
                IDispatch* pPres = vPres.v.pdispVal;
                // Pres.SaveAs(FileName, FileFormat=32 [ppSaveAsPDF])
                std::vector<VARIANT> saveArgs = {
                    ScopedVariant::MakeBstr(outPdfPath),
                    ScopedVariant::MakeInt(32)
                };
                HRESULT hrSave = ComInvoke(pPres, DISPATCH_METHOD, L"SaveAs", saveArgs);
                if (FAILED(hrSave)) {
                    std::vector<VARIANT> saveArgs3 = {
                        ScopedVariant::MakeBstr(outPdfPath),
                        ScopedVariant::MakeInt(32),
                        ScopedVariant::MakeInt(0)
                    };
                    hrSave = ComInvoke(pPres, DISPATCH_METHOD, L"SaveAs", saveArgs3);
                }
                ok = SUCCEEDED(hrSave);

                std::vector<VARIANT> closeArgs;
                ComInvoke(pPres, DISPATCH_METHOD, L"Close", closeArgs);
            }
        }
        std::vector<VARIANT> quitArgs;
        ComInvoke(pApp, DISPATCH_METHOD, L"Quit", quitArgs);
    }
    pApp->Release();
    std::error_code ec;
    return ok && std::filesystem::exists(outPdfPath, ec) && std::filesystem::file_size(outPdfPath, ec) > 64;
}

bool TryConvertExcelComInWorker(const std::wstring& srcCopyPath, const std::wstring& outPdfPath) {
    CLSID clsid{};
    if (FAILED(CLSIDFromProgID(L"Excel.Application", &clsid))) return false;

    IDispatch* pApp = nullptr;
    if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, IID_IDispatch, reinterpret_cast<void**>(&pApp))) || !pApp) {
        return false;
    }

    bool ok = false;
    {
        std::vector<VARIANT> aVis = { ScopedVariant::MakeBool(false) };
        ComInvoke(pApp, DISPATCH_PROPERTYPUT, L"Visible", aVis);
        std::vector<VARIANT> aAlerts = { ScopedVariant::MakeBool(false) };
        ComInvoke(pApp, DISPATCH_PROPERTYPUT, L"DisplayAlerts", aAlerts);

        ScopedVariant vWbs;
        std::vector<VARIANT> noArgs;
        if (SUCCEEDED(ComInvoke(pApp, DISPATCH_PROPERTYGET, L"Workbooks", noArgs, &vWbs.v)) &&
            vWbs.v.vt == VT_DISPATCH && vWbs.v.pdispVal) {
            IDispatch* pWbs = vWbs.v.pdispVal;
            ScopedVariant vWb;
            // Workbooks.Open(Filename, UpdateLinks=0, ReadOnly=True)
            std::vector<VARIANT> openArgs = {
                ScopedVariant::MakeBstr(srcCopyPath),
                ScopedVariant::MakeInt(0),
                ScopedVariant::MakeBool(true)
            };
            if (SUCCEEDED(ComInvoke(pWbs, DISPATCH_METHOD, L"Open", openArgs, &vWb.v)) &&
                vWb.v.vt == VT_DISPATCH && vWb.v.pdispVal) {
                IDispatch* pWb = vWb.v.pdispVal;
                // Workbook.ExportAsFixedFormat(Type=0 [xlTypePDF], Filename=outPdfPath)
                std::vector<VARIANT> expArgs = {
                    ScopedVariant::MakeInt(0),
                    ScopedVariant::MakeBstr(outPdfPath)
                };
                const HRESULT hrExp = ComInvoke(pWb, DISPATCH_METHOD, L"ExportAsFixedFormat", expArgs);
                ok = SUCCEEDED(hrExp);

                std::vector<VARIANT> closeArgs = { ScopedVariant::MakeBool(false) };
                ComInvoke(pWb, DISPATCH_METHOD, L"Close", closeArgs);
            }
        }
        std::vector<VARIANT> quitArgs;
        ComInvoke(pApp, DISPATCH_METHOD, L"Quit", quitArgs);
    }
    pApp->Release();
    std::error_code ec;
    return ok && std::filesystem::exists(outPdfPath, ec) && std::filesystem::file_size(outPdfPath, ec) > 64;
}

bool TryConvertWithMicrosoftOfficeCom(
    const std::wstring& srcCopyPath,
    const std::string& kind,
    unsigned timeoutMs,
    const std::wstring& outPdfPath,
    Report& rep)
{
    auto prom = std::make_shared<std::promise<bool>>();
    std::future<bool> fut = prom->get_future();

    const std::wstring normSrc = std::filesystem::path(srcCopyPath).make_preferred().wstring();
    const std::wstring normOut = std::filesystem::path(outPdfPath).make_preferred().wstring();
    std::thread worker([normSrc, kind, normOut, prom]() {
        const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool ok = false;
        try {
            if (kind == "docx") {
                ok = TryConvertWordComInWorker(normSrc, normOut);
            } else if (kind == "pptx") {
                ok = TryConvertPowerPointComInWorker(normSrc, normOut);
            } else if (kind == "xlsx") {
                ok = TryConvertExcelComInWorker(normSrc, normOut);
            }
        } catch (...) {
            ok = false;
        }
        if (SUCCEEDED(hrCo)) {
            CoUninitialize();
        }
        try { prom->set_value(ok); } catch (...) {}
    });

    if (fut.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready) {
        if (worker.joinable()) worker.join();
        if (fut.get()) {
            rep.converterUsedUtf8 = (kind == "docx") ? "Microsoft Word"
                                  : (kind == "pptx") ? "Microsoft PowerPoint"
                                                     : "Microsoft Excel";
            rep.visualFidelityExact = true;
            return true;
        }
    } else {
        // Worker timed out; detach so UI/caller never blocks
        worker.detach();
        rep.warningsUtf8.push_back("Tiempo de espera agotado en Microsoft Office COM; intentando alternativa.");
    }
    return false;
}

bool TryConvertWithLibreOffice(
    const std::wstring& srcCopyPath,
    unsigned timeoutMs,
    const std::wstring& outPdfPath,
    Report& rep)
{
    const wchar_t* kCandidates[] = {
        L"C:\\Program Files\\LibreOffice\\program\\soffice.exe",
        L"C:\\Program Files (x86)\\LibreOffice\\program\\soffice.exe",
        L"D:\\DevTools\\LibreOffice\\program\\soffice.exe"
    };
    std::wstring sofficePath;
    std::error_code ec;
    for (const wchar_t* c : kCandidates) {
        if (std::filesystem::exists(c, ec)) {
            sofficePath = c;
            break;
        }
    }
    if (sofficePath.empty()) return false;

    const auto tempRoot = EnsurePrintoutTempDir();
    const auto seq = NextTempSeq();
    const auto loOutDir = tempRoot / (L"lo_out_" + std::to_wstring(seq));
    const auto loProfDir = tempRoot / (L"lo_prof_" + std::to_wstring(seq));
    std::filesystem::create_directories(loOutDir, ec);
    std::filesystem::create_directories(loProfDir, ec);

    // Format isolated UserInstallation URI: file:///C:/Users/... (with %20 for spaces)
    std::wstring profUri = L"file:///";
    for (wchar_t ch : loProfDir.wstring()) {
        if (ch == L'\\') profUri.push_back(L'/');
        else if (ch == L' ') profUri.append(L"%20");
        else profUri.push_back(ch);
    }

    std::wstring cmd = L"\"" + sofficePath + L"\" --headless --norestore --nolockcheck \"-env:UserInstallation=" +
                       profUri + L"\" --convert-to pdf --outdir \"" + loOutDir.wstring() + L"\" \"" +
                       srcCopyPath + L"\"";

    const bool ranOk = RunProcessWithJobTimeout(cmd, timeoutMs);
    const auto expectedPdf = loOutDir / (std::filesystem::path(srcCopyPath).stem().wstring() + L".pdf");
    bool converted = false;
    if (ranOk && std::filesystem::exists(expectedPdf, ec) && std::filesystem::file_size(expectedPdf, ec) > 64) {
        std::filesystem::copy_file(expectedPdf, outPdfPath, std::filesystem::copy_options::overwrite_existing, ec);
        converted = !ec && std::filesystem::exists(outPdfPath, ec);
    }

    std::filesystem::remove_all(loOutDir, ec);
    std::filesystem::remove_all(loProfDir, ec);

    if (converted) {
        rep.converterUsedUtf8 = "LibreOffice";
        rep.visualFidelityExact = true;
        return true;
    }
    return false;
}

#endif // _WIN32

bool TryConvertWithNativeOpenXml(
    const std::wstring& srcCopyPath,
    const std::string& kind,
    const std::wstring& outPdfPath,
    Report& rep)
{
    DocumentModel tmpDoc;
    std::string err;
    bool imported = false;
    if (kind == "docx") {
        imported = OpenXmlInterop::ImportFromDocxFile(srcCopyPath, tmpDoc, &err);
    } else if (kind == "pptx") {
        imported = OpenXmlInterop::ImportFromPptxFile(srcCopyPath, tmpDoc, &err);
    } else if (kind == "xlsx") {
        const std::wstring ext = GetLowerExtW(srcCopyPath);
        if (ext == L".csv") {
            imported = DocStudioApp::ImportFromCsvFile(WideToUtf8(srcCopyPath), tmpDoc, &err);
        } else {
            imported = DocStudioApp::ImportFromXlsxFile(WideToUtf8(srcCopyPath), tmpDoc, &err);
        }
    }
    if (!imported) {
        rep.errorUtf8 = err.empty() ? "No se pudo importar archivo Office con el motor nativo." : err;
        return false;
    }
    if (!PdfWriter::ExportDocumentToFile(tmpDoc, outPdfPath, &err)) {
        rep.errorUtf8 = err.empty() ? "No se pudo generar PDF intermedio desde el motor nativo." : err;
        return false;
    }
    rep.converterUsedUtf8 = "Nativo";
    rep.visualFidelityExact = false;
    rep.warningsUtf8.push_back("Vista aproximada: se utiliz\xC3\xB3 el motor nativo OpenXML (sin Office/LibreOffice).");
    return true;
}

// Fast reversible Undo/Redo command for batch printout insertion (Pages or Canvas cards)
class BatchInsertPrintoutCommand final : public ICommand {
public:
    // Infinite Canvas constructor
    BatchInsertPrintoutCommand(
        DocumentMode prevMode,
        std::vector<AnnotationObject> canvasCards)
        : isCanvas_(true)
        , prevMode_(prevMode)
        , canvasCards_(std::move(canvasCards))
    {}

    // Paginated Notebook constructor
    BatchInsertPrintoutCommand(
        DocumentMode prevMode,
        std::size_t insertPos,
        std::vector<PageModel> pages)
        : isCanvas_(false)
        , prevMode_(prevMode)
        , insertPos_(insertPos)
        , pages_(std::move(pages))
    {}

    std::string Description() const override {
        return "Insertar copia impresa";
    }

    bool Execute(DocumentModel& doc) override {
        if (isCanvas_) {
            doc.SetMode(DocumentMode::InfiniteCanvas);
            for (const auto& card : canvasCards_) {
                doc.AddCanvasObject(card);
            }
            doc.BumpRevision();
            return true;
        } else {
            doc.SetMode(DocumentMode::PaginatedPdf);
            auto& pgs = doc.MutablePages();
            const std::size_t pos = (std::min)(insertPos_, pgs.size());
            pgs.insert(pgs.begin() + static_cast<std::ptrdiff_t>(pos), pages_.begin(), pages_.end());
            doc.RebuildLayout();
            doc.SetDirty(true);
            doc.BumpRevision();
            return true;
        }
    }

    bool Undo(DocumentModel& doc) override {
        if (isCanvas_) {
            for (auto it = canvasCards_.rbegin(); it != canvasCards_.rend(); ++it) {
                doc.RemoveCanvasObject(it->id);
            }
            doc.SetMode(prevMode_);
            doc.BumpRevision();
            return true;
        } else {
            auto& pgs = doc.MutablePages();
            const std::size_t pos = (std::min)(insertPos_, pgs.size());
            const std::size_t count = (std::min)(pages_.size(), pgs.size() - pos);
            if (count > 0) {
                pgs.erase(pgs.begin() + static_cast<std::ptrdiff_t>(pos),
                          pgs.begin() + static_cast<std::ptrdiff_t>(pos + count));
            }
            if (pgs.empty()) {
                PageModel blank;
                blank.pageId = doc.AllocatePageId();
                pgs.push_back(std::move(blank));
            }
            doc.SetMode(prevMode_);
            doc.RebuildLayout();
            doc.SetDirty(true);
            doc.BumpRevision();
            return true;
        }
    }

private:
    bool isCanvas_ = true;
    DocumentMode prevMode_ = DocumentMode::PaginatedPdf;
    std::size_t insertPos_ = 0;
    std::vector<AnnotationObject> canvasCards_;
    std::vector<PageModel> pages_;
};

} // namespace

std::string ComputeFileSha256Hex(const std::wstring& path) {
#ifdef _WIN32
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return {};

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0 || !hAlg) {
        CloseHandle(hFile);
        return {};
    }
    BCRYPT_HASH_HANDLE hHash = nullptr;
    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) != 0 || !hHash) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        CloseHandle(hFile);
        return {};
    }

    std::uint8_t buf[65536];
    DWORD rd = 0;
    bool ok = true;
    while (ReadFile(hFile, buf, sizeof(buf), &rd, nullptr) && rd > 0) {
        if (BCryptHashData(hHash, buf, rd, 0) != 0) {
            ok = false;
            break;
        }
    }

    std::uint8_t digest[32]{};
    if (ok && BCryptFinishHash(hHash, digest, sizeof(digest), 0) != 0) {
        ok = false;
    }

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    CloseHandle(hFile);
    if (!ok) return {};

    char hex[65]{};
    for (int i = 0; i < 32; ++i) {
        std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
    }
    return std::string(hex, 64);
#else
    (void)path;
    return {};
#endif
}

bool ConvertOfficeToPdf(
    const std::wstring& srcPath,
    ConverterChoice choice,
    unsigned timeoutMs,
    std::wstring& outPdfPath,
    Report& rep)
{
    std::error_code ec;
    if (!std::filesystem::exists(srcPath, ec)) {
        rep.errorUtf8 = "El archivo de origen no existe.";
        return false;
    }

    const std::wstring ext = GetLowerExtW(srcPath);
    const std::string kind = DetectSourceKind(ext);
    if (kind != "docx" && kind != "pptx" && kind != "xlsx") {
        rep.errorUtf8 = "Formato Office no soportado para conversi\xC3\xB3n: " + WideToUtf8(ext);
        return false;
    }

    const auto tempDir = EnsurePrintoutTempDir();
    const auto seq = NextTempSeq();
    const auto tick = static_cast<unsigned long long>(GetTickCount64());
    const std::wstring tempSrcCopy = (tempDir / (L"src_" + std::to_wstring(tick) + L"_" + std::to_wstring(seq) + ext)).wstring();
    outPdfPath = (tempDir / (L"printout_" + std::to_wstring(tick) + L"_" + std::to_wstring(seq) + L".pdf")).wstring();

    std::filesystem::copy_file(srcPath, tempSrcCopy, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec || !std::filesystem::exists(tempSrcCopy, ec)) {
        rep.errorUtf8 = "No se pudo preparar copia temporal de solo lectura.";
        return false;
    }
#ifdef _WIN32
    SetFileAttributesW(tempSrcCopy.c_str(), FILE_ATTRIBUTE_READONLY);
#endif

    auto cleanupTempSrc = [&]() {
#ifdef _WIN32
        SetFileAttributesW(tempSrcCopy.c_str(), FILE_ATTRIBUTE_NORMAL);
#endif
        std::error_code rmEc;
        std::filesystem::remove(tempSrcCopy, rmEc);
    };

    const unsigned effTimeout = (timeoutMs < 2000u) ? 2000u : timeoutMs;
    bool converted = false;

#ifdef _WIN32
    if (choice == ConverterChoice::MicrosoftOffice) {
        converted = TryConvertWithMicrosoftOfficeCom(tempSrcCopy, kind, effTimeout, outPdfPath, rep);
        if (!converted && rep.errorUtf8.empty()) {
            rep.errorUtf8 = "Microsoft Office COM no pudo convertir el documento a PDF.";
        }
    } else if (choice == ConverterChoice::LibreOffice) {
        converted = TryConvertWithLibreOffice(tempSrcCopy, effTimeout, outPdfPath, rep);
        if (!converted && rep.errorUtf8.empty()) {
            rep.errorUtf8 = "LibreOffice headless no pudo convertir el documento a PDF.";
        }
    } else if (choice == ConverterChoice::NativeEditable) {
        converted = TryConvertWithNativeOpenXml(tempSrcCopy, kind, outPdfPath, rep);
    } else {
        // Automatic cascade: Microsoft Office COM -> LibreOffice headless -> Native OpenXML
        converted = TryConvertWithMicrosoftOfficeCom(tempSrcCopy, kind, effTimeout, outPdfPath, rep);
        if (!converted) {
            converted = TryConvertWithLibreOffice(tempSrcCopy, effTimeout, outPdfPath, rep);
        }
        if (!converted) {
            converted = TryConvertWithNativeOpenXml(tempSrcCopy, kind, outPdfPath, rep);
        }
    }
#else
    (void)choice;
    (void)effTimeout;
    converted = TryConvertWithNativeOpenXml(tempSrcCopy, kind, outPdfPath, rep);
#endif

    cleanupTempSrc();
    return converted;
}

bool BuildPages(
    const std::wstring& pdfPath,
    const Options& opts,
    std::vector<PageResult>& outPages,
    Report& rep,
    const std::atomic<bool>* cancel,
    const std::function<void(std::size_t, std::size_t)>& progress)
{
    outPages.clear();
    std::error_code ec;
    if (!std::filesystem::exists(pdfPath, ec) || std::filesystem::file_size(pdfPath, ec) < 16) {
        rep.errorUtf8 = "Archivo PDF de trabajo no encontrado o vac\xC3\xADo.";
        return false;
    }

    rep.ocrEngineAvailable = !WinRtOcrAdapter::ListAvailableLanguageTags().empty();

    PdfDocument pdf;
    std::string parseErr;
    const bool parsedOk = pdf.LoadFromFile(pdfPath, &parseErr);

    int winrtTotalPages = 0;
    double w0 = 595.0, h0 = 842.0;
    std::string winrtErr;
    const bool winrtOk = WinRtPdfRenderer::GetPageCountAndSize(pdfPath, 0, winrtTotalPages, w0, h0, &winrtErr);

    const std::size_t totalPages = (std::max)(
        parsedOk ? pdf.PageCount() : std::size_t{ 0 },
        (winrtOk && winrtTotalPages > 0) ? static_cast<std::size_t>(winrtTotalPages) : std::size_t{ 0 });

    if (totalPages == 0) {
        rep.errorUtf8 = !parseErr.empty() ? parseErr : (!winrtErr.empty() ? winrtErr : "PDF inv\xC3\xA1lido o cifrado.");
        return false;
    }
    rep.totalSourcePages = totalPages;

    const int first1 = (opts.firstPage1 < 1) ? 1 : opts.firstPage1;
    const int last1 = (opts.lastPage1 <= 0 || static_cast<std::size_t>(opts.lastPage1) > totalPages)
        ? static_cast<int>(totalPages)
        : opts.lastPage1;
    if (first1 > last1 || static_cast<std::size_t>(first1) > totalPages) {
        rep.errorUtf8 = "Rango de p\xC3\xA1ginas fuera de los l\xC3\xADmites del documento (1.." + std::to_string(totalPages) + ").";
        return false;
    }

    const std::size_t countToBuild = static_cast<std::size_t>(last1 - first1 + 1);
    outPages.reserve(countToBuild);

    const int effDpi = std::clamp(opts.rasterDpi, 72, 300);
    const std::size_t maxSyncOcrPages = (countToBuild <= 250) ? 16u : 2u;
    std::size_t ocrPagesAttempted = 0;

    for (int p1 = first1; p1 <= last1; ++p1) {
        if (cancel && cancel->load(std::memory_order_acquire)) {
            rep.errorUtf8 = "Operaci\xC3\xB3n cancelada por el usuario.";
            return false;
        }
        const std::size_t idx0 = static_cast<std::size_t>(p1 - 1);
        if (progress) {
            progress(static_cast<std::size_t>(p1 - first1), countToBuild);
        }

        PageResult pr{};
        pr.sourcePageIndex0 = static_cast<int>(idx0);
        pr.widthPt = 595.0;
        pr.heightPt = 842.0;
        bool havePageBox = false;

        if (parsedOk && idx0 < pdf.PageCount()) {
            const auto& pi = pdf.PageAt(idx0);
            const DocRect box = pi.hasCropBox ? pi.cropBox : pi.mediaBox;
            if (box.w > 18.0 && box.h > 18.0) {
                pr.widthPt = box.w;
                pr.heightPt = box.h;
                havePageBox = true;
            }
            pr.text = pi.extractedTextUtf8;
            pr.runs = pi.textBoxes;
        }

        if (!havePageBox) {
            int dummyTotal = 0;
            double wrW = 0.0, wrH = 0.0;
            if (WinRtPdfRenderer::GetPageCountAndSize(pdfPath, static_cast<int>(idx0), dummyTotal, wrW, wrH) &&
                wrW > 18.0 && wrH > 18.0) {
                pr.widthPt = wrW;
                pr.heightPt = wrH;
            }
        }

        const std::size_t nonWs = CountNonWhitespace(pr.text);
        const bool needsOcr = (nonWs < 8) && opts.ocrWhenNoText && rep.ocrEngineAvailable && (ocrPagesAttempted < maxSyncOcrPages);

        if (needsOcr) {
            ++ocrPagesAttempted;
            const int targetW = std::clamp(
                static_cast<int>(std::lround((pr.widthPt / 72.0) * effDpi)),
                360, 1800);
            RasterImageBgra32 bgra{};
            if (WinRtPdfRenderer::RenderPageToBgra32(pdfPath, static_cast<int>(idx0), targetW, 0, {}, bgra, cancel)) {
                pr.pixelW = bgra.width;
                pr.pixelH = bgra.height;
                if (bgra.width > 0 && bgra.height > 0) {
                    const OcrOutput ocr = WinRtOcrAdapter::RecognizeBgra32(
                        bgra.width, bgra.height, bgra.pixels.data());
                    if (ocr.engineAvailable && CountNonWhitespace(ocr.fullTextUtf8) > 0) {
                        pr.text = ocr.fullTextUtf8;
                        pr.fromOcr = true;
                        pr.runs.clear();
                        const double invW = 1.0 / static_cast<double>((std::max)(1, bgra.width));
                        const double invH = 1.0 / static_cast<double>((std::max)(1, bgra.height));
                        for (const auto& w : ocr.words) {
                            if (w.textUtf8.empty()) continue;
                            PdfTextRunBox rb{};
                            rb.textUtf8 = w.textUtf8;
                            rb.rectNorm = {
                                std::clamp(w.pixelRect.x * invW, 0.0, 0.98),
                                std::clamp(w.pixelRect.y * invH, 0.0, 0.98),
                                std::clamp((std::max)(8.0, w.pixelRect.w) * invW, 0.01, 1.0),
                                std::clamp((std::max)(8.0, w.pixelRect.h) * invH, 0.01, 1.0)
                            };
                            pr.runs.push_back(std::move(rb));
                        }
                        ++rep.pagesWithOcr;
                    }
                }
            }
        }

        if (CountNonWhitespace(pr.text) > 0) {
            ++rep.pagesWithText;
            if (pr.runs.empty()) {
                pr.runs = SynthesizeLineRunsFromPlainText(pr.text);
            }
        }

        outPages.push_back(std::move(pr));
    }

    if (progress) {
        progress(countToBuild, countToBuild);
    }
    return !outPages.empty();
}

bool InsertIntoDocument(
    DocumentModel& doc,
    CommandStack& cmdStack,
    const std::wstring& workingPdfPath,
    const std::vector<PageResult>& pages,
    const Options& opts,
    std::size_t activePageIndex,
    DocPoint canvasOrigin,
    Report& rep)
{
    if (pages.empty()) {
        rep.errorUtf8 = "No hay p\xC3\xA1ginas para insertar.";
        return false;
    }

    // Embed the working PDF once into DocumentModel::Assets() when its size is <= 20 MB
    // (covers Office conversions and standard PDFs portably inside .cudoc, while large
    // 30-100 MB textbooks stream directly from workingPdfPath via PageRenderCache without
    // duplicating 50+ MB into .cudoc RAM).
    std::string sharedPdfAssetKey;
    {
        std::error_code ecSize;
        const auto fsz = std::filesystem::file_size(std::filesystem::path(workingPdfPath), ecSize);
        if (!ecSize && fsz > 0 && fsz <= 20ull * 1024ull * 1024ull) {
            std::vector<std::uint8_t> pdfBytes;
            if (ReadFileBytes(workingPdfPath, pdfBytes) && !pdfBytes.empty()) {
                std::uint64_t h = 14695981039346656037ull;
                const std::size_t step = (pdfBytes.size() > 65536u) ? (pdfBytes.size() / 16384u) : 1u;
                for (std::size_t bi = 0; bi < pdfBytes.size(); bi += step) {
                    h ^= pdfBytes[bi];
                    h *= 1099511628211ull;
                }
                h ^= static_cast<std::uint64_t>(pdfBytes.size());
                h *= 1099511628211ull;
                char hex[40]{};
                std::snprintf(hex, sizeof(hex), "src_pdf_%016llx", static_cast<unsigned long long>(h));
                sharedPdfAssetKey = hex;
                if (!doc.FindAsset(sharedPdfAssetKey)) {
                    doc.SetNamedAsset(sharedPdfAssetKey, std::move(pdfBytes));
                }
            }
        }
    }

    const std::string pdfPathUtf8 = WideToUtf8(workingPdfPath);
    const DocumentMode prevMode = doc.Mode();

    if (opts.target == Target::NotebookPages) {
        std::vector<PageModel> newPages;
        newPages.reserve(pages.size());
        for (const auto& pr : pages) {
            PageModel pm{};
            pm.pageId = doc.AllocatePageId();
            pm.widthPt = pr.widthPt;
            pm.heightPt = pr.heightPt;
            pm.background = PaperPattern::Plain;
            pm.hasPdfSource = true;
            pm.sourcePdfPathUtf8 = pdfPathUtf8;
            pm.sourcePdfAssetKey = sharedPdfAssetKey;
            pm.sourcePageIndex0 = pr.sourcePageIndex0;
            if (pr.fromOcr) {
                pm.ocrTextUtf8 = pr.text;
            } else {
                pm.extractedTextUtf8 = pr.text;
            }
            pm.textBoxes = pr.runs;
            newPages.push_back(std::move(pm));
        }

        const std::size_t insertPos = doc.Pages().empty()
            ? 0u
            : (std::min)(activePageIndex + 1, doc.Pages().size());

        auto cmd = std::make_unique<BatchInsertPrintoutCommand>(prevMode, insertPos, std::move(newPages));
        if (!cmdStack.Execute(doc, std::move(cmd))) {
            rep.errorUtf8 = "Error al insertar p\xC3\xA1ginas en el cuaderno.";
            return false;
        }
        rep.pagesInserted = pages.size();
        return true;
    }

    // Target::InfiniteCanvas -> build pinned PdfCard objects
    std::vector<AnnotationObject> cards;
    cards.reserve(pages.size());

    const int cols = (opts.layout == CanvasLayout::Grid) ? (std::max)(1, opts.gridColumns) : 1;
    const double colGapPt = std::clamp(opts.colGapPt, 0.0, 4000.0);
    const double rowGapPt = std::clamp(opts.pageGapPt, 0.0, 4000.0);

    double curX = canvasOrigin.x;
    double curY = canvasOrigin.y;
    double rowMaxH = 0.0;
    int colIdx = 0;

    for (std::size_t i = 0; i < pages.size(); ++i) {
        const auto& pr = pages[i];
        AnnotationObject card{};
        card.id = doc.AllocateObjectId();
        card.zOrder = doc.AllocateZOrder();
        card.kind = ObjectKind::PdfCard;
        card.layerId = doc.ActiveLayerId();

        // Preserve exact page/slide aspect ratio in world points
        const double dispW = (pr.widthPt >= pr.heightPt) ? 640.0 : 520.0;
        const double dispH = dispW * (pr.heightPt / (std::max)(18.0, pr.widthPt));
        card.bounds = { curX, curY, dispW, dispH };

        if (!pr.png.empty()) {
            card.assetKey = doc.RegisterAsset(pr.png);
            card.imagePixelW = pr.pixelW;
            card.imagePixelH = pr.pixelH;
        }
        card.pdfSourcePathUtf8 = pdfPathUtf8;
        card.pdfSourceAssetKey = sharedPdfAssetKey;
        card.pdfSourcePageIndex0 = pr.sourcePageIndex0;
        card.printoutTextUtf8 = pr.text;
        card.printoutTextRuns = pr.runs;
        card.printoutTextFromOcr = pr.fromOcr;
        card.printoutSourceKindUtf8 = rep.sourceKindUtf8;
        card.printoutSourceNameUtf8 = rep.sourceNameUtf8;
        card.lockedInPlace = opts.lockInPlace;
        card.captionUtf8 = (rep.sourceNameUtf8.empty() ? std::string("Documento") : rep.sourceNameUtf8) +
                           " \xC2\xB7 P\xC3\xA1g. " + std::to_string(pr.sourcePageIndex0 + 1);

        cards.push_back(std::move(card));

        rowMaxH = (std::max)(rowMaxH, dispH);
        ++colIdx;
        if (colIdx >= cols) {
            colIdx = 0;
            curX = canvasOrigin.x;
            curY += rowMaxH + rowGapPt;
            rowMaxH = 0.0;
        } else {
            curX += dispW + colGapPt;
        }
    }

    auto cmd = std::make_unique<BatchInsertPrintoutCommand>(prevMode, std::move(cards));
    if (!cmdStack.Execute(doc, std::move(cmd))) {
        rep.errorUtf8 = "Error al insertar tarjetas impresas en el Lienzo Infinito.";
        return false;
    }
    rep.pagesInserted = pages.size();
    return true;
}

bool InsertPrintoutFromFile(
    DocumentModel& doc,
    CommandStack& cmdStack,
    const std::wstring& srcPath,
    const Options& opts,
    std::size_t activePageIndex,
    DocPoint canvasOrigin,
    Report& rep,
    const std::atomic<bool>* cancel,
    const std::function<void(std::size_t, std::size_t)>& progress)
{
    rep = Report{};
    std::error_code ec;
    if (!std::filesystem::exists(srcPath, ec)) {
        rep.errorUtf8 = "Archivo no encontrado: " + WideToUtf8(srcPath);
        return false;
    }

    rep.sourceNameUtf8 = WideToUtf8(std::filesystem::path(srcPath).filename().wstring());
    const std::wstring ext = GetLowerExtW(srcPath);
    rep.sourceKindUtf8 = DetectSourceKind(ext);
    if (rep.sourceKindUtf8 == "unknown") {
        rep.errorUtf8 = "Formato no soportado para copia impresa (use .pdf, .docx o .pptx).";
        return false;
    }

    rep.sourceSha256Before = ComputeFileSha256Hex(srcPath);

    std::wstring workingPdfPath;
    if (rep.sourceKindUtf8 == "pdf") {
        workingPdfPath = srcPath;
        rep.converterUsedUtf8 = "PDF";
        rep.visualFidelityExact = true;
    } else {
        if (!ConvertOfficeToPdf(srcPath, opts.converter, opts.officeTimeoutMs, workingPdfPath, rep)) {
            rep.sourceSha256After = ComputeFileSha256Hex(srcPath);
            return false;
        }
    }

    std::vector<PageResult> builtPages;
    const bool builtOk = BuildPages(workingPdfPath, opts, builtPages, rep, cancel, progress);
    if (!builtOk) {
        rep.sourceSha256After = ComputeFileSha256Hex(srcPath);
        return false;
    }

    const bool insertedOk = InsertIntoDocument(
        doc, cmdStack, workingPdfPath, builtPages, opts, activePageIndex, canvasOrigin, rep);

    rep.sourceSha256After = ComputeFileSha256Hex(srcPath);
    if (!rep.sourceSha256Before.empty() && rep.sourceSha256Before != rep.sourceSha256After) {
        rep.errorUtf8 = "Verificaci\xC3\xB3n de integridad fallida: el hash SHA-256 del archivo fuente cambi\xC3\xB3.";
        return false;
    }
    return insertedOk;
}

} // namespace DocStudio::Printout
