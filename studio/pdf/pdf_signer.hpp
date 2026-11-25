#pragma once

#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio {

struct PdfSignatureOptions {
    std::string signerNameUtf8 = "Calculadora Unificada";
    std::string reasonUtf8 = "Aprobado con Document Studio";
    std::string locationUtf8 = "Local";
    bool addVisualStamp = true;
    std::size_t visualPageIndex0 = 0;
    DocRect visualRectPt{ 360.0, 720.0, 200.0, 72.0 };
};

struct PdfSignatureVerifyResult {
    bool hasSignature = false;
    bool byteRangeValid = false;
    bool cryptographicValid = false;
    bool trustedBySystemRoot = false;
    std::string signerSubjectUtf8;
    std::string reasonUtf8;
    std::string locationUtf8;
    std::vector<std::uint64_t> byteRange;
    std::string diagnosticUtf8;
};

class PdfSigner {
public:
    // Signs existing PDF bytes using a PKCS#12 (.pfx) certificate file & password.
    static bool SignPdfBytesWithPfx(
        const std::vector<std::uint8_t>& inputPdfBytes,
        const std::wstring& pfxPath,
        const std::wstring& pfxPassword,
        const PdfSignatureOptions& opts,
        std::vector<std::uint8_t>& outSignedPdfBytes,
        std::string* outErr = nullptr);

    // Signs existing PDF bytes with a freshly generated RSA-2048 SHA-256 self-signed X.509 cert (in-memory cleanup).
    static bool SignPdfBytesWithSelfSigned(
        const std::vector<std::uint8_t>& inputPdfBytes,
        const PdfSignatureOptions& opts,
        std::vector<std::uint8_t>& outSignedPdfBytes,
        std::string* outErr = nullptr);

    // Signs DocumentModel directly (optionally adds SignatureStamp visual appearance, exports PDF, then applies PKCS#7 seal).
    static bool ExportAndSignDocument(
        const DocumentModel& doc,
        const PdfSignatureOptions& opts,
        const std::wstring& outPdfPath,
        const std::wstring& optionalPfxPath = L"",
        const std::wstring& optionalPfxPassword = L"",
        std::string* outErr = nullptr);

    // Verifies /ByteRange geometry and PKCS#7 detached cryptographic signature against the PDF bytes.
    static PdfSignatureVerifyResult VerifyPdfBytes(const std::vector<std::uint8_t>& pdfBytes);
    static PdfSignatureVerifyResult VerifyPdfFile(const std::wstring& pdfPath);
};

} // namespace DocStudio
