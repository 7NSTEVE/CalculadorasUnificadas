#include "pdf/pdf_signer.hpp"
#include "pdf/pdf_parser.hpp"
#include "pdf/pdf_writer.hpp"
#include "core/utf_conv.hpp"

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>
#include <openssl/safestack.h>
#include <openssl/x509.h>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio {

namespace {

constexpr std::size_t kSigReserveBytes = 8192;

struct BioFree {
    void operator()(BIO* b) const noexcept { if (b) BIO_free_all(b); }
};
struct X509Free {
    void operator()(X509* x) const noexcept { if (x) X509_free(x); }
};
struct EvpPkeyFree {
    void operator()(EVP_PKEY* k) const noexcept { if (k) EVP_PKEY_free(k); }
};
struct CmsFree {
    void operator()(CMS_ContentInfo* c) const noexcept { if (c) CMS_ContentInfo_free(c); }
};
struct Pkcs12Free {
    void operator()(PKCS12* p) const noexcept { if (p) PKCS12_free(p); }
};
struct X509StoreFree {
    void operator()(X509_STORE* s) const noexcept { if (s) X509_STORE_free(s); }
};
struct StackX509Free {
    void operator()(STACK_OF(X509)* st) const noexcept { if (st) sk_X509_pop_free(st, X509_free); }
};
struct StackX509ShallowFree {
    void operator()(STACK_OF(X509)* st) const noexcept { if (st) sk_X509_free(st); }
};

using BioPtr = std::unique_ptr<BIO, BioFree>;
using X509Ptr = std::unique_ptr<X509, X509Free>;
using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, EvpPkeyFree>;
using CmsPtr = std::unique_ptr<CMS_ContentInfo, CmsFree>;
using Pkcs12Ptr = std::unique_ptr<PKCS12, Pkcs12Free>;
using X509StorePtr = std::unique_ptr<X509_STORE, X509StoreFree>;
using StackX509Ptr = std::unique_ptr<STACK_OF(X509), StackX509Free>;
using StackX509ShallowPtr = std::unique_ptr<STACK_OF(X509), StackX509ShallowFree>;

std::string LastOpenSslError() {
    std::string msg;
    unsigned long code = 0;
    while ((code = ERR_get_error()) != 0) {
        char buf[256]{};
        ERR_error_string_n(code, buf, sizeof(buf));
        if (!msg.empty()) msg += "; ";
        msg += buf;
    }
    return msg;
}

std::string EscapePdfLit(const std::string& s) {
    std::string out = "(";
    for (unsigned char c : s) {
        if (c == '(' || c == ')' || c == '\\') out.push_back('\\');
        if (c >= 0x20 && c <= 0x7Eu) {
            out.push_back(static_cast<char>(c));
        } else {
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
    out.reserve(len * 2u);
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
        else if (ch >= 'A' && ch <= 'F') v = ch - 'A' + 10;
        else if (ch >= 'a' && ch <= 'f') v = ch - 'a' + 10;
        else return false;
        if (hi < 0) {
            hi = v;
        } else {
            out.push_back(static_cast<std::uint8_t>((hi << 4) | v));
            hi = -1;
        }
    }
    return hi < 0;
}

std::size_t ComputeDerTlvLength(const std::uint8_t* p, std::size_t avail) {
    if (avail < 2 || p[0] != 0x30u) return avail;
    const std::uint8_t lenByte = p[1];
    if ((lenByte & 0x80u) == 0u) {
        const std::size_t total = 2u + lenByte;
        return (std::min)(avail, total);
    }
    const std::size_t numLenBytes = lenByte & 0x7Fu;
    if (numLenBytes == 0 || numLenBytes > 4 || 2u + numLenBytes > avail) return avail;
    std::size_t bodyLen = 0;
    for (std::size_t i = 0; i < numLenBytes; ++i) {
        bodyLen = (bodyLen << 8) | p[2u + i];
    }
    const std::size_t total = 2u + numLenBytes + bodyLen;
    return (std::min)(avail, total);
}

bool WriteChunkedToBio(BIO* bio, const std::uint8_t* data, std::size_t len) {
    std::size_t off = 0;
    while (off < len) {
        const int chunk = static_cast<int>((std::min<std::size_t>)(len - off, 64u * 1024u * 1024u));
        const int wr = BIO_write(bio, data + off, chunk);
        if (wr <= 0) return false;
        off += static_cast<std::size_t>(wr);
    }
    return true;
}

BioPtr CreateBioForTwoRanges(
    const std::uint8_t* range1, std::size_t len1,
    const std::uint8_t* range2, std::size_t len2)
{
    BioPtr bio(BIO_new(BIO_s_mem()));
    if (!bio) return nullptr;
    if (len1 > 0 && !WriteChunkedToBio(bio.get(), range1, len1)) return nullptr;
    if (len2 > 0 && !WriteChunkedToBio(bio.get(), range2, len2)) return nullptr;
    return bio;
}

bool SignTwoByteRangesPkcs7(
    X509* signerCert,
    EVP_PKEY* signerKey,
    STACK_OF(X509)* extraCaCerts,
    const std::uint8_t* range1, std::size_t len1,
    const std::uint8_t* range2, std::size_t len2,
    std::vector<std::uint8_t>& outDerSig,
    std::string* outErr)
{
    ERR_clear_error();
    BioPtr dataBio = CreateBioForTwoRanges(range1, len1, range2, len2);
    if (!dataBio) {
        if (outErr) *outErr = "No se pudo preparar buffer BIO para firma CMS.";
        return false;
    }

    const unsigned int flags = CMS_DETACHED | CMS_BINARY;
    CmsPtr cms(CMS_sign(nullptr, nullptr, extraCaCerts, nullptr, flags | CMS_PARTIAL));
    if (!cms) {
        if (outErr) *outErr = "CMS_sign fallo: " + LastOpenSslError();
        return false;
    }

    if (!CMS_add1_signer(cms.get(), signerCert, signerKey, EVP_sha256(), flags)) {
        if (outErr) *outErr = "CMS_add1_signer fallo: " + LastOpenSslError();
        return false;
    }

    if (CMS_final(cms.get(), dataBio.get(), nullptr, flags) != 1) {
        if (outErr) *outErr = "CMS_final fallo: " + LastOpenSslError();
        return false;
    }

    const int derLen = i2d_CMS_ContentInfo(cms.get(), nullptr);
    if (derLen <= 0) {
        if (outErr) *outErr = "i2d_CMS_ContentInfo (tamano) fallo: " + LastOpenSslError();
        return false;
    }

    outDerSig.resize(static_cast<std::size_t>(derLen));
    unsigned char* outPtr = outDerSig.data();
    const int encodedLen = i2d_CMS_ContentInfo(cms.get(), &outPtr);
    if (encodedLen <= 0 || encodedLen != derLen) {
        outDerSig.clear();
        if (outErr) *outErr = "i2d_CMS_ContentInfo fallo: " + LastOpenSslError();
        return false;
    }
    return true;
}

bool BuildSignedPdfFromCleanBytes(
    const std::vector<std::uint8_t>& inputPdfBytes,
    X509* signerCert,
    EVP_PKEY* signerKey,
    STACK_OF(X509)* extraCaCerts,
    const PdfSignatureOptions& opts,
    std::vector<std::uint8_t>& outSignedPdfBytes,
    std::string* outErr)
{
    PdfDocument src;
    if (!src.LoadFromBytes(inputPdfBytes, outErr) || src.PageCount() == 0) {
        return false;
    }

    std::string pdf = "%PDF-1.7\n%\xE2\xE3\xCF\xD3\n";
    struct ObjRec { std::uint32_t id; std::string body; };
    std::vector<ObjRec> objs;
    auto allocId = [&]() -> std::uint32_t {
        objs.push_back({ static_cast<std::uint32_t>(objs.size() + 1u), {} });
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

    std::vector<std::size_t> offsets(objs.size() + 1u, 0);
    std::size_t brTokenPos = std::string::npos;
    std::size_t contentsLtPos = std::string::npos;
    std::size_t contentsGtPos = std::string::npos;

    for (const auto& obj : objs) {
        offsets[obj.id] = pdf.size();
        const std::size_t bodyBase = pdf.size() + std::to_string(obj.id).size() + 7u;
        pdf += std::to_string(obj.id) + " 0 obj\n";
        pdf += obj.body;
        if (obj.id == sigObjId) {
            const std::size_t brRel = obj.body.find(kByteRangePlaceholder);
            const std::size_t cntRel = obj.body.find("/Contents <");
            if (brRel != std::string::npos) brTokenPos = bodyBase + brRel;
            if (cntRel != std::string::npos) {
                contentsLtPos = bodyBase + cntRel + 10u;
                contentsGtPos = contentsLtPos + 1u + zeroHex.size() + 1u;
            }
        }
        pdf += "\nendobj\n";
    }

    const std::size_t xrefOff = pdf.size();
    pdf += "xref\n0 " + std::to_string(objs.size() + 1u) + "\n";
    pdf += "0000000000 65535 f \n";
    for (std::size_t i = 1; i <= objs.size(); ++i) {
        char line[32]{};
        std::snprintf(line, sizeof(line), "%010zu 00000 n \n", offsets[i]);
        pdf += line;
    }
    pdf += "trailer\n<</Size " + std::to_string(objs.size() + 1u) + " /Root " + std::to_string(catalogId) + " 0 R>>\n";
    pdf += "startxref\n" + std::to_string(xrefOff) + "\n%%EOF\n";

    if (brTokenPos == std::string::npos || contentsLtPos == std::string::npos || contentsGtPos > pdf.size()) {
        if (outErr) *outErr = "Error interno localizando /ByteRange o /Contents.";
        return false;
    }

    char brBuf[64]{};
    const std::size_t part2Len = pdf.size() - contentsGtPos;
    std::snprintf(brBuf, sizeof(brBuf), "[0 %10zu %10zu %10zu]", contentsLtPos, contentsGtPos, part2Len);
    const std::string brFixed(brBuf);
    if (brFixed.size() != std::strlen(kByteRangePlaceholder)) {
        if (outErr) *outErr = "Desbordamiento formateando /ByteRange.";
        return false;
    }
    std::memcpy(&pdf[brTokenPos], brFixed.data(), brFixed.size());

    std::vector<std::uint8_t> derSig;
    if (!SignTwoByteRangesPkcs7(
            signerCert,
            signerKey,
            extraCaCerts,
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
    std::memcpy(&pdf[contentsLtPos + 1u], sigHex.data(), sigHex.size());

    outSignedPdfBytes.assign(pdf.begin(), pdf.end());
    return true;
}

bool ReadBytesFromFilePosix(const std::wstring& path, std::vector<std::uint8_t>& out, std::string* outErr) {
    out.clear();
    const std::string utf8 = WideToUtf8(path);
    if (utf8.empty()) {
        if (outErr) *outErr = "Ruta de archivo vacia.";
        return false;
    }
    const int fd = ::open(utf8.c_str(), O_RDONLY);
    if (fd < 0) {
        if (outErr) *outErr = "No se pudo abrir archivo (errno " + std::to_string(errno) + ").";
        return false;
    }
    struct stat st{};
    if (::fstat(fd, &st) != 0 || st.st_size < 0 || st.st_size > 512ll * 1024ll * 1024ll) {
        ::close(fd);
        if (outErr) *outErr = "Tamano de archivo invalido o demasiado grande.";
        return false;
    }
    out.resize(static_cast<std::size_t>(st.st_size));
    std::size_t off = 0;
    while (off < out.size()) {
        const ssize_t rd = ::read(fd, out.data() + off, out.size() - off);
        if (rd < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            out.clear();
            if (outErr) *outErr = "Error leyendo archivo.";
            return false;
        }
        if (rd == 0) {
            out.resize(off);
            break;
        }
        off += static_cast<std::size_t>(rd);
    }
    ::close(fd);
    return true;
}

bool WriteBytesToFilePosix(const std::wstring& path, const std::vector<std::uint8_t>& bytes, std::string* outErr) {
    const std::string utf8 = WideToUtf8(path);
    if (utf8.empty()) {
        if (outErr) *outErr = "Ruta destino vacia.";
        return false;
    }
    const std::string tmpPath = utf8 + ".tmp";
    const int fd = ::open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        if (outErr) *outErr = "No se pudo escribir PDF firmado (errno " + std::to_string(errno) + ").";
        return false;
    }
    std::size_t off = 0;
    while (off < bytes.size()) {
        const ssize_t wr = ::write(fd, bytes.data() + off, bytes.size() - off);
        if (wr < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            ::unlink(tmpPath.c_str());
            if (outErr) *outErr = "Error escribiendo PDF firmado.";
            return false;
        }
        if (wr == 0) {
            ::close(fd);
            ::unlink(tmpPath.c_str());
            if (outErr) *outErr = "Error escribiendo PDF firmado (write=0).";
            return false;
        }
        off += static_cast<std::size_t>(wr);
    }
    (void)::fsync(fd);
    ::close(fd);
    if (std::rename(tmpPath.c_str(), utf8.c_str()) != 0) {
        ::unlink(tmpPath.c_str());
        if (outErr) *outErr = "No se pudo renombrar PDF firmado.";
        return false;
    }
    return true;
}

std::string ExtractSignerCommonName(X509* cert) {
    if (!cert) return {};
    X509_NAME* subj = X509_get_subject_name(cert);
    if (!subj) return {};

    const int idx = X509_NAME_get_index_by_NID(subj, NID_commonName, -1);
    if (idx >= 0) {
        X509_NAME_ENTRY* entry = X509_NAME_get_entry(subj, idx);
        if (entry) {
            ASN1_STRING* str = X509_NAME_ENTRY_get_data(entry);
            if (str) {
                unsigned char* utf8 = nullptr;
                const int len = ASN1_STRING_to_UTF8(&utf8, str);
                if (len >= 0 && utf8) {
                    std::string out(reinterpret_cast<const char*>(utf8), static_cast<std::size_t>(len));
                    OPENSSL_free(utf8);
                    return out;
                }
            }
        }
    }

    BioPtr bio(BIO_new(BIO_s_mem()));
    if (!bio) return {};
    X509_NAME_print_ex(bio.get(), subj, 0, XN_FLAG_RFC2253);
    BUF_MEM* memPtr = nullptr;
    BIO_get_mem_ptr(bio.get(), &memPtr);
    if (!memPtr || !memPtr->data) return {};
    return std::string(memPtr->data, memPtr->length);
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
    if (!ReadBytesFromFilePosix(pfxPath, pfxBytes, outErr)) {
        if (outErr && outErr->empty()) *outErr = "No se pudo leer archivo .pfx.";
        return false;
    }

    ERR_clear_error();
    const unsigned char* p = pfxBytes.data();
    Pkcs12Ptr p12(d2i_PKCS12(nullptr, &p, static_cast<long>(pfxBytes.size())));
    if (!p12) {
        if (outErr) *outErr = "Formato PKCS#12 (.pfx) invalido: " + LastOpenSslError();
        return false;
    }

    const std::string pwdUtf8 = WideToUtf8(pfxPassword);
    EVP_PKEY* rawKey = nullptr;
    X509* rawCert = nullptr;
    STACK_OF(X509)* rawCa = nullptr;
    int parseOk = PKCS12_parse(p12.get(), pwdUtf8.c_str(), &rawKey, &rawCert, &rawCa);
    if (parseOk != 1 && pwdUtf8.empty()) {
        ERR_clear_error();
        parseOk = PKCS12_parse(p12.get(), nullptr, &rawKey, &rawCert, &rawCa);
    }

    EvpPkeyPtr pkey(rawKey);
    X509Ptr cert(rawCert);
    StackX509Ptr caStack(rawCa);

    if (parseOk != 1 || !pkey || !cert) {
        if (outErr) *outErr = "No se pudo importar archivo .pfx (contrasena incorrecta o falta clave privada).";
        return false;
    }

    return BuildSignedPdfFromCleanBytes(inputPdfBytes, cert.get(), pkey.get(), caStack.get(), opts, outSignedPdfBytes, outErr);
}

bool PdfSigner::SignPdfBytesWithSelfSigned(
    const std::vector<std::uint8_t>& inputPdfBytes,
    const PdfSignatureOptions& opts,
    std::vector<std::uint8_t>& outSignedPdfBytes,
    std::string* outErr)
{
    ERR_clear_error();
    static std::atomic<std::uint64_t> seq{ 0 };

    EvpPkeyPtr pkey(EVP_RSA_gen(2048));
    if (!pkey) {
        if (outErr) *outErr = "EVP_RSA_gen(2048) fallo: " + LastOpenSslError();
        return false;
    }

    X509Ptr cert(X509_new());
    if (!cert) {
        if (outErr) *outErr = "X509_new fallo.";
        return false;
    }

    X509_set_version(cert.get(), 2); // v3
    const std::uint64_t serialNum = (static_cast<std::uint64_t>(std::time(nullptr)) << 16) ^
        (static_cast<std::uint64_t>(::getpid()) << 8) ^ (++seq);
    ASN1_INTEGER_set_uint64(X509_get_serialNumber(cert.get()), serialNum ? serialNum : 1ull);

    X509_gmtime_adj(X509_getm_notBefore(cert.get()), -60);
    X509_gmtime_adj(X509_getm_notAfter(cert.get()), 5L * 365L * 24L * 3600L);

    const std::string cn = opts.signerNameUtf8.empty() ? std::string("Document Studio") : opts.signerNameUtf8;
    X509_NAME* name = X509_get_subject_name(cert.get());
    if (!name ||
        X509_NAME_add_entry_by_txt(
            name,
            "CN",
            MBSTRING_UTF8,
            reinterpret_cast<const unsigned char*>(cn.data()),
            static_cast<int>(cn.size()),
            -1,
            0) != 1) {
        if (outErr) *outErr = "No se pudo establecer CN del certificado.";
        return false;
    }
    X509_set_issuer_name(cert.get(), name);

    if (X509_set_pubkey(cert.get(), pkey.get()) != 1) {
        if (outErr) *outErr = "X509_set_pubkey fallo.";
        return false;
    }
    if (X509_sign(cert.get(), pkey.get(), EVP_sha256()) <= 0) {
        if (outErr) *outErr = "X509_sign fallo: " + LastOpenSslError();
        return false;
    }

    return BuildSignedPdfFromCleanBytes(inputPdfBytes, cert.get(), pkey.get(), nullptr, opts, outSignedPdfBytes, outErr);
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

    return WriteBytesToFilePosix(outPdfPath, signedPdf, outErr);
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

    ERR_clear_error();
    const unsigned char* p = derSig.data();
    CmsPtr cms(d2i_CMS_ContentInfo(nullptr, &p, static_cast<long>(derSig.size())));
    if (!cms) {
        res.cryptographicValid = false;
        res.diagnosticUtf8 = "CMS/PKCS#7 DER inv\xC3\xA1lido: " + LastOpenSslError();
        return res;
    }

    BioPtr dataBio1 = CreateBioForTwoRanges(
        pdfBytes.data() + r0, static_cast<std::size_t>(l0),
        pdfBytes.data() + r1, static_cast<std::size_t>(l1));
    if (!dataBio1) {
        res.cryptographicValid = false;
        res.diagnosticUtf8 = "Error interno preparando BIO de verificaci\xC3\xB3n.";
        return res;
    }

    const unsigned int baseFlags = CMS_DETACHED | CMS_BINARY;
    if (CMS_verify(cms.get(), nullptr, nullptr, dataBio1.get(), nullptr, baseFlags | CMS_NO_SIGNER_CERT_VERIFY) != 1) {
        res.cryptographicValid = false;
        res.diagnosticUtf8 = "Firma PKCS#7 INV\xC3\x81LIDA: el contenido del PDF fue modificado despu\xC3\xA9s de firmarlo (" + LastOpenSslError() + ").";
        return res;
    }
    res.cryptographicValid = true;

    StackX509ShallowPtr signers(CMS_get0_signers(cms.get()));
    if (signers && sk_X509_num(signers.get()) > 0) {
        X509* signerCert = sk_X509_value(signers.get(), 0);
        res.signerSubjectUtf8 = ExtractSignerCommonName(signerCert);
    }

    ERR_clear_error();
    X509StorePtr rootStore(X509_STORE_new());
    if (rootStore && X509_STORE_set_default_paths(rootStore.get()) == 1) {
        BioPtr dataBio2 = CreateBioForTwoRanges(
            pdfBytes.data() + r0, static_cast<std::size_t>(l0),
            pdfBytes.data() + r1, static_cast<std::size_t>(l1));
        if (dataBio2 && CMS_verify(cms.get(), nullptr, rootStore.get(), dataBio2.get(), nullptr, baseFlags) == 1) {
            res.trustedBySystemRoot = true;
        }
    }
    ERR_clear_error();

    res.diagnosticUtf8 = res.trustedBySystemRoot
        ? "Firma PKCS#7 SHA-256 v\xC3\xA1lida y verificada contra entidad de confianza del sistema."
        : "Firma PKCS#7 SHA-256 criptogr\xC3\xA1" "ficamente \xC3\xADntegra (certificado autofirmado o no presente en ra\xC3\xADz del sistema).";
    return res;
}

PdfSignatureVerifyResult PdfSigner::VerifyPdfFile(const std::wstring& pdfPath) {
    std::vector<std::uint8_t> bytes;
    std::string err;
    if (!ReadBytesFromFilePosix(pdfPath, bytes, &err)) {
        PdfSignatureVerifyResult res;
        res.diagnosticUtf8 = err.empty() ? "No se pudo leer el PDF." : err;
        return res;
    }
    return VerifyPdfBytes(bytes);
}

} // namespace DocStudio
