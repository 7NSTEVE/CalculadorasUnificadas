#include "pdf_signer.hpp"
#include "pdf_parser.hpp"
#include "pdf_writer.hpp"
#include "../core/utf_conv.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <functional>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>

namespace DocStudio {

namespace {

constexpr std::size_t kSigReserveBytes = 8192;

std::string EscapePdfLit(const std::string& s) {
    std::string out = "(";
    for (unsigned char c : s) {
        if (c == '(' || c == ')' || c == '\\') out.push_back('\\');
        if (c >= 0x20 && c <= 0x7Eu) out.push_back(static_cast<char>(c));
        else {
            char b[8]{};
            std::snprintf(b, sizeof(b), "\\%03o", c);
            out += b;
        }
    }
    out.push_back(')');
    return out;
}

std::string HexEncodeUpper(const std::uint8_t* data, std::size_t len) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(len * 2);
    for (std::size_t i = 0; i < len; ++i) {
        out.push_back(kHex[(data[i] >> 4) & 0x0Fu]);
        out.push_back(kHex[data[i] & 0x0Fu]);
    }
    return out;
}

bool HexDecode(const std::string& hex, std::vector<std::uint8_t>& out) {
    out.clear();
    int hi = -1;
    for (unsigned char ch : hex) {
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') continue;
        int v = -1;
        if (ch >= '0' && ch <= '9') v = ch - '0';
        else if (ch >= 'a' && ch <= 'f') v = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F') v = ch - 'A' + 10;
        else return false;
        if (hi < 0) hi = v;
        else {
            out.push_back(static_cast<std::uint8_t>((hi << 4) | v));
            hi = -1;
        }
    }
    if (hi >= 0) out.push_back(static_cast<std::uint8_t>(hi << 4));
    return !out.empty();
}

// Trims zero-padding after ASN.1 SEQUENCE header length
std::size_t ComputeDerTlvLength(const std::uint8_t* data, std::size_t maxLen) noexcept {
    if (!data || maxLen < 2 || data[0] != 0x30u) return maxLen;
    if ((data[1] & 0x80u) == 0) {
        const std::size_t total = 2u + data[1];
        return (total <= maxLen) ? total : maxLen;
    }
    const std::size_t lenBytes = data[1] & 0x7Fu;
    if (lenBytes == 0 || lenBytes > 4 || 2u + lenBytes > maxLen) return maxLen;
    std::size_t bodyLen = 0;
    for (std::size_t i = 0; i < lenBytes; ++i) {
        bodyLen = (bodyLen << 8) | data[2u + i];
    }
    const std::size_t total = 2u + lenBytes + bodyLen;
    return (total <= maxLen) ? total : maxLen;
}

bool SignTwoByteRangesPkcs7(
    PCCERT_CONTEXT cert,
    const std::uint8_t* part1,
    std::size_t len1,
    const std::uint8_t* part2,
    std::size_t len2,
    std::vector<std::uint8_t>& outDerSig,
    std::string* outErr)
{
    if (!cert) {
        if (outErr) *outErr = "Certificado X.509 nulo.";
        return false;
    }
    CRYPT_SIGN_MESSAGE_PARA signPara{};
    signPara.cbSize = sizeof(signPara);
    signPara.dwMsgEncodingType = PKCS_7_ASN_ENCODING | X509_ASN_ENCODING;
    signPara.pSigningCert = cert;
    signPara.cMsgCert = 1;
    signPara.rgpMsgCert = &cert;
    signPara.HashAlgorithm.pszObjId = const_cast<LPSTR>(szOID_NIST_sha256);

    const BYTE* rgpb[2] = { part1, part2 };
    DWORD rgcb[2] = { static_cast<DWORD>(len1), static_cast<DWORD>(len2) };

    DWORD cbSigned = 0;
    if (!CryptSignMessage(&signPara, TRUE /* fDetachedSignature */, 2, rgpb, rgcb, nullptr, &cbSigned) || cbSigned == 0) {
        if (outErr) *outErr = "CryptSignMessage (tama\xC3\xB1o) fall\xC3\xB3 (WinError " + std::to_string(GetLastError()) + ").";
        return false;
    }
    outDerSig.resize(cbSigned);
    if (!CryptSignMessage(&signPara, TRUE, 2, rgpb, rgcb, outDerSig.data(), &cbSigned)) {
        if (outErr) *outErr = "CryptSignMessage fall\xC3\xB3 (WinError " + std::to_string(GetLastError()) + ").";
        return false;
    }
    outDerSig.resize(cbSigned);
    return true;
}

bool BuildSignedPdfFromCleanBytes(
    const std::vector<std::uint8_t>& inputPdfBytes,
    PCCERT_CONTEXT cert,
    const PdfSignatureOptions& opts,
    std::vector<std::uint8_t>& outSignedPdfBytes,
    std::string* outErr)
{
    // Re-serialize inputPdfBytes via PdfDocument + PdfWriter cleanly or append AcroForm/Sig as incremental or rebuilt clean PDF.
    // Let's inspect inputPdfBytes via PdfDocument so we can clone every object cleanly and append /AcroForm + /Sig dictionary
    // with exact deterministic ByteRange & Contents offsets!
    PdfDocument src;
    if (!src.LoadFromBytes(inputPdfBytes, outErr) || src.PageCount() == 0) {
        return false;
    }

    // Build clean PDF body copying all existing pages + appending Signature widget on target page
    // Wait: we can preserve inputPdfBytes verbatim and simply locate root/first page OR rebuild cleanly.
    // Incremental update preserves original signatures too; rebuilding cleanly avoids compressed XRef complications!
    // Let's do a clean rebuild via PdfValue remapping so ByteRange & classic xref are 100% deterministic.
    std::string pdf = "%PDF-1.7\n%\xE2\xE3\xCF\xD3\n";
    struct ObjRec { std::uint32_t id; std::string body; };
    std::vector<ObjRec> objs;
    auto allocId = [&]() -> std::uint32_t {
        objs.push_back({ static_cast<std::uint32_t>(objs.size() + 1), {} });
        return objs.back().id;
    };
    std::unordered_map<std::uint32_t, std::uint32_t> remap;
    std::function<std::uint32_t(std::uint32_t, int)> cloneObj;
    std::function<PdfValue(const PdfValue&, int)> rewriteVal;

    rewriteVal = [&](const PdfValue& v, int depth) -> PdfValue {
        PdfValue o = v;
        if (v.kind == PdfValue::Kind::Ref) {
            o.refVal = { cloneObj(v.refVal.obj, depth + 1), 0 };
        } else if (v.kind == PdfValue::Kind::Array) {
            for (auto& e : o.arrayVal) e = rewriteVal(e, depth);
        } else if (v.kind == PdfValue::Kind::Dict) {
            for (auto& kv : o.dictVal) kv.second = rewriteVal(kv.second, depth);
        } else if (v.kind == PdfValue::Kind::Stream && v.streamVal) {
            auto sd = std::make_shared<PdfStreamData>();
            sd->rawBytes = v.streamVal->rawBytes;
            for (const auto& kv : v.streamVal->dict) sd->dict[kv.first] = rewriteVal(kv.second, depth);
            o.streamVal = std::move(sd);
        }
        return o;
    };

    cloneObj = [&](std::uint32_t srcId, int depth) -> std::uint32_t {
        auto it = remap.find(srcId);
        if (it != remap.end()) return it->second;
        if (depth > 48) return 0;
        const std::uint32_t nid = allocId();
        remap[srcId] = nid;
        PdfValue val = src.LookupObject(srcId);
        PdfValue rw = rewriteVal(val, depth + 1);
        objs[nid - 1].body = PdfDocument::SerializeValue(rw);
        return nid;
    };

    const std::uint32_t pagesRootId = allocId();
    const std::uint32_t sigObjId = allocId();
    const std::uint32_t widgetObjId = allocId();
    const std::size_t targetPageIdx = (std::min)(opts.visualPageIndex0, src.PageCount() - 1);

    std::vector<std::uint32_t> pageIds;
    std::uint32_t targetPageNewId = 0;

    for (std::size_t i = 0; i < src.PageCount(); ++i) {
        const auto& pi = src.PageAt(i);
        PdfValue pageVal = src.LookupObject(pi.pageRef.obj);
        PdfValue rwPage = rewriteVal(pageVal, 1);
        PdfDict pd = rwPage.dictVal;

        PdfValue pRef;
        pRef.kind = PdfValue::Kind::Ref;
        pRef.refVal = { pagesRootId, 0 };
        pd["Parent"] = pRef;

        const std::uint32_t pid = allocId();
        if (i == targetPageIdx) {
            targetPageNewId = pid;
            PdfValue wRef;
            wRef.kind = PdfValue::Kind::Ref;
            wRef.refVal = { widgetObjId, 0 };
            auto aIt = pd.find("Annots");
            if (aIt != pd.end() && aIt->second.IsArray()) {
                aIt->second.arrayVal.push_back(wRef);
            } else {
                PdfValue arr;
                arr.kind = PdfValue::Kind::Array;
                arr.arrayVal.push_back(wRef);
                pd["Annots"] = std::move(arr);
            }
        }
        PdfValue dv;
        dv.kind = PdfValue::Kind::Dict;
        dv.dictVal = std::move(pd);
        objs[pid - 1].body = PdfDocument::SerializeValue(dv);
        pageIds.push_back(pid);
    }

    // Pages root
    {
        std::ostringstream ps;
        ps << "<</Type /Pages /Count " << pageIds.size() << " /Kids [";
        for (std::size_t i = 0; i < pageIds.size(); ++i) {
            if (i > 0) ps << ' ';
            ps << pageIds[i] << " 0 R";
        }
        ps << "]>>";
        objs[pagesRootId - 1].body = ps.str();
    }

    // Signature Widget Annotation + optional visual appearance stream (/AP /N)
    std::uint32_t apFormId = 0;
    if (opts.addVisualStamp) {
        apFormId = allocId();
        const double w = (std::max)(24.0, opts.visualRectPt.w);
        const double h = (std::max)(18.0, opts.visualRectPt.h);
        std::ostringstream stm;
        stm << "q\n"
            << "0.925 0.957 1.0 rg 0.078 0.282 0.627 RG 1.5 w\n"
            << "0 0 " << w << " " << h << " re B\n"
            << "BT /F1 9 Tf 0.078 0.282 0.627 rg 1 0 0 1 8 " << (h - 16.0) << " Tm "
            << EscapePdfLit("Firmado digitalmente:") << " Tj ET\n"
            << "BT /F2 9.5 Tf 0.05 0.12 0.25 rg 1 0 0 1 8 " << (h - 30.0) << " Tm "
            << EscapePdfLit(opts.signerNameUtf8) << " Tj ET\n"
            << "BT /F1 8 Tf 0.22 0.30 0.42 rg 1 0 0 1 8 10 Tm "
            << EscapePdfLit(opts.reasonUtf8) << " Tj ET\n"
            << "Q\n";
        const std::string bodyBytes = stm.str();
        std::ostringstream fobj;
        fobj << "<</Type /XObject /Subtype /Form /BBox [0 0 " << w << " " << h << "] "
             << "/Resources <</Font <</F1 <</Type /Font /Subtype /Type1 /BaseFont /Helvetica>> "
             << "/F2 <</Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold>>>>>> "
             << "/Length " << bodyBytes.size() << ">>\nstream\n"
             << bodyBytes << "endstream";
        objs[apFormId - 1].body = fobj.str();
    }
    {
        const double x0 = opts.visualRectPt.x;
        const double y0 = opts.visualRectPt.y;
        const double x1 = x0 + opts.visualRectPt.w;
        const double y1 = y0 + opts.visualRectPt.h;
        std::ostringstream ws;
        ws << "<</Type /Annot /Subtype /Widget /FT /Sig /T (DocStudioSig1) "
           << "/Rect [" << x0 << ' ' << y0 << ' ' << x1 << ' ' << y1 << "] "
           << "/P " << targetPageNewId << " 0 R /V " << sigObjId << " 0 R /F 132";
        if (apFormId != 0) ws << " /AP <</N " << apFormId << " 0 R>>";
        ws << ">>";
        objs[widgetObjId - 1].body = ws.str();
    }

    // Placeholder tokens inside /Sig object so ByteRange & Contents lengths remain immutable after patching
    static constexpr const char* kByteRangePlaceholder = "[0 0000000000 0000000000 0000000000]";
    const std::string zeroHex(kSigReserveBytes * 2u, '0');
    {
        std::ostringstream ss;
        ss << "<</Type /Sig /Filter /Adobe.PPKLite /SubFilter /adbe.pkcs7.detached "
           << "/Name " << EscapePdfLit(opts.signerNameUtf8) << " "
           << "/Reason " << EscapePdfLit(opts.reasonUtf8) << " "
           << "/Location " << EscapePdfLit(opts.locationUtf8) << " "
           << "/ByteRange " << kByteRangePlaceholder << " "
           << "/Contents <" << zeroHex << ">>>";
        objs[sigObjId - 1].body = ss.str();
    }

    const std::uint32_t catalogId = allocId();
    {
        std::ostringstream cs;
        cs << "<</Type /Catalog /Pages " << pagesRootId << " 0 R "
           << "/AcroForm <</Fields [" << widgetObjId << " 0 R] /SigFlags 3>>>>";
        objs[catalogId - 1].body = cs.str();
    }

    // Write out objects & record offsets
    std::vector<std::size_t> offsets(objs.size() + 1, 0);
    std::size_t brTokenPos = std::string::npos;
    std::size_t contentsLtPos = std::string::npos;
    std::size_t contentsGtPos = std::string::npos;

    for (const auto& obj : objs) {
        offsets[obj.id] = pdf.size();
        const std::size_t bodyBase = pdf.size() + std::to_string(obj.id).size() + 7; // "<id> 0 obj\n"
        pdf += std::to_string(obj.id) + " 0 obj\n";
        pdf += obj.body;
        if (obj.id == sigObjId) {
            const std::size_t brRel = obj.body.find(kByteRangePlaceholder);
            const std::size_t cntRel = obj.body.find("/Contents <");
            if (brRel != std::string::npos) brTokenPos = bodyBase + brRel;
            if (cntRel != std::string::npos) {
                contentsLtPos = bodyBase + cntRel + 10; // index of '<'
                contentsGtPos = contentsLtPos + 1 + zeroHex.size() + 1; // one past '>'
            }
        }
        pdf += "\nendobj\n";
    }

    const std::size_t xrefOff = pdf.size();
    pdf += "xref\n0 " + std::to_string(objs.size() + 1) + "\n";
    pdf += "0000000000 65535 f \n";
    for (std::size_t i = 1; i <= objs.size(); ++i) {
        char line[32]{};
        std::snprintf(line, sizeof(line), "%010zu 00000 n \n", offsets[i]);
        pdf += line;
    }
    pdf += "trailer\n<</Size " + std::to_string(objs.size() + 1) + " /Root " + std::to_string(catalogId) + " 0 R>>\n";
    pdf += "startxref\n" + std::to_string(xrefOff) + "\n%%EOF\n";

    if (brTokenPos == std::string::npos || contentsLtPos == std::string::npos || contentsGtPos > pdf.size()) {
        if (outErr) *outErr = "Error interno localizando /ByteRange o /Contents.";
        return false;
    }

    // Patch /ByteRange preserving exact character count of kByteRangePlaceholder
    char brBuf[64]{};
    const std::size_t part2Len = pdf.size() - contentsGtPos;
    std::snprintf(brBuf, sizeof(brBuf), "[0 %10zu %10zu %10zu]", contentsLtPos, contentsGtPos, part2Len);
    const std::string brFixed(brBuf);
    if (brFixed.size() != std::strlen(kByteRangePlaceholder)) {
        if (outErr) *outErr = "Desbordamiento formateando /ByteRange.";
        return false;
    }
    std::memcpy(&pdf[brTokenPos], brFixed.data(), brFixed.size());

    // Compute detached PKCS#7 signature over the two ranges [0 .. contentsLtPos) and [contentsGtPos .. pdf.size())
    std::vector<std::uint8_t> derSig;
    if (!SignTwoByteRangesPkcs7(
            cert,
            reinterpret_cast<const std::uint8_t*>(pdf.data()),
            contentsLtPos,
            reinterpret_cast<const std::uint8_t*>(pdf.data() + contentsGtPos),
            part2Len,
            derSig,
            outErr)) {
        return false;
    }
    if (derSig.size() > kSigReserveBytes) {
        if (outErr) *outErr = "La firma PKCS#7 excede el bloque reservado.";
        return false;
    }

    const std::string sigHex = HexEncodeUpper(derSig.data(), derSig.size());
    std::memcpy(&pdf[contentsLtPos + 1], sigHex.data(), sigHex.size());

    outSignedPdfBytes.assign(pdf.begin(), pdf.end());
    return true;
}

bool ReadBytesFromFile(const std::wstring& path, std::vector<std::uint8_t>& out, std::string* outErr) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (outErr) *outErr = "No se pudo abrir archivo.";
        return false;
    }
    LARGE_INTEGER li{};
    if (!GetFileSizeEx(hFile, &li) || li.QuadPart < 0 || li.QuadPart > 512ll * 1024ll * 1024ll) {
        CloseHandle(hFile);
        return false;
    }
    out.resize(static_cast<std::size_t>(li.QuadPart));
    std::size_t off = 0;
    while (off < out.size()) {
        const DWORD chunk = static_cast<DWORD>((std::min<std::size_t>)(out.size() - off, 4u * 1024u * 1024u));
        DWORD rd = 0;
        if (!ReadFile(hFile, out.data() + off, chunk, &rd, nullptr) || rd == 0) {
            CloseHandle(hFile);
            return false;
        }
        off += rd;
    }
    CloseHandle(hFile);
    return true;
}

std::string CertNameToUtf8(PCCERT_CONTEXT cert, DWORD dwType) {
    if (!cert) return {};
    const DWORD n = CertGetNameStringW(cert, dwType, 0, nullptr, nullptr, 0);
    if (n <= 1) return {};
    std::wstring buf(n, L'\0');
    CertGetNameStringW(cert, dwType, 0, nullptr, buf.data(), n);
    if (!buf.empty() && buf.back() == L'\0') buf.pop_back();
    return WideToUtf8(buf);
}

} // namespace

bool PdfSigner::SignPdfBytesWithPfx(
    const std::vector<std::uint8_t>& inputPdfBytes,
    const std::wstring& pfxPath,
    const std::wstring& pfxPassword,
    const PdfSignatureOptions& opts,
    std::vector<std::uint8_t>& outSignedPdfBytes,
    std::string* outErr)
{
    std::vector<std::uint8_t> pfxBytes;
    if (!ReadBytesFromFile(pfxPath, pfxBytes, outErr)) {
        if (outErr && outErr->empty()) *outErr = "No se pudo leer archivo .pfx.";
        return false;
    }

    CRYPT_DATA_BLOB blob{};
    blob.cbData = static_cast<DWORD>(pfxBytes.size());
    blob.pbData = pfxBytes.data();
    HCERTSTORE hStore = PFXImportCertStore(&blob, pfxPassword.c_str(), CRYPT_EXPORTABLE);
    if (!hStore) {
        if (outErr) *outErr = "No se pudo importar archivo .pfx (contrase\xC3\xB1" "a incorrecta o formato inv\xC3\xA1lido).";
        return false;
    }

    PCCERT_CONTEXT chosen = nullptr;
    PCCERT_CONTEXT iter = nullptr;
    while ((iter = CertEnumCertificatesInStore(hStore, iter)) != nullptr) {
        DWORD sz = 0;
        if (CertGetCertificateContextProperty(iter, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &sz)) {
            chosen = CertDuplicateCertificateContext(iter);
            CertFreeCertificateContext(iter);
            break;
        }
    }
    if (!chosen) {
        CertCloseStore(hStore, 0);
        if (outErr) *outErr = "El archivo .pfx no contiene un certificado con clave privada.";
        return false;
    }

    const bool ok = BuildSignedPdfFromCleanBytes(inputPdfBytes, chosen, opts, outSignedPdfBytes, outErr);
    CertFreeCertificateContext(chosen);
    CertCloseStore(hStore, 0);
    return ok;
}

bool PdfSigner::SignPdfBytesWithSelfSigned(
    const std::vector<std::uint8_t>& inputPdfBytes,
    const PdfSignatureOptions& opts,
    std::vector<std::uint8_t>& outSignedPdfBytes,
    std::string* outErr)
{
    static std::atomic<std::uint64_t> seq{ 0 };
    const std::wstring containerName = L"DocStudioTmpSig_" + std::to_wstring(GetCurrentProcessId()) +
        L"_" + std::to_wstring(++seq);

    HCRYPTPROV hProv = 0;
    if (!CryptAcquireContextW(&hProv, containerName.c_str(), MS_ENH_RSA_AES_PROV_W, PROV_RSA_AES, CRYPT_NEWKEYSET | CRYPT_SILENT)) {
        if (outErr) *outErr = "CryptAcquireContextW fall\xC3\xB3 (WinError " + std::to_string(GetLastError()) + ").";
        return false;
    }

    HCRYPTKEY hKey = 0;
    if (!CryptGenKey(hProv, AT_SIGNATURE, (2048u << 16) | CRYPT_EXPORTABLE, &hKey)) {
        CryptReleaseContext(hProv, 0);
        HCRYPTPROV hDel = 0;
        CryptAcquireContextW(&hDel, containerName.c_str(), MS_ENH_RSA_AES_PROV_W, PROV_RSA_AES, CRYPT_DELETEKEYSET | CRYPT_SILENT);
        if (outErr) *outErr = "CryptGenKey RSA-2048 fall\xC3\xB3.";
        return false;
    }

    const std::string x500Utf8 = "CN=" + (opts.signerNameUtf8.empty() ? std::string("Document Studio") : opts.signerNameUtf8);
    const std::wstring x500W = Utf8ToWide(x500Utf8);
    DWORD cbName = 0;
    CertStrToNameW(X509_ASN_ENCODING, x500W.c_str(), CERT_X500_NAME_STR, nullptr, nullptr, &cbName, nullptr);
    std::vector<BYTE> encodedName(cbName);
    CertStrToNameW(X509_ASN_ENCODING, x500W.c_str(), CERT_X500_NAME_STR, nullptr, encodedName.data(), &cbName, nullptr);

    CERT_NAME_BLOB sib{ cbName, encodedName.data() };
    CRYPT_KEY_PROV_INFO kpi{};
    kpi.pwszContainerName = const_cast<LPWSTR>(containerName.c_str());
    kpi.pwszProvName = const_cast<LPWSTR>(MS_ENH_RSA_AES_PROV_W);
    kpi.dwProvType = PROV_RSA_AES;
    kpi.dwFlags = CRYPT_SILENT;
    kpi.dwKeySpec = AT_SIGNATURE;

    CRYPT_ALGORITHM_IDENTIFIER sigAlg{};
    sigAlg.pszObjId = const_cast<LPSTR>(szOID_RSA_SHA256RSA);

    SYSTEMTIME stStart{}, stEnd{};
    GetSystemTime(&stStart);
    stEnd = stStart;
    stEnd.wYear += 5;

    PCCERT_CONTEXT cert = CertCreateSelfSignCertificate(hProv, &sib, 0, &kpi, &sigAlg, &stStart, &stEnd, nullptr);
    bool ok = false;
    if (!cert) {
        if (outErr) *outErr = "CertCreateSelfSignCertificate fall\xC3\xB3 (WinError " + std::to_string(GetLastError()) + ").";
    } else {
        ok = BuildSignedPdfFromCleanBytes(inputPdfBytes, cert, opts, outSignedPdfBytes, outErr);
        CertFreeCertificateContext(cert);
    }

    CryptDestroyKey(hKey);
    CryptReleaseContext(hProv, 0);
    // Clean up temporary persisted RSA key container so keystore stays clean
    HCRYPTPROV hDel = 0;
    CryptAcquireContextW(&hDel, containerName.c_str(), MS_ENH_RSA_AES_PROV_W, PROV_RSA_AES, CRYPT_DELETEKEYSET | CRYPT_SILENT);
    return ok;
}

bool PdfSigner::ExportAndSignDocument(
    const DocumentModel& doc,
    const PdfSignatureOptions& opts,
    const std::wstring& outPdfPath,
    const std::wstring& optionalPfxPath,
    const std::wstring& optionalPfxPassword,
    std::string* outErr)
{
    std::vector<std::uint8_t> basePdf;
    if (doc.Mode() == DocumentMode::InfiniteCanvas) {
        CanvasPdfExportOptions copts;
        if (!PdfWriter::ExportInfiniteCanvasToBytes(doc, copts, basePdf, outErr)) return false;
    } else {
        if (!PdfWriter::ExportDocumentToBytes(doc, basePdf, outErr)) return false;
    }

    std::vector<std::uint8_t> signedPdf;
    const bool signedOk = !optionalPfxPath.empty()
        ? SignPdfBytesWithPfx(basePdf, optionalPfxPath, optionalPfxPassword, opts, signedPdf, outErr)
        : SignPdfBytesWithSelfSigned(basePdf, opts, signedPdf, outErr);
    if (!signedOk) return false;

    HANDLE hFile = CreateFileW(outPdfPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (outErr) *outErr = "No se pudo escribir PDF firmado.";
        return false;
    }
    DWORD wr = 0;
    const BOOL wrOk = WriteFile(hFile, signedPdf.data(), static_cast<DWORD>(signedPdf.size()), &wr, nullptr);
    FlushFileBuffers(hFile);
    CloseHandle(hFile);
    return wrOk && wr == signedPdf.size();
}

PdfSignatureVerifyResult PdfSigner::VerifyPdfBytes(const std::vector<std::uint8_t>& pdfBytes) {
    PdfSignatureVerifyResult res;
    if (pdfBytes.empty()) {
        res.diagnosticUtf8 = "Archivo PDF vac\xC3\xADo.";
        return res;
    }

    const std::string text(reinterpret_cast<const char*>(pdfBytes.data()), pdfBytes.size());
    const std::size_t sigPos = text.rfind("/SubFilter /adbe.pkcs7.detached");
    if (sigPos == std::string::npos) {
        res.diagnosticUtf8 = "El PDF no contiene diccionario de firma PKCS#7 /adbe.pkcs7.detached.";
        return res;
    }
    res.hasSignature = true;

    const std::size_t dictStart = text.rfind("<<", sigPos);
    const std::size_t brPos = text.find("/ByteRange", dictStart != std::string::npos ? dictStart : sigPos);
    const std::size_t cntPos = text.find("/Contents", dictStart != std::string::npos ? dictStart : sigPos);
    if (brPos == std::string::npos || cntPos == std::string::npos) {
        res.diagnosticUtf8 = "Diccionario /Sig incompleto (falta /ByteRange o /Contents).";
        return res;
    }

    const std::size_t brL = text.find('[', brPos);
    const std::size_t brR = text.find(']', brL != std::string::npos ? brL : brPos);
    const std::size_t cntL = text.find('<', cntPos);
    const std::size_t cntR = text.find('>', cntL != std::string::npos ? cntL : cntPos);
    if (brL == std::string::npos || brR == std::string::npos || cntL == std::string::npos || cntR == std::string::npos) {
        res.diagnosticUtf8 = "Sintaxis inv\xC3\xA1lida en /ByteRange o /Contents.";
        return res;
    }

    std::istringstream brIss(text.substr(brL + 1, brR - brL - 1));
    std::uint64_t v = 0;
    while (brIss >> v) res.byteRange.push_back(v);
    if (res.byteRange.size() != 4) {
        res.diagnosticUtf8 = "/ByteRange debe tener exactamente 4 enteros.";
        return res;
    }

    const std::uint64_t r0 = res.byteRange[0];
    const std::uint64_t l0 = res.byteRange[1];
    const std::uint64_t r1 = res.byteRange[2];
    const std::uint64_t l1 = res.byteRange[3];
    if (r0 != 0 || l0 != cntL || r1 != cntR + 1 || r1 + l1 != pdfBytes.size()) {
        res.byteRangeValid = false;
        res.diagnosticUtf8 = "El /ByteRange no coincide con los l\xC3\xADmites reales de /Contents y el archivo (posible truncado o alteraci\xC3\xB3n).";
        return res;
    }
    res.byteRangeValid = true;

    std::vector<std::uint8_t> derSig;
    if (!HexDecode(text.substr(cntL + 1, cntR - cntL - 1), derSig)) {
        res.diagnosticUtf8 = "/Contents hexadecimal inv\xC3\xA1lido.";
        return res;
    }
    const std::size_t exactDerLen = ComputeDerTlvLength(derSig.data(), derSig.size());
    derSig.resize(exactDerLen);

    const BYTE* rgpb[2] = { pdfBytes.data() + r0, pdfBytes.data() + r1 };
    DWORD rgcb[2] = { static_cast<DWORD>(l0), static_cast<DWORD>(l1) };

    CRYPT_VERIFY_MESSAGE_PARA vPara{};
    vPara.cbSize = sizeof(vPara);
    vPara.dwMsgAndCertEncodingType = PKCS_7_ASN_ENCODING | X509_ASN_ENCODING;

    PCCERT_CONTEXT signerCert = nullptr;
    if (!CryptVerifyDetachedMessageSignature(&vPara, 0, derSig.data(), static_cast<DWORD>(derSig.size()), 2, rgpb, rgcb, &signerCert)) {
        res.cryptographicValid = false;
        res.diagnosticUtf8 = "Firma PKCS#7 INV\xC3\x81LIDA: el contenido del PDF fue modificado despu\xC3\xA9s de firmarlo (WinError " + std::to_string(GetLastError()) + ").";
        if (signerCert) CertFreeCertificateContext(signerCert);
        return res;
    }

    res.cryptographicValid = true;
    if (signerCert) {
        res.signerSubjectUtf8 = CertNameToUtf8(signerCert, CERT_NAME_SIMPLE_DISPLAY_TYPE);

        CERT_CHAIN_PARA chainPara{};
        chainPara.cbSize = sizeof(chainPara);
        PCCERT_CHAIN_CONTEXT chainCtx = nullptr;
        if (CertGetCertificateChain(nullptr, signerCert, nullptr, signerCert->hCertStore, &chainPara, 0, nullptr, &chainCtx) && chainCtx) {
            res.trustedBySystemRoot = (chainCtx->TrustStatus.dwErrorStatus == CERT_TRUST_NO_ERROR);
            CertFreeCertificateChain(chainCtx);
        }
        CertFreeCertificateContext(signerCert);
    }

    res.diagnosticUtf8 = res.trustedBySystemRoot
        ? "Firma PKCS#7 SHA-256 v\xC3\xA1lida y verificada contra entidad de confianza del sistema."
        : "Firma PKCS#7 SHA-256 criptogr\xC3\xA1" "ficamente \xC3\xADntegra (certificado autofirmado o no presente en ra\xC3\xADz del sistema).";
    return res;
}

PdfSignatureVerifyResult PdfSigner::VerifyPdfFile(const std::wstring& pdfPath) {
    std::vector<std::uint8_t> bytes;
    std::string err;
    if (!ReadBytesFromFile(pdfPath, bytes, &err)) {
        PdfSignatureVerifyResult r;
        r.diagnosticUtf8 = err.empty() ? "No se pudo leer PDF." : err;
        return r;
    }
    return VerifyPdfBytes(bytes);
}

} // namespace DocStudio
