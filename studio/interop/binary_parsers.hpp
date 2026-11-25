#pragma once

#include "../core/doc_types.hpp"

#include <zlib.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <fdi.h>
#endif

namespace DocStudio::Interop::Binary {

// ============================================================================
// 1. Lightweight Protobuf Wire Decoder (Goodnotes 6 .pb / .events.pb streams)
// ============================================================================
struct ProtoField {
    std::uint32_t fieldNumber = 0;
    std::uint8_t wireType = 0; // 0=varint, 1=64bit, 2=len, 5=32bit
    std::uint64_t varintVal = 0;
    std::uint64_t fixed64Val = 0;
    std::uint32_t fixed32Val = 0;
    std::vector<std::uint8_t> bytesVal;

    float AsFloat() const noexcept {
        float f = 0.0f;
        std::memcpy(&f, &fixed32Val, sizeof(f));
        return f;
    }
    double AsDouble() const noexcept {
        double d = 0.0;
        std::memcpy(&d, &fixed64Val, sizeof(d));
        return d;
    }
    std::string AsString() const {
        return std::string(reinterpret_cast<const char*>(bytesVal.data()), bytesVal.size());
    }
    std::vector<float> AsPackedFloats() const {
        std::vector<float> out;
        if (bytesVal.size() < 4 || (bytesVal.size() % 4) != 0) return out;
        out.resize(bytesVal.size() / 4);
        std::memcpy(out.data(), bytesVal.data(), bytesVal.size());
        return out;
    }
    std::vector<double> AsPackedDoubles() const {
        std::vector<double> out;
        if (bytesVal.size() < 8 || (bytesVal.size() % 8) != 0) return out;
        out.resize(bytesVal.size() / 8);
        std::memcpy(out.data(), bytesVal.data(), bytesVal.size());
        return out;
    }
};

inline bool ReadVarint(const std::uint8_t* data, std::size_t size, std::size_t& pos, std::uint64_t& out) noexcept {
    out = 0;
    int shift = 0;
    while (pos < size && shift < 64) {
        const std::uint8_t b = data[pos++];
        out |= static_cast<std::uint64_t>(b & 0x7Fu) << shift;
        if ((b & 0x80u) == 0) return true;
        shift += 7;
    }
    return false;
}

inline bool DecodeProtobufMessage(
    const std::uint8_t* data,
    std::size_t size,
    std::vector<ProtoField>& outFields,
    std::size_t maxFields = 250000u)
{
    outFields.clear();
    if (!data || size == 0) return true;
    std::size_t pos = 0;
    while (pos < size && outFields.size() < maxFields) {
        std::uint64_t tag = 0;
        if (!ReadVarint(data, size, pos, tag)) return false;
        const std::uint32_t fieldNum = static_cast<std::uint32_t>(tag >> 3);
        const std::uint8_t wireType = static_cast<std::uint8_t>(tag & 0x07u);
        if (fieldNum == 0) return false;

        ProtoField f{};
        f.fieldNumber = fieldNum;
        f.wireType = wireType;
        if (wireType == 0) {
            if (!ReadVarint(data, size, pos, f.varintVal)) return false;
        } else if (wireType == 1) {
            if (pos + 8 > size) return false;
            std::memcpy(&f.fixed64Val, data + pos, 8);
            pos += 8;
        } else if (wireType == 2) {
            std::uint64_t len = 0;
            if (!ReadVarint(data, size, pos, len)) return false;
            if (len > size - pos) return false;
            f.bytesVal.assign(data + pos, data + pos + static_cast<std::size_t>(len));
            pos += static_cast<std::size_t>(len);
        } else if (wireType == 5) {
            if (pos + 4 > size) return false;
            std::memcpy(&f.fixed32Val, data + pos, 4);
            pos += 4;
        } else {
            return false;
        }
        outFields.push_back(std::move(f));
    }
    return true;
}

// ============================================================================
// 2. iPadOS Binary Plist (bplist00) + XML Plist + NSKeyedArchiver Decoder
//    (Used by Notability .note Session.plist)
// ============================================================================
struct PlistValue {
    enum class Kind { Null, Bool, Int, Real, String, Data, Uid, Array, Dict };
    Kind kind = Kind::Null;
    bool boolVal = false;
    std::int64_t intVal = 0;
    double realVal = 0.0;
    std::uint64_t uidVal = 0;
    std::string strVal;
    std::vector<std::uint8_t> dataVal;
    std::vector<PlistValue> arrayVal;
    std::map<std::string, PlistValue> dictVal;

    const PlistValue* Get(const std::string& key) const noexcept {
        if (kind != Kind::Dict) return nullptr;
        auto it = dictVal.find(key);
        return (it == dictVal.end()) ? nullptr : &it->second;
    }
    double AsDouble(double def = 0.0) const noexcept {
        if (kind == Kind::Real) return realVal;
        if (kind == Kind::Int) return static_cast<double>(intVal);
        return def;
    }
};

inline std::uint64_t ReadBeUint(const std::uint8_t* p, std::size_t n) noexcept {
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < n; ++i) v = (v << 8) | p[i];
    return v;
}

inline bool ParseBinaryPlist(const std::uint8_t* data, std::size_t size, PlistValue& outRoot) {
    outRoot = {};
    if (!data || size < 40 || std::memcmp(data, "bplist00", 8) != 0) return false;
    const std::uint8_t* trailer = data + size - 32;
    const std::uint8_t offsetIntSize = trailer[6];
    const std::uint8_t objRefSize = trailer[7];
    const std::uint64_t numObjects = ReadBeUint(trailer + 8, 8);
    const std::uint64_t topObject = ReadBeUint(trailer + 16, 8);
    const std::uint64_t offsetTableOffset = ReadBeUint(trailer + 24, 8);

    if (offsetIntSize == 0 || offsetIntSize > 8 || objRefSize == 0 || objRefSize > 8) return false;
    if (numObjects == 0 || numObjects > 1000000ull || topObject >= numObjects) return false;
    if (offsetTableOffset >= size - 32) return false;
    if (offsetTableOffset + numObjects * offsetIntSize > size - 32) return false;

    std::vector<std::uint64_t> offsets(static_cast<std::size_t>(numObjects), 0);
    for (std::uint64_t i = 0; i < numObjects; ++i) {
        offsets[static_cast<std::size_t>(i)] = ReadBeUint(data + offsetTableOffset + i * offsetIntSize, offsetIntSize);
    }

    auto readLength = [&](std::size_t& pos, std::uint8_t lowNibble, std::uint64_t& outLen) -> bool {
        if (lowNibble < 0x0Fu) {
            outLen = lowNibble;
            return true;
        }
        if (pos >= size) return false;
        const std::uint8_t intMarker = data[pos++];
        if ((intMarker & 0xF0u) != 0x10u) return false;
        const std::size_t nBytes = static_cast<std::size_t>(1ull << (intMarker & 0x0Fu));
        if (nBytes > 8 || pos + nBytes > size) return false;
        outLen = ReadBeUint(data + pos, nBytes);
        pos += nBytes;
        return true;
    };

    auto parseObj = [&](auto&& self, std::uint64_t objIdx, int depth) -> PlistValue {
        PlistValue v{};
        if (depth > 48 || objIdx >= numObjects) return v;
        std::size_t pos = static_cast<std::size_t>(offsets[static_cast<std::size_t>(objIdx)]);
        if (pos >= offsetTableOffset) return v;
        const std::uint8_t marker = data[pos++];
        const std::uint8_t hi = (marker >> 4) & 0x0Fu;
        const std::uint8_t lo = marker & 0x0Fu;

        if (hi == 0x0u) {
            if (lo == 0x8u) { v.kind = PlistValue::Kind::Bool; v.boolVal = false; }
            else if (lo == 0x9u) { v.kind = PlistValue::Kind::Bool; v.boolVal = true; }
        } else if (hi == 0x1u) {
            const std::size_t nBytes = (lo < 4u) ? (static_cast<std::size_t>(1ull) << lo) : 16u;
            if (nBytes <= 8 && pos + nBytes <= offsetTableOffset) {
                v.kind = PlistValue::Kind::Int;
                v.intVal = static_cast<std::int64_t>(ReadBeUint(data + pos, nBytes));
            }
        } else if (hi == 0x2u || hi == 0x3u) {
            const std::size_t nBytes = (hi == 0x3u) ? 8u : ((lo < 4u) ? (static_cast<std::size_t>(1ull) << lo) : 16u);
            if (nBytes == 4 && pos + 4 <= offsetTableOffset) {
                std::uint32_t u = static_cast<std::uint32_t>(ReadBeUint(data + pos, 4));
                float f = 0.0f;
                std::memcpy(&f, &u, 4);
                v.kind = PlistValue::Kind::Real;
                v.realVal = f;
            } else if (nBytes == 8 && pos + 8 <= offsetTableOffset) {
                std::uint64_t u = ReadBeUint(data + pos, 8);
                double d = 0.0;
                std::memcpy(&d, &u, 8);
                v.kind = PlistValue::Kind::Real;
                v.realVal = d;
            }
        } else if (hi == 0x4u) {
            std::uint64_t len = 0;
            if (readLength(pos, lo, len) && len <= offsetTableOffset - pos) {
                v.kind = PlistValue::Kind::Data;
                v.dataVal.assign(data + pos, data + pos + static_cast<std::size_t>(len));
            }
        } else if (hi == 0x5u) {
            std::uint64_t len = 0;
            if (readLength(pos, lo, len) && len <= offsetTableOffset - pos) {
                v.kind = PlistValue::Kind::String;
                v.strVal.assign(reinterpret_cast<const char*>(data + pos), static_cast<std::size_t>(len));
            }
        } else if (hi == 0x6u) {
            std::uint64_t len = 0;
            if (readLength(pos, lo, len) && len * 2 <= offsetTableOffset - pos) {
                v.kind = PlistValue::Kind::String;
                for (std::uint64_t i = 0; i < len; ++i) {
                    const std::uint16_t u16 = static_cast<std::uint16_t>(ReadBeUint(data + pos + i * 2, 2));
                    if (u16 < 0x80u) v.strVal.push_back(static_cast<char>(u16));
                    else if (u16 < 0x800u) {
                        v.strVal.push_back(static_cast<char>(0xC0u | (u16 >> 6)));
                        v.strVal.push_back(static_cast<char>(0x80u | (u16 & 0x3Fu)));
                    } else {
                        v.strVal.push_back(static_cast<char>(0xE0u | (u16 >> 12)));
                        v.strVal.push_back(static_cast<char>(0x80u | ((u16 >> 6) & 0x3Fu)));
                        v.strVal.push_back(static_cast<char>(0x80u | (u16 & 0x3Fu)));
                    }
                }
            }
        } else if (hi == 0x8u) {
            const std::size_t nBytes = static_cast<std::size_t>(lo) + 1u;
            if (nBytes <= 8 && pos + nBytes <= offsetTableOffset) {
                v.kind = PlistValue::Kind::Uid;
                v.uidVal = ReadBeUint(data + pos, nBytes);
            }
        } else if (hi == 0xAu) {
            std::uint64_t cnt = 0;
            if (readLength(pos, lo, cnt) && cnt * objRefSize <= offsetTableOffset - pos) {
                v.kind = PlistValue::Kind::Array;
                v.arrayVal.reserve(static_cast<std::size_t>((std::min<std::uint64_t>)(cnt, 65536ull)));
                for (std::uint64_t i = 0; i < cnt && i < 65536ull; ++i) {
                    const std::uint64_t ref = ReadBeUint(data + pos + i * objRefSize, objRefSize);
                    v.arrayVal.push_back(self(self, ref, depth + 1));
                }
            }
        } else if (hi == 0xDu) {
            std::uint64_t cnt = 0;
            if (readLength(pos, lo, cnt) && cnt * objRefSize * 2 <= offsetTableOffset - pos) {
                v.kind = PlistValue::Kind::Dict;
                const std::size_t keysPos = pos;
                const std::size_t valsPos = pos + static_cast<std::size_t>(cnt) * objRefSize;
                for (std::uint64_t i = 0; i < cnt && i < 65536ull; ++i) {
                    const std::uint64_t kRef = ReadBeUint(data + keysPos + i * objRefSize, objRefSize);
                    const std::uint64_t vRef = ReadBeUint(data + valsPos + i * objRefSize, objRefSize);
                    PlistValue kv = self(self, kRef, depth + 1);
                    PlistValue vv = self(self, vRef, depth + 1);
                    if (kv.kind == PlistValue::Kind::String) {
                        v.dictVal[kv.strVal] = std::move(vv);
                    }
                }
            }
        }
        return v;
    };

    outRoot = parseObj(parseObj, topObject, 0);
    return outRoot.kind != PlistValue::Kind::Null;
}

// Dereference NSKeyedArchiver $objects table UIDs transparently
inline const PlistValue* ResolveArchiverUid(const PlistValue& root, const PlistValue* val) noexcept {
    if (!val) return nullptr;
    if (val->kind != PlistValue::Kind::Uid) return val;
    const PlistValue* objs = root.Get("$objects");
    if (!objs || objs->kind != PlistValue::Kind::Array) return val;
    if (val->uidVal >= objs->arrayVal.size()) return val;
    return &objs->arrayVal[static_cast<std::size_t>(val->uidVal)];
}

// ============================================================================
// 3. Microsoft CAB (MSCF) Unpacker (NONE & MSZIP via zlib) for OneNote .onepkg
// ============================================================================
struct CabFileEntry {
    std::string filename;
    std::vector<std::uint8_t> data;
};

inline std::uint16_t ReadLe16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
}

inline std::uint32_t ReadLe32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

#ifdef _WIN32
namespace detail_cab {
struct MemCabHandle {
    bool isInput = false;
    const std::uint8_t* readPtr = nullptr;
    std::size_t readSize = 0;
    std::size_t pos = 0;
    std::vector<std::uint8_t>* writeVec = nullptr;
};

struct FdiMemContext {
    const std::uint8_t* cabBytes = nullptr;
    std::size_t cabSize = 0;
    std::vector<CabFileEntry>* outFiles = nullptr;
};

inline thread_local FdiMemContext* g_fdiCtx = nullptr;

inline void* DIAMONDAPI FdiAlloc(ULONG cb) { return std::malloc(cb); }
inline void DIAMONDAPI FdiFree(void* pv) { std::free(pv); }
inline INT_PTR DIAMONDAPI FdiOpen(LPSTR pszFile, int /*oflag*/, int /*pmode*/) {
    if (!g_fdiCtx) return -1;
    auto* h = new MemCabHandle();
    h->isInput = true;
    h->readPtr = g_fdiCtx->cabBytes;
    h->readSize = g_fdiCtx->cabSize;
    h->pos = 0;
    (void)pszFile;
    return reinterpret_cast<INT_PTR>(h);
}
inline UINT DIAMONDAPI FdiRead(INT_PTR hf, void* pv, UINT cb) {
    auto* h = reinterpret_cast<MemCabHandle*>(hf);
    if (!h || !h->isInput || h->pos >= h->readSize) return 0;
    const std::size_t avail = h->readSize - h->pos;
    const std::size_t take = (std::min<std::size_t>)(cb, avail);
    std::memcpy(pv, h->readPtr + h->pos, take);
    h->pos += take;
    return static_cast<UINT>(take);
}
inline UINT DIAMONDAPI FdiWrite(INT_PTR hf, void* pv, UINT cb) {
    auto* h = reinterpret_cast<MemCabHandle*>(hf);
    if (!h || h->isInput || !h->writeVec) return 0;
    if (h->writeVec->size() + cb > 128u * 1024u * 1024u) return 0;
    const auto* src = static_cast<const std::uint8_t*>(pv);
    h->writeVec->insert(h->writeVec->end(), src, src + cb);
    return cb;
}
inline int DIAMONDAPI FdiClose(INT_PTR hf) {
    auto* h = reinterpret_cast<MemCabHandle*>(hf);
    delete h;
    return 0;
}
inline long DIAMONDAPI FdiSeek(INT_PTR hf, long dist, int seektype) {
    auto* h = reinterpret_cast<MemCabHandle*>(hf);
    if (!h || !h->isInput) return -1;
    std::int64_t next = static_cast<std::int64_t>(h->pos);
    if (seektype == 0) next = dist; // SEEK_SET
    else if (seektype == 1) next += dist; // SEEK_CUR
    else if (seektype == 2) next = static_cast<std::int64_t>(h->readSize) + dist; // SEEK_END
    if (next < 0) next = 0;
    if (static_cast<std::size_t>(next) > h->readSize) next = static_cast<std::int64_t>(h->readSize);
    h->pos = static_cast<std::size_t>(next);
    return static_cast<long>(h->pos);
}
inline INT_PTR DIAMONDAPI FdiNotify(FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION pfdin) {
    if (!g_fdiCtx || !g_fdiCtx->outFiles) return -1;
    switch (fdint) {
    case fdintCOPY_FILE: {
        CabFileEntry entry{};
        entry.filename = pfdin->psz1 ? std::string(pfdin->psz1) : std::string("section.one");
        g_fdiCtx->outFiles->push_back(std::move(entry));
        auto* h = new MemCabHandle();
        h->isInput = false;
        h->writeVec = &g_fdiCtx->outFiles->back().data;
        return reinterpret_cast<INT_PTR>(h);
    }
    case fdintCLOSE_FILE_INFO: {
        auto* h = reinterpret_cast<MemCabHandle*>(pfdin->hf);
        delete h;
        return TRUE;
    }
    case fdintCABINET_INFO:
    case fdintPARTIAL_FILE:
    case fdintENUMERATE:
        return 0;
    case fdintNEXT_CABINET:
        return -1;
    }
    return 0;
}

inline bool TryExtractCabViaWindowsFdi(
    const std::uint8_t* data,
    std::size_t size,
    std::vector<CabFileEntry>& outFiles)
{
    using FnFdiCreate = HFDI(DIAMONDAPI*)(PFNALLOC, PFNFREE, PFNOPEN, PFNREAD, PFNWRITE, PFNCLOSE, PFNSEEK, int, PERF);
    using FnFdiCopy = BOOL(DIAMONDAPI*)(HFDI, LPSTR, LPSTR, int, PFNFDINOTIFY, PFNFDIDECRYPT, void*);
    using FnFdiDestroy = BOOL(DIAMONDAPI*)(HFDI);

    static HMODULE hCab = LoadLibraryW(L"cabinet.dll");
    if (!hCab) return false;
    static auto pCreate = reinterpret_cast<FnFdiCreate>(GetProcAddress(hCab, "FDICreate"));
    static auto pCopy = reinterpret_cast<FnFdiCopy>(GetProcAddress(hCab, "FDICopy"));
    static auto pDestroy = reinterpret_cast<FnFdiDestroy>(GetProcAddress(hCab, "FDIDestroy"));
    if (!pCreate || !pCopy || !pDestroy) return false;

    outFiles.clear();
    outFiles.reserve(32); // keep pointers stable only if no realloc? Wait: FdiNotify only holds pointer during single file!
    // Safety: single file is open at a time in FDICopy, wait - push_back only happens in COPY_FILE before next open!
    // Still, storing into temporary vector per file guarantees pointer safety across any reserve growth.
    FdiMemContext ctx{ data, size, &outFiles };
    g_fdiCtx = &ctx;
    ERF erf{};
    HFDI hfdi = pCreate(FdiAlloc, FdiFree, FdiOpen, FdiRead, FdiWrite, FdiClose, FdiSeek, cpuUNKNOWN, &erf);
    bool ok = false;
    if (hfdi) {
        char cabName[] = "memory.cab";
        char cabPath[] = "";
        ok = (pCopy(hfdi, cabName, cabPath, 0, FdiNotify, nullptr, nullptr) == TRUE) && !outFiles.empty();
        pDestroy(hfdi);
    }
    g_fdiCtx = nullptr;
    return ok;
}
} // namespace detail_cab
#endif

inline bool ExtractCabArchive(
    const std::uint8_t* data,
    std::size_t size,
    std::vector<CabFileEntry>& outFiles,
    std::string* outErr = nullptr)
{
    outFiles.clear();
    if (!data || size < 36 || std::memcmp(data, "MSCF", 4) != 0) {
        if (outErr) *outErr = "Cabecera MSCF inv\xC3\xA1lida para paquete .onepkg.";
        return false;
    }

#ifdef _WIN32
    // Native Windows cabinet.dll FDICopy supports NONE, MSZIP, QUANTUM, and LZX (OneNote default!)
    if (detail_cab::TryExtractCabViaWindowsFdi(data, size, outFiles)) {
        return true;
    }
    outFiles.clear();
#endif

    const std::uint32_t coffFiles = ReadLe32(data + 16);
    const std::uint16_t cFolders = ReadLe16(data + 26);
    const std::uint16_t cFiles = ReadLe16(data + 28);
    const std::uint16_t flags = ReadLe16(data + 30);

    std::size_t pos = 36;
    std::uint16_t cbCFHeader = 0;
    std::uint8_t cbCFFolder = 0;
    std::uint8_t cbCFData = 0;
    if (flags & 0x0004u) { // RESERVE_PRESENT
        if (pos + 4 > size) return false;
        cbCFHeader = ReadLe16(data + pos);
        cbCFFolder = data[pos + 2];
        cbCFData = data[pos + 3];
        pos += 4 + cbCFHeader;
    }
    auto skipCString = [&]() {
        while (pos < size && data[pos] != 0) ++pos;
        if (pos < size) ++pos;
    };
    if (flags & 0x0001u) { skipCString(); skipCString(); }
    if (flags & 0x0002u) { skipCString(); skipCString(); }

    struct FolderInfo {
        std::uint32_t coffCabStart = 0;
        std::uint16_t cCFData = 0;
        std::uint16_t typeCompress = 0;
        std::vector<std::uint8_t> uncompressedStream;
    };
    std::vector<FolderInfo> folders;
    folders.reserve(cFolders);
    for (std::uint16_t i = 0; i < cFolders; ++i) {
        if (pos + 8 + cbCFFolder > size) return false;
        FolderInfo fi{};
        fi.coffCabStart = ReadLe32(data + pos);
        fi.cCFData = ReadLe16(data + pos + 4);
        fi.typeCompress = ReadLe16(data + pos + 6);
        pos += 8 + cbCFFolder;
        folders.push_back(fi);
    }

    // Decompress each folder's CFDATA blocks
    for (auto& fld : folders) {
        std::size_t dPos = fld.coffCabStart;
        const std::uint16_t compMethod = fld.typeCompress & 0x000Fu;
        std::vector<std::uint8_t> prevDict;
        for (std::uint16_t b = 0; b < fld.cCFData; ++b) {
            if (dPos + 8 + cbCFData > size) break;
            const std::uint16_t cbData = ReadLe16(data + dPos + 4);
            const std::uint16_t cbUncomp = ReadLe16(data + dPos + 6);
            dPos += 8 + cbCFData;
            if (dPos + cbData > size) break;
            const std::uint8_t* blockPtr = data + dPos;
            dPos += cbData;

            if (compMethod == 0) { // NONE
                fld.uncompressedStream.insert(fld.uncompressedStream.end(), blockPtr, blockPtr + cbData);
            } else if (compMethod == 1) { // MSZIP ("CK" + raw deflate)
                if (cbData < 2 || blockPtr[0] != 'C' || blockPtr[1] != 'K') continue;
                z_stream zs{};
                if (inflateInit2(&zs, -15) != Z_OK) continue;
                if (!prevDict.empty()) {
                    inflateSetDictionary(&zs, prevDict.data(), static_cast<uInt>(prevDict.size()));
                }
                zs.next_in = const_cast<Bytef*>(blockPtr + 2);
                zs.avail_in = static_cast<uInt>(cbData - 2);
                std::vector<std::uint8_t> outBlock((std::max<std::size_t>)(cbUncomp, 32768u));
                zs.next_out = outBlock.data();
                zs.avail_out = static_cast<uInt>(outBlock.size());
                const int zr = inflate(&zs, Z_FINISH);
                const std::size_t got = outBlock.size() - zs.avail_out;
                inflateEnd(&zs);
                if (zr == Z_STREAM_END || zr == Z_OK || got > 0) {
                    outBlock.resize(got);
                    fld.uncompressedStream.insert(fld.uncompressedStream.end(), outBlock.begin(), outBlock.end());
                    const std::size_t keep = (std::min<std::size_t>)(outBlock.size(), 32768u);
                    prevDict.assign(outBlock.end() - keep, outBlock.end());
                }
            }
        }
    }

    // Read CFFILE table
    std::size_t fPos = coffFiles;
    for (std::uint16_t i = 0; i < cFiles; ++i) {
        if (fPos + 16 > size) break;
        const std::uint32_t cbFile = ReadLe32(data + fPos);
        const std::uint32_t uoffFolderStart = ReadLe32(data + fPos + 4);
        const std::uint16_t iFolder = ReadLe16(data + fPos + 8);
        fPos += 16;
        std::string name;
        while (fPos < size && data[fPos] != 0) {
            name.push_back(static_cast<char>(data[fPos++]));
        }
        if (fPos < size) ++fPos;

        if (iFolder < folders.size()) {
            const auto& stream = folders[iFolder].uncompressedStream;
            if (uoffFolderStart <= stream.size()) {
                const std::size_t avail = stream.size() - uoffFolderStart;
                const std::size_t copyLen = (std::min<std::size_t>)(cbFile, avail);
                CabFileEntry entry{};
                entry.filename = std::move(name);
                entry.data.assign(stream.begin() + uoffFolderStart, stream.begin() + uoffFolderStart + copyLen);
                outFiles.push_back(std::move(entry));
            }
        }
    }
    return !outFiles.empty();
}

// ============================================================================
// 4. InkML <trace> & Base64 Helper (OneNote / OpenXML Ink extraction)
// ============================================================================
inline std::vector<std::vector<InkPoint>> ParseInkMlTraces(const std::string& xmlText) {
    std::vector<std::vector<InkPoint>> strokes;
    std::size_t pos = 0;
    while (pos < xmlText.size()) {
        const std::size_t open = xmlText.find("<trace", pos);
        if (open == std::string::npos) break;
        const std::size_t gt = xmlText.find('>', open);
        if (gt == std::string::npos) break;
        const std::size_t close = xmlText.find("</trace>", gt + 1);
        if (close == std::string::npos) break;
        const std::string body = xmlText.substr(gt + 1, close - (gt + 1));
        pos = close + 8;

        std::vector<InkPoint> pts;
        std::size_t p = 0;
        while (p < body.size()) {
            const std::size_t comma = body.find(',', p);
            const std::string tok = body.substr(p, comma == std::string::npos ? std::string::npos : (comma - p));
            p = (comma == std::string::npos) ? body.size() : (comma + 1);
            char* p1 = nullptr;
            char* p2 = nullptr;
            char* p3 = nullptr;
            const double x = std::strtod(tok.c_str(), &p1);
            const bool hasX = (p1 && p1 != tok.c_str());
            const double y = hasX ? std::strtod(p1, &p2) : 0.0;
            const bool hasY = (hasX && p2 && p2 != p1);
            const double f = hasY ? std::strtod(p2, &p3) : 0.55;
            const bool hasF = (hasY && p3 && p3 != p2);
            if (hasY && std::isfinite(x) && std::isfinite(y)) {
                InkPoint pt{};
                pt.x = x;
                pt.y = y;
                pt.pressure = static_cast<float>(std::clamp(hasF ? (f > 1.5 ? f / 255.0 : f) : 0.55, 0.1, 1.0));
                pts.push_back(pt);
            }
        }
        if (pts.size() >= 2) strokes.push_back(std::move(pts));
    }
    return strokes;
}

} // namespace DocStudio::Interop::Binary
