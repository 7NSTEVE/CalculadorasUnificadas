#pragma once

#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio {

struct PdfRef {
    std::uint32_t obj = 0;
    std::uint16_t gen = 0;
    bool operator==(const PdfRef& o) const noexcept { return obj == o.obj && gen == o.gen; }
};

struct PdfValue;
using PdfArray = std::vector<PdfValue>;
using PdfDict = std::map<std::string, PdfValue>;

struct PdfStreamData {
    PdfDict dict;
    std::vector<std::uint8_t> rawBytes;
};

struct PdfValue {
    enum class Kind {
        Null = 0,
        Bool,
        Int,
        Real,
        Name,
        String,
        Array,
        Dict,
        Stream,
        Ref
    };

    Kind kind = Kind::Null;
    bool boolVal = false;
    std::int64_t intVal = 0;
    double realVal = 0.0;
    std::string strVal; // Name (without leading '/'), raw byte string
    PdfArray arrayVal;
    PdfDict dictVal;
    std::shared_ptr<PdfStreamData> streamVal;
    PdfRef refVal{};

    bool IsNull() const noexcept { return kind == Kind::Null; }
    bool IsDict() const noexcept { return kind == Kind::Dict; }
    bool IsArray() const noexcept { return kind == Kind::Array; }
    bool IsStream() const noexcept { return kind == Kind::Stream && streamVal != nullptr; }
    bool IsName() const noexcept { return kind == Kind::Name; }
    bool IsRef() const noexcept { return kind == Kind::Ref; }
    bool IsNumber() const noexcept { return kind == Kind::Int || kind == Kind::Real; }

    double AsNumber(double def = 0.0) const noexcept {
        if (kind == Kind::Int) return static_cast<double>(intVal);
        if (kind == Kind::Real) return realVal;
        return def;
    }

    const PdfDict* DictPtr() const noexcept {
        if (kind == Kind::Dict) return &dictVal;
        if (kind == Kind::Stream && streamVal) return &streamVal->dict;
        return nullptr;
    }

    const PdfValue* DictGet(const std::string& key) const noexcept {
        const PdfDict* d = DictPtr();
        if (!d) return nullptr;
        auto it = d->find(key);
        return (it == d->end()) ? nullptr : &it->second;
    }
};

struct PdfPageInfo {
    std::size_t pageIndex0 = 0;
    PdfRef pageRef{};
    DocRect mediaBox{ 0.0, 0.0, 595.0, 842.0 };
    DocRect cropBox{ 0.0, 0.0, 595.0, 842.0 };
    bool hasCropBox = false;
    int rotationDeg = 0;
    std::string extractedTextUtf8;
    std::vector<PdfLinkRect> pdfLinks;
    std::vector<PdfFormField> formFields;
    std::vector<PdfTextRunBox> textBoxes;
};

struct TextEditCapability {
    bool supportedDirectStream = false;
    bool supportedVisualRedaction = true;
    std::size_t matchCountInStream = 0;
    std::string diagnosticUtf8;
};

class PdfDocument {
public:
    bool LoadFromBytes(std::vector<std::uint8_t> bytes, std::string* outErr = nullptr);
    bool LoadFromFile(const std::wstring& path, std::string* outErr = nullptr);

    bool IsLoaded() const noexcept { return loaded_; }
    const std::string& PdfVersion() const noexcept { return version_; }
    PdfRef CatalogRef() const noexcept { return rootRef_; }
    std::size_t PageCount() const noexcept { return pages_.size(); }
    const PdfPageInfo& PageAt(std::size_t idx) const { return pages_.at(idx); }
    const std::vector<PdfPageInfo>& Pages() const noexcept { return pages_; }

    const PdfValue* Resolve(const PdfValue* v) const;
    PdfValue LookupObject(std::uint32_t objNum) const;
    bool DecompressStream(const PdfStreamData& st, std::vector<std::uint8_t>& outBytes, std::string* outErr = nullptr) const;
    std::vector<std::uint8_t> GetPageContentBytes(std::size_t pageIndex0) const;

    // Populates DocumentModel with pages imported from this PDF (and extracted text).
    bool PopulateDocumentModel(
        DocumentModel& doc,
        const std::string& sourcePathUtf8,
        std::string* outErr = nullptr) const;

    // Inspects whether exact plain-text direct replacement in content stream is safely possible,
    // vs visual redaction overlay fallback (reported honestly).
    TextEditCapability InspectTextReplacement(
        std::size_t pageIndex0,
        const std::string& searchUtf8) const;

    // Serializes indirect object or resolved value back into clean PDF source syntax.
    static std::string SerializeValue(const PdfValue& v);
    const std::vector<std::uint8_t>& RawBytes() const noexcept { return data_; }

private:
    struct XRefEntry {
        bool inUse = false;
        bool compressed = false; // in ObjStm
        std::uint64_t offsetOrObjStm = 0;
        std::uint32_t genOrIndex = 0;
    };

    std::vector<std::uint8_t> data_;
    std::string version_ = "1.7";
    bool loaded_ = false;
    PdfRef rootRef_{};
    bool hasEncryptRef_ = false;
    PdfRef encryptRef_{};
    std::string fileId0_;
    bool encrypted_ = false;
    int encryptR_ = 0;
    std::vector<std::uint8_t> fileKey_;
    bool stmEncrypted_ = false;
    bool strEncrypted_ = false;
    bool stmUseAes_ = false;
    bool strUseAes_ = false;
    bool encryptMetadata_ = true;
    std::unordered_map<std::uint32_t, XRefEntry> xref_;
    mutable std::unordered_map<std::uint32_t, PdfValue> cache_;
    mutable std::unordered_map<std::uint32_t, std::unordered_map<std::size_t, PdfValue>> objStmCache_;
    std::vector<PdfPageInfo> pages_;

    bool ParseTrailerAndXref(std::size_t startXrefOffset, std::string* outErr);
    bool ParseClassicXref(std::size_t offset, PdfDict& outTrailer, std::string* outErr);
    bool ParseXrefStream(std::size_t offset, PdfDict& outTrailer, std::string* outErr);
    bool InitializeEncryption(std::string* outErr);
    std::vector<std::uint8_t> ComputeObjectKey(std::uint32_t objNum, std::uint16_t gen, bool aes) const;
    bool DecryptBufferForObject(std::uint32_t objNum, std::uint16_t gen, bool aes, const std::vector<std::uint8_t>& in, std::vector<std::uint8_t>& out) const;
    void DecryptValueTree(PdfValue& v, std::uint32_t objNum, std::uint16_t gen, bool insideTopStream = false) const;
    bool LoadObjStm(std::uint32_t objStmNum) const;
    void CollectPages(const PdfRef& nodeRef, const DocRect& inheritedMedia, const DocRect& inheritedCrop, bool hasInheritedCrop, int inheritedRot, const PdfDict* inheritedRes, std::vector<std::uint32_t>& visited, int depth);
    std::string ExtractPageText(const PdfValue& pageDictVal, const PdfDict* inheritedRes, std::size_t pageIndex0) const;
};

// Normalizes Unicode ligatures (fi, fl, ff, ffi, ffl) to ASCII sequences for consistent search.
std::string NormalizeLigaturesUtf8(const std::string& inUtf8);

} // namespace DocStudio
