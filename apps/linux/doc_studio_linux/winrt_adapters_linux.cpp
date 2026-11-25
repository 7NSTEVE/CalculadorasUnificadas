#include "pdf/winrt_adapters.hpp"
#include "core/utf_conv.hpp"

#include <cairo.h>
#include <glib.h>
#include <poppler.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <list>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace DocStudio {

namespace {

std::string NormalizeAbsUtf8Path(const std::wstring& wpath) {
    const std::string utf8 = WideToUtf8(wpath);
    if (utf8.empty()) return {};
    std::error_code ec;
    const std::filesystem::path raw(utf8);
    std::filesystem::path abs = std::filesystem::absolute(raw, ec);
    if (ec) abs = raw;
    return abs.lexically_normal().string();
}

struct CachedPopplerDoc {
    std::string key;
    std::uint64_t fileSize = 0;
    std::int64_t mtimeSec = 0;
    std::int64_t mtimeNsec = 0;
    PopplerDocument* doc = nullptr;
    std::shared_ptr<std::mutex> docMutex;

    ~CachedPopplerDoc() {
        if (doc) {
            g_object_unref(doc);
            doc = nullptr;
        }
    }
};

class PopplerDocCache {
public:
    static constexpr std::size_t kMaxCachedDocs = 16;

    static PopplerDocCache& Instance() {
        static PopplerDocCache inst;
        return inst;
    }

    std::shared_ptr<CachedPopplerDoc> Acquire(const std::wstring& pdfPath, std::string* outErr) {
        const std::string absPath = NormalizeAbsUtf8Path(pdfPath);
        if (absPath.empty()) {
            if (outErr) *outErr = "Ruta PDF vacia.";
            return nullptr;
        }

        struct stat st{};
        if (::stat(absPath.c_str(), &st) != 0) {
            if (outErr) *outErr = "No se pudo acceder al archivo PDF (" + std::string(std::strerror(errno)) + ").";
            return nullptr;
        }

        const std::uint64_t sz = static_cast<std::uint64_t>(st.st_size);
        const std::int64_t msec = static_cast<std::int64_t>(st.st_mtim.tv_sec);
        const std::int64_t mnsec = static_cast<std::int64_t>(st.st_mtim.tv_nsec);

        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = map_.find(absPath);
            if (it != map_.end()) {
                const auto& entry = *(it->second);
                if (entry->doc && entry->fileSize == sz && entry->mtimeSec == msec && entry->mtimeNsec == mnsec) {
                    lru_.splice(lru_.begin(), lru_, it->second);
                    return entry;
                }
                lru_.erase(it->second);
                map_.erase(it);
            }
        }

        GError* gerr = nullptr;
        gchar* uri = g_filename_to_uri(absPath.c_str(), nullptr, &gerr);
        if (!uri) {
            if (outErr) {
                *outErr = gerr && gerr->message ? gerr->message : "g_filename_to_uri fallo.";
            }
            if (gerr) g_error_free(gerr);
            return nullptr;
        }

        PopplerDocument* rawDoc = nullptr;
        {
            // Serialize poppler_document_new_from_file briefly in case GLib type init runs concurrently.
            std::lock_guard<std::mutex> openLk(openMtx_);
            rawDoc = poppler_document_new_from_file(uri, nullptr, &gerr);
        }
        g_free(uri);

        if (!rawDoc) {
            if (outErr) {
                *outErr = gerr && gerr->message ? gerr->message : "Poppler no pudo abrir el PDF.";
            }
            if (gerr) g_error_free(gerr);
            return nullptr;
        }
        if (gerr) g_error_free(gerr);

        auto entry = std::make_shared<CachedPopplerDoc>();
        entry->key = absPath;
        entry->fileSize = sz;
        entry->mtimeSec = msec;
        entry->mtimeNsec = mnsec;
        entry->doc = rawDoc;
        entry->docMutex = std::make_shared<std::mutex>();

        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto oldIt = map_.find(absPath);
            if (oldIt != map_.end()) {
                lru_.erase(oldIt->second);
                map_.erase(oldIt);
            }
            lru_.push_front(entry);
            map_[absPath] = lru_.begin();
            while (lru_.size() > kMaxCachedDocs) {
                auto back = lru_.back();
                map_.erase(back->key);
                lru_.pop_back();
            }
        }
        return entry;
    }

    void Clear() {
        std::lock_guard<std::mutex> lk(mtx_);
        map_.clear();
        lru_.clear();
    }

private:
    PopplerDocCache() = default;
    std::mutex mtx_;
    std::mutex openMtx_;
    std::list<std::shared_ptr<CachedPopplerDoc>> lru_;
    std::unordered_map<std::string, std::list<std::shared_ptr<CachedPopplerDoc>>::iterator> map_;
};

void ApplyCropAndDeltaRotationBgra32(
    const std::uint8_t* src,
    int srcW,
    int srcH,
    int srcStride,
    const NormalizedCropRect& cropIn,
    int deltaRotDeg,
    RasterImageBgra32& out)
{
    NormalizedCropRect crop = cropIn;
    crop.Clamp();
    const int cx0 = std::clamp(static_cast<int>(std::floor(crop.left * srcW)), 0, srcW - 1);
    const int cy0 = std::clamp(static_cast<int>(std::floor(crop.top * srcH)), 0, srcH - 1);
    const int cx1 = std::clamp(static_cast<int>(std::ceil(crop.right * srcW)), cx0 + 1, srcW);
    const int cy1 = std::clamp(static_cast<int>(std::ceil(crop.bottom * srcH)), cy0 + 1, srcH);
    const int cw = cx1 - cx0;
    const int ch = cy1 - cy0;
    const int r = ((deltaRotDeg % 360) + 360) % 360;

    if (r == 0 && cx0 == 0 && cy0 == 0 && cw == srcW && ch == srcH) {
        out.width = srcW;
        out.height = srcH;
        const std::size_t rowBytes = static_cast<std::size_t>(srcW) * 4u;
        out.pixels.resize(rowBytes * static_cast<std::size_t>(srcH));
        for (int y = 0; y < srcH; ++y) {
            std::memcpy(
                out.pixels.data() + static_cast<std::size_t>(y) * rowBytes,
                src + static_cast<std::size_t>(y) * static_cast<std::size_t>(srcStride),
                rowBytes);
        }
        return;
    }

    out.width = (r == 90 || r == 270) ? ch : cw;
    out.height = (r == 90 || r == 270) ? cw : ch;
    out.pixels.resize(static_cast<std::size_t>(out.width) * static_cast<std::size_t>(out.height) * 4u);

    for (int y = 0; y < ch; ++y) {
        const std::uint8_t* srcRow = src + static_cast<std::size_t>(cy0 + y) * static_cast<std::size_t>(srcStride);
        for (int x = 0; x < cw; ++x) {
            int dx = x, dy = y;
            if (r == 90) { dx = ch - 1 - y; dy = x; }
            else if (r == 180) { dx = cw - 1 - x; dy = ch - 1 - y; }
            else if (r == 270) { dx = y; dy = cw - 1 - x; }
            const std::uint8_t* sp = srcRow + static_cast<std::size_t>(cx0 + x) * 4u;
            std::uint8_t* dp = out.pixels.data() + (static_cast<std::size_t>(dy) * static_cast<std::size_t>(out.width) + static_cast<std::size_t>(dx)) * 4u;
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
            dp[3] = sp[3];
        }
    }
}

cairo_status_t CairoVectorWriteFunc(void* closure, const unsigned char* data, unsigned int length) {
    auto* vec = static_cast<std::vector<std::uint8_t>*>(closure);
    if (!vec) return CAIRO_STATUS_WRITE_ERROR;
    vec->insert(vec->end(), data, data + length);
    return CAIRO_STATUS_SUCCESS;
}

bool EncodeRawBgra32ToPng(int w, int h, const std::uint8_t* bgraBytes, std::vector<std::uint8_t>& outPng) {
    outPng.clear();
    if (w <= 0 || h <= 0 || !bgraBytes) return false;
    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (!surf || cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        if (surf) cairo_surface_destroy(surf);
        return false;
    }
    cairo_surface_flush(surf);
    unsigned char* dst = cairo_image_surface_get_data(surf);
    const int dstStride = cairo_image_surface_get_stride(surf);
    const std::size_t srcRowBytes = static_cast<std::size_t>(w) * 4u;
    for (int y = 0; y < h; ++y) {
        std::memcpy(
            dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(dstStride),
            bgraBytes + static_cast<std::size_t>(y) * srcRowBytes,
            srcRowBytes);
    }
    cairo_surface_mark_dirty(surf);
    const cairo_status_t st = cairo_surface_write_to_png_stream(surf, CairoVectorWriteFunc, &outPng);
    cairo_surface_destroy(surf);
    return st == CAIRO_STATUS_SUCCESS && !outPng.empty();
}

std::string ToLowerAscii(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string TrimAscii(const std::string& s) {
    std::size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    std::size_t b = s.size();
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

bool RunChildExecCaptureStdout(const char* exePath, const std::vector<const char*>& argv, std::string& outStdout, int& outExitCode) {
    outStdout.clear();
    outExitCode = -1;
    if (!exePath || ::access(exePath, X_OK) != 0) return false;

    int pipeFds[2]{ -1, -1 };
    if (::pipe(pipeFds) != 0) return false;

    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(pipeFds[0]);
        ::close(pipeFds[1]);
        return false;
    }
    if (pid == 0) {
        ::close(pipeFds[0]);
        ::dup2(pipeFds[1], STDOUT_FILENO);
        ::close(pipeFds[1]);
        // Silence stderr noise from tesseract banners
        const int devNull = ::open("/dev/null", O_WRONLY);
        if (devNull >= 0) {
            ::dup2(devNull, STDERR_FILENO);
            ::close(devNull);
        }
        std::vector<char*> args;
        args.reserve(argv.size() + 1);
        for (const char* a : argv) args.push_back(const_cast<char*>(a));
        args.push_back(nullptr);
        ::execv(exePath, args.data());
        ::_exit(127);
    }

    ::close(pipeFds[1]);
    char buf[4096];
    for (;;) {
        const ssize_t n = ::read(pipeFds[0], buf, sizeof(buf));
        if (n > 0) {
            outStdout.append(buf, static_cast<std::size_t>(n));
        } else if (n == 0) {
            break;
        } else {
            if (errno == EINTR) continue;
            break;
        }
    }
    ::close(pipeFds[0]);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (WIFEXITED(status)) {
        outExitCode = WEXITSTATUS(status);
        return true;
    }
    return false;
}

} // namespace

bool WinRtPdfRenderer::GetPageCountAndSize(
    const std::wstring& pdfPath,
    int pageIndex0,
    int& outPageCount,
    double& outWidthPt,
    double& outHeightPt,
    std::string* outErr)
{
    auto entry = PopplerDocCache::Instance().Acquire(pdfPath, outErr);
    if (!entry || !entry->doc || !entry->docMutex) return false;

    std::lock_guard<std::mutex> docLk(*entry->docMutex);
    const int nPages = poppler_document_get_n_pages(entry->doc);
    outPageCount = nPages;
    if (pageIndex0 < 0 || pageIndex0 >= nPages) {
        if (outErr) *outErr = "Indice de pagina PDF fuera de rango.";
        return false;
    }

    PopplerPage* page = poppler_document_get_page(entry->doc, pageIndex0);
    if (!page) {
        if (outErr) *outErr = "poppler_document_get_page fallo.";
        return false;
    }
    double w = 0.0, h = 0.0;
    poppler_page_get_size(page, &w, &h);
    g_object_unref(page);

    if (w <= 0.0 || h <= 0.0) {
        if (outErr) *outErr = "Tamano de pagina PDF invalido.";
        return false;
    }
    outWidthPt = w;
    outHeightPt = h;
    return true;
}

bool WinRtPdfRenderer::RenderPageToBgra32(
    const std::wstring& pdfPath,
    int pageIndex0,
    int targetWidthPx,
    int deltaRotationDeg,
    const NormalizedCropRect& crop,
    RasterImageBgra32& outImage,
    const std::atomic<bool>* cancelFlag,
    std::string* outErr)
{
    if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) return false;

    auto entry = PopplerDocCache::Instance().Acquire(pdfPath, outErr);
    if (!entry || !entry->doc || !entry->docMutex) return false;

    int reqW = 0;
    int reqH = 0;
    cairo_surface_t* surf = nullptr;
    {
        std::lock_guard<std::mutex> docLk(*entry->docMutex);
        if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) return false;

        const int nPages = poppler_document_get_n_pages(entry->doc);
        if (pageIndex0 < 0 || pageIndex0 >= nPages) {
            if (outErr) *outErr = "Indice de pagina PDF fuera de rango.";
            return false;
        }

        PopplerPage* page = poppler_document_get_page(entry->doc, pageIndex0);
        if (!page) {
            if (outErr) *outErr = "poppler_document_get_page fallo.";
            return false;
        }

        double ptW = 595.0, ptH = 842.0;
        poppler_page_get_size(page, &ptW, &ptH);
        if (ptW < 1.0) ptW = 595.0;
        if (ptH < 1.0) ptH = 842.0;
        const double aspect = ptH / ptW;

        reqW = std::clamp(targetWidthPx, 48, 3000);
        reqH = (std::max)(48, static_cast<int>(std::lround(reqW * aspect)));
        if (reqH > 3800) {
            reqH = 3800;
            reqW = (std::max)(48, static_cast<int>(std::lround(reqH / aspect)));
        }
        constexpr std::uint64_t kMaxPageBytes = 42ull * 1024ull * 1024ull;
        const std::uint64_t estBytes = static_cast<std::uint64_t>(reqW) * static_cast<std::uint64_t>(reqH) * 4ull;
        if (estBytes > kMaxPageBytes) {
            const double f = std::sqrt(static_cast<double>(kMaxPageBytes) / static_cast<double>(estBytes));
            reqW = (std::max)(48, static_cast<int>(reqW * f));
            reqH = (std::max)(48, static_cast<int>(reqH * f));
        }

        surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, reqW, reqH);
        if (!surf || cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
            if (surf) cairo_surface_destroy(surf);
            g_object_unref(page);
            if (outErr) *outErr = "No se pudo crear cairo_image_surface.";
            return false;
        }

        cairo_t* cr = cairo_create(surf);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        cairo_paint(cr);
        cairo_scale(cr, static_cast<double>(reqW) / ptW, static_cast<double>(reqH) / ptH);
        poppler_page_render(page, cr);
        cairo_destroy(cr);
        g_object_unref(page);
    }

    if (cancelFlag && cancelFlag->load(std::memory_order_acquire)) {
        cairo_surface_destroy(surf);
        return false;
    }

    cairo_surface_flush(surf);
    const unsigned char* srcData = cairo_image_surface_get_data(surf);
    const int srcStride = cairo_image_surface_get_stride(surf);
    if (!srcData || srcStride < reqW * 4) {
        cairo_surface_destroy(surf);
        if (outErr) *outErr = "Buffer Cairo invalido tras poppler_page_render.";
        return false;
    }

    ApplyCropAndDeltaRotationBgra32(srcData, reqW, reqH, srcStride, crop, deltaRotationDeg, outImage);
    cairo_surface_destroy(surf);
    return outImage.Valid();
}

bool WinRtPdfRenderer::EncodeBgra32ToPngBytes(
    const RasterImageBgra32& img,
    std::vector<std::uint8_t>& outPngBytes)
{
    if (!img.Valid()) return false;
    return EncodeRawBgra32ToPng(img.width, img.height, img.pixels.data(), outPngBytes);
}

void WinRtPdfRenderer::ClearOpenDocumentCache() {
    PopplerDocCache::Instance().Clear();
}

std::vector<std::string> WinRtOcrAdapter::ListAvailableLanguageTags() {
    static constexpr const char* kTesseractBin = "/usr/bin/tesseract";
    std::string stdoutTxt;
    int exitCode = -1;
    std::vector<std::string> langs;
    if (!RunChildExecCaptureStdout(kTesseractBin, { kTesseractBin, "--list-langs" }, stdoutTxt, exitCode) || exitCode != 0) {
        return langs;
    }
    std::istringstream iss(stdoutTxt);
    std::string line;
    bool firstLine = true;
    while (std::getline(iss, line)) {
        const std::string t = TrimAscii(line);
        if (t.empty()) continue;
        if (firstLine) {
            firstLine = false;
            if (t.find("List of available") != std::string::npos) continue;
        }
        if (t == "osd") continue;
        langs.push_back(t);
    }
    return langs;
}

OcrOutput WinRtOcrAdapter::RecognizeBgra32(
    int width,
    int height,
    const std::uint8_t* bgraBytes,
    const std::string& preferredLangTag)
{
    OcrOutput out;
    static constexpr const char* kTesseractBin = "/usr/bin/tesseract";

    if (width <= 0 || height <= 0 || width > 8192 || height > 8192 || !bgraBytes) {
        out.errorUtf8 = "Buffer de imagen invalido para OCR.";
        return out;
    }

    if (::access(kTesseractBin, X_OK) != 0) {
        out.engineAvailable = false;
        out.errorUtf8 = "Motor OCR Tesseract no encontrado en /usr/bin/tesseract.";
        return out;
    }

    const std::vector<std::string> available = ListAvailableLanguageTags();
    if (available.empty()) {
        out.engineAvailable = false;
        out.errorUtf8 = "No hay idiomas instalados para Tesseract.";
        return out;
    }
    out.engineAvailable = true;

    auto pickLang = [&]() -> std::string {
        const std::string pref = ToLowerAscii(TrimAscii(preferredLangTag));
        auto hasLang = [&](const std::string& code) -> bool {
            return std::find(available.begin(), available.end(), code) != available.end();
        };
        if (!pref.empty()) {
            if (hasLang(pref)) return pref;
            if ((pref == "en" || pref.rfind("en-", 0) == 0 || pref.rfind("en_", 0) == 0) && hasLang("eng")) return "eng";
            if ((pref == "es" || pref.rfind("es-", 0) == 0 || pref.rfind("es_", 0) == 0) && hasLang("spa")) return "spa";
            if ((pref == "fr" || pref.rfind("fr-", 0) == 0) && hasLang("fra")) return "fra";
            if ((pref == "de" || pref.rfind("de-", 0) == 0) && hasLang("deu")) return "deu";
            if ((pref == "pt" || pref.rfind("pt-", 0) == 0) && hasLang("por")) return "por";
        }
        if (hasLang("eng")) return "eng";
        return available.front();
    };
    const std::string lang = pickLang();
    out.languageTagUtf8 = lang;

    std::vector<std::uint8_t> pngBytes;
    if (!EncodeRawBgra32ToPng(width, height, bgraBytes, pngBytes)) {
        out.errorUtf8 = "Fallo codificando PNG temporal para OCR.";
        return out;
    }

    char tmpl[] = "/tmp/docstudio_ocr_XXXXXX.png";
    const int fd = ::mkstemps(tmpl, 4);
    if (fd < 0) {
        out.errorUtf8 = "No se pudo crear archivo temporal para OCR.";
        return out;
    }
    struct TempFileUnlink {
        std::string path;
        ~TempFileUnlink() {
            if (!path.empty()) ::unlink(path.c_str());
        }
    } tempGuard{ tmpl };

    std::size_t written = 0;
    bool writeOk = true;
    while (written < pngBytes.size()) {
        const ssize_t n = ::write(fd, pngBytes.data() + written, pngBytes.size() - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            writeOk = false;
            break;
        }
        written += static_cast<std::size_t>(n);
    }
    ::close(fd);
    if (!writeOk) {
        out.errorUtf8 = "Fallo escribiendo imagen PNG temporal para OCR.";
        return out;
    }

    std::string tsvOut;
    int exitCode = -1;
    const std::vector<const char*> args = {
        kTesseractBin,
        tmpl,
        "stdout",
        "-l",
        lang.c_str(),
        "--psm",
        "3",
        "tsv"
    };
    if (!RunChildExecCaptureStdout(kTesseractBin, args, tsvOut, exitCode) || exitCode != 0) {
        out.errorUtf8 = "Tesseract fallo al procesar la imagen (exit " + std::to_string(exitCode) + ").";
        return out;
    }

    std::istringstream iss(tsvOut);
    std::string line;
    std::string currentLineKey;
    std::string currentLineText;
    bool headerSkipped = false;

    auto flushLine = [&]() {
        if (!currentLineText.empty()) {
            out.linesUtf8.push_back(currentLineText);
            currentLineText.clear();
        }
    };

    while (std::getline(iss, line)) {
        if (!headerSkipped) {
            headerSkipped = true;
            if (line.rfind("level\t", 0) == 0) continue;
        }
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();

        std::vector<std::string> cols;
        std::size_t start = 0;
        for (int col = 0; col < 11; ++col) {
            const std::size_t tab = line.find('\t', start);
            if (tab == std::string::npos) break;
            cols.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }
        cols.push_back(start <= line.size() ? line.substr(start) : std::string());
        if (cols.size() < 12) continue;

        if (cols[0] != "5") continue; // level 5 = word
        const std::string wordText = TrimAscii(cols[11]);
        if (wordText.empty()) continue;

        OcrWordBox wb;
        wb.textUtf8 = wordText;
        wb.pixelRect = {
            std::atof(cols[6].c_str()),
            std::atof(cols[7].c_str()),
            std::atof(cols[8].c_str()),
            std::atof(cols[9].c_str())
        };
        out.words.push_back(wb);

        const std::string lkey = cols[1] + ":" + cols[2] + ":" + cols[3] + ":" + cols[4];
        if (!currentLineKey.empty() && lkey != currentLineKey) {
            flushLine();
        }
        currentLineKey = lkey;
        if (!currentLineText.empty()) currentLineText.push_back(' ');
        currentLineText += wordText;
    }
    flushLine();

    for (std::size_t i = 0; i < out.linesUtf8.size(); ++i) {
        if (i > 0) out.fullTextUtf8.push_back('\n');
        out.fullTextUtf8 += out.linesUtf8[i];
    }
    return out;
}

} // namespace DocStudio
