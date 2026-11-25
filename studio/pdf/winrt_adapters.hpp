#pragma once

#include "../core/doc_types.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio {

struct RasterImageBgra32 {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels; // width * height * 4 (BGRA8 premultiplied / opaque white bg)

    bool Valid() const noexcept {
        return width > 0 && height > 0 && pixels.size() == static_cast<std::size_t>(width) * height * 4u;
    }

    std::size_t ByteSize() const noexcept {
        return pixels.size();
    }
};

struct OcrWordBox {
    std::string textUtf8;
    DocRect pixelRect{};
};

struct OcrOutput {
    bool engineAvailable = false;
    std::string languageTagUtf8;
    std::string fullTextUtf8;
    std::vector<std::string> linesUtf8;
    std::vector<OcrWordBox> words;
    std::string errorUtf8;
};

// Thread-safe Windows.Data.Pdf renderer producing BGRA32 buffers with crop & delta-rotation support.
class WinRtPdfRenderer {
public:
    static bool GetPageCountAndSize(
        const std::wstring& pdfPath,
        int pageIndex0,
        int& outPageCount,
        double& outWidthPt,
        double& outHeightPt,
        std::string* outErr = nullptr);

    static bool RenderPageToBgra32(
        const std::wstring& pdfPath,
        int pageIndex0,
        int targetWidthPx,
        int deltaRotationDeg,
        const NormalizedCropRect& crop,
        RasterImageBgra32& outImage,
        const std::atomic<bool>* cancelFlag = nullptr,
        std::string* outErr = nullptr);

    static bool EncodeBgra32ToPngBytes(
        const RasterImageBgra32& img,
        std::vector<std::uint8_t>& outPngBytes);

    static void ClearOpenDocumentCache();
};

// Thread-safe offline OCR adapter using Windows.Media.Ocr over BGRA32 buffers.
class WinRtOcrAdapter {
public:
    static std::vector<std::string> ListAvailableLanguageTags();

    static OcrOutput RecognizeBgra32(
        int width,
        int height,
        const std::uint8_t* bgraBytes,
        const std::string& preferredLangTag = "");
};

struct HtrOutput {
    bool recognized = false;
    bool usedNativeInkRecognizer = false;
    std::string textUtf8;
    std::string mathLatexUtf8;
    std::vector<std::string> candidatesUtf8;
    std::string errorUtf8;
};

// Native offline Windows Handwriting Text Recognition (HTR) (F-043, F-045, F-169):
// Uses Windows.UI.Input.Inking.InkRecognizerContainer when available, falling back cleanly
// to high-contrast stroke rasterization + Windows.Media.Ocr offline.
class WinRtInkHtrAdapter {
public:
    static HtrOutput RecognizeStrokes(
        const std::vector<AnnotationObject>& strokes,
        const std::string& preferredLangTag = "");
};

} // namespace DocStudio
