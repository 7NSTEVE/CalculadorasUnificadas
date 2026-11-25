#include "inflate.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace academic::tec_digital::detail {

namespace {

constexpr int kMaxBits = 15;
constexpr int kFixLitLenSyms = 288;
constexpr int kMaxDistSyms = 32;

struct BitReader {
    const uint8_t* data = nullptr;
    size_t size = 0;
    size_t byte_pos = 0;
    uint32_t bit_buf = 0;
    int bit_cnt = 0;

    bool Ensure(int n) {
        while (bit_cnt < n) {
            if (byte_pos >= size) {
                return false;
            }
            bit_buf |= static_cast<uint32_t>(data[byte_pos++]) << bit_cnt;
            bit_cnt += 8;
        }
        return true;
    }

    bool ReadBits(int n, uint32_t& out) {
        if (n == 0) {
            out = 0;
            return true;
        }
        if (!Ensure(n)) {
            return false;
        }
        out = bit_buf & ((1u << n) - 1u);
        bit_buf >>= n;
        bit_cnt -= n;
        return true;
    }

    void AlignToByte() {
        int discard = bit_cnt & 7;
        bit_buf >>= discard;
        bit_cnt -= discard;
        while (bit_cnt >= 8) {
            --byte_pos;
            bit_cnt -= 8;
        }
        bit_buf = 0;
        bit_cnt = 0;
    }

    bool ReadU16LE(uint16_t& out) {
        if (byte_pos + 2 > size) {
            return false;
        }
        out = static_cast<uint16_t>(data[byte_pos]) |
              (static_cast<uint16_t>(data[byte_pos + 1]) << 8);
        byte_pos += 2;
        return true;
    }
};

struct HuffmanTable {
    std::array<int, kMaxBits + 1> count{};
    std::array<uint16_t, 320> symbol{};
    bool has_symbols = false;

    bool Build(const int* lengths, int num_syms, bool allow_empty) {
        count.fill(0);
        symbol.fill(0);
        has_symbols = false;

        if (num_syms <= 0 || num_syms > static_cast<int>(symbol.size())) {
            return false;
        }

        for (int i = 0; i < num_syms; ++i) {
            int len = lengths[i];
            if (len < 0 || len > kMaxBits) {
                return false;
            }
            if (len > 0) {
                ++count[len];
                has_symbols = true;
            }
        }

        if (!has_symbols) {
            return allow_empty;
        }

        int left = 1;
        for (int len = 1; len <= kMaxBits; ++len) {
            left <<= 1;
            left -= count[len];
            if (left < 0) {
                return false;  // Oversubscribed Huffman tree
            }
        }

        std::array<int, kMaxBits + 1> offs{};
        offs[1] = 0;
        for (int len = 1; len < kMaxBits; ++len) {
            offs[len + 1] = offs[len] + count[len];
        }

        for (int sym = 0; sym < num_syms; ++sym) {
            int len = lengths[sym];
            if (len != 0) {
                symbol[offs[len]++] = static_cast<uint16_t>(sym);
            }
        }
        return true;
    }

    bool Decode(BitReader& br, int& out_sym) const {
        if (!has_symbols) {
            return false;
        }
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len <= kMaxBits; ++len) {
            uint32_t bit = 0;
            if (!br.ReadBits(1, bit)) {
                return false;
            }
            code |= static_cast<int>(bit);
            int c = count[len];
            if (code - c < first) {
                out_sym = symbol[index + (code - first)];
                return true;
            }
            index += c;
            first += c;
            first <<= 1;
            code <<= 1;
        }
        return false;
    }
};

void BuildFixedTables(HuffmanTable& litlen, HuffmanTable& dist) {
    std::array<int, kFixLitLenSyms> ll_lens{};
    int sym = 0;
    for (; sym < 144; ++sym) ll_lens[sym] = 8;
    for (; sym < 256; ++sym) ll_lens[sym] = 9;
    for (; sym < 280; ++sym) ll_lens[sym] = 7;
    for (; sym < 288; ++sym) ll_lens[sym] = 8;
    litlen.Build(ll_lens.data(), kFixLitLenSyms, false);

    std::array<int, kMaxDistSyms> d_lens{};
    d_lens.fill(5);
    dist.Build(d_lens.data(), kMaxDistSyms, false);
}

uint32_t ComputeAdler32(const uint8_t* data, size_t len) {
    constexpr uint32_t kMod = 65521u;
    uint32_t s1 = 1;
    uint32_t s2 = 0;
    for (size_t i = 0; i < len; ++i) {
        s1 += data[i];
        if (s1 >= kMod) s1 -= kMod;
        s2 += s1;
        if (s2 >= kMod) s2 -= kMod;
    }
    return (s2 << 16) | s1;
}

uint32_t ComputeCrc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

}  // namespace

Result<std::vector<uint8_t>> InflateRawDeflateImpl(const uint8_t* data,
                                                   size_t len,
                                                   size_t max_output_bytes,
                                                   size_t* out_bytes_consumed) {
    if (data == nullptr && len > 0) {
        return Result<std::vector<uint8_t>>::Failure("invalid_input", "null deflate buffer");
    }

    static constexpr uint16_t kLenBase[29] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13,
        15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
        67, 83, 99, 115, 131, 163, 195, 227, 258
    };
    static constexpr uint8_t kLenExtra[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
        1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
        4, 4, 4, 4, 5, 5, 5, 5, 0
    };
    static constexpr uint16_t kDistBase[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25,
        33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
    };
    static constexpr uint8_t kDistExtra[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3,
        4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
        9, 9, 10, 10, 11, 11, 12, 12, 13, 13
    };
    static constexpr int kCodeLenOrder[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };

    BitReader br;
    br.data = data;
    br.size = len;

    std::vector<uint8_t> out;
    uint32_t bfinal = 0;

    do {
        uint32_t btype = 0;
        if (!br.ReadBits(1, bfinal) || !br.ReadBits(2, btype)) {
            return Result<std::vector<uint8_t>>::Failure("truncated_deflate",
                                                        "unexpected EOF in deflate block header");
        }

        if (btype == 0) {
            br.AlignToByte();
            uint16_t slen = 0;
            uint16_t snlen = 0;
            if (!br.ReadU16LE(slen) || !br.ReadU16LE(snlen)) {
                return Result<std::vector<uint8_t>>::Failure("truncated_stored_block",
                                                            "unexpected EOF in stored block header");
            }
            if (static_cast<uint16_t>(slen ^ 0xFFFFu) != snlen) {
                return Result<std::vector<uint8_t>>::Failure("corrupt_stored_block",
                                                            "stored block LEN/NLEN mismatch");
            }
            if (br.byte_pos + slen > br.size) {
                return Result<std::vector<uint8_t>>::Failure("truncated_stored_block",
                                                            "stored block exceeds input");
            }
            if (out.size() + slen > max_output_bytes) {
                return Result<std::vector<uint8_t>>::Failure("output_limit_exceeded",
                                                            "decompressed size exceeds limit");
            }
            out.insert(out.end(), br.data + br.byte_pos, br.data + br.byte_pos + slen);
            br.byte_pos += slen;
        } else if (btype == 1 || btype == 2) {
            HuffmanTable litlen_tbl;
            HuffmanTable dist_tbl;

            if (btype == 1) {
                BuildFixedTables(litlen_tbl, dist_tbl);
            } else {
                uint32_t hlit_raw = 0;
                uint32_t hdist_raw = 0;
                uint32_t hclen_raw = 0;
                if (!br.ReadBits(5, hlit_raw) || !br.ReadBits(5, hdist_raw) ||
                    !br.ReadBits(4, hclen_raw)) {
                    return Result<std::vector<uint8_t>>::Failure("truncated_deflate",
                                                                "truncated dynamic block counts");
                }
                const int hlit = static_cast<int>(hlit_raw) + 257;
                const int hdist = static_cast<int>(hdist_raw) + 1;
                const int hclen = static_cast<int>(hclen_raw) + 4;
                if (hlit > 286 || hdist > 32 || hclen > 19) {
                    return Result<std::vector<uint8_t>>::Failure("invalid_dynamic_header",
                                                                "invalid dynamic Huffman counts");
                }

                std::array<int, 19> cl_lens{};
                for (int i = 0; i < hclen; ++i) {
                    uint32_t v = 0;
                    if (!br.ReadBits(3, v)) {
                        return Result<std::vector<uint8_t>>::Failure(
                            "truncated_deflate", "truncated code-length table");
                    }
                    cl_lens[kCodeLenOrder[i]] = static_cast<int>(v);
                }

                HuffmanTable cl_tbl;
                if (!cl_tbl.Build(cl_lens.data(), 19, false)) {
                    return Result<std::vector<uint8_t>>::Failure("invalid_huffman",
                                                                "invalid code-length Huffman tree");
                }

                std::array<int, 286 + 32> lens{};
                const int total_codes = hlit + hdist;
                int idx = 0;
                while (idx < total_codes) {
                    int sym = 0;
                    if (!cl_tbl.Decode(br, sym)) {
                        return Result<std::vector<uint8_t>>::Failure(
                            "invalid_huffman", "failed decoding code length symbol");
                    }
                    if (sym < 16) {
                        lens[idx++] = sym;
                    } else {
                        int repeat = 0;
                        int val = 0;
                        uint32_t extra = 0;
                        if (sym == 16) {
                            if (idx == 0 || !br.ReadBits(2, extra)) {
                                return Result<std::vector<uint8_t>>::Failure(
                                    "invalid_huffman", "invalid repeat-previous code");
                            }
                            val = lens[idx - 1];
                            repeat = 3 + static_cast<int>(extra);
                        } else if (sym == 17) {
                            if (!br.ReadBits(3, extra)) {
                                return Result<std::vector<uint8_t>>::Failure(
                                    "truncated_deflate", "truncated repeat-zero code");
                            }
                            repeat = 3 + static_cast<int>(extra);
                        } else if (sym == 18) {
                            if (!br.ReadBits(7, extra)) {
                                return Result<std::vector<uint8_t>>::Failure(
                                    "truncated_deflate", "truncated long repeat-zero code");
                            }
                            repeat = 11 + static_cast<int>(extra);
                        } else {
                            return Result<std::vector<uint8_t>>::Failure(
                                "invalid_huffman", "unknown code-length symbol");
                        }
                        if (idx + repeat > total_codes) {
                            return Result<std::vector<uint8_t>>::Failure(
                                "invalid_huffman", "code-length repeat overflows table");
                        }
                        for (int r = 0; r < repeat; ++r) {
                            lens[idx++] = val;
                        }
                    }
                }

                if (lens[256] == 0) {
                    return Result<std::vector<uint8_t>>::Failure(
                        "invalid_huffman", "end-of-block symbol missing from dynamic tree");
                }
                if (!litlen_tbl.Build(lens.data(), hlit, false) ||
                    !dist_tbl.Build(lens.data() + hlit, hdist, true)) {
                    return Result<std::vector<uint8_t>>::Failure(
                        "invalid_huffman", "invalid dynamic literal/distance Huffman tree");
                }
            }

            while (true) {
                int sym = 0;
                if (!litlen_tbl.Decode(br, sym)) {
                    return Result<std::vector<uint8_t>>::Failure("invalid_huffman",
                                                                "failed decoding literal/length symbol");
                }
                if (sym < 256) {
                    if (out.size() >= max_output_bytes) {
                        return Result<std::vector<uint8_t>>::Failure(
                            "output_limit_exceeded", "decompressed size exceeds limit");
                    }
                    out.push_back(static_cast<uint8_t>(sym));
                } else if (sym == 256) {
                    break;
                } else if (sym <= 285) {
                    int lidx = sym - 257;
                    uint32_t lextra = 0;
                    if (!br.ReadBits(kLenExtra[lidx], lextra)) {
                        return Result<std::vector<uint8_t>>::Failure("truncated_deflate",
                                                                    "truncated length extra bits");
                    }
                    size_t match_len = static_cast<size_t>(kLenBase[lidx]) + lextra;

                    int dsym = 0;
                    if (!dist_tbl.Decode(br, dsym) || dsym >= 30) {
                        return Result<std::vector<uint8_t>>::Failure("invalid_huffman",
                                                                    "invalid distance symbol");
                    }
                    uint32_t dextra = 0;
                    if (!br.ReadBits(kDistExtra[dsym], dextra)) {
                        return Result<std::vector<uint8_t>>::Failure("truncated_deflate",
                                                                    "truncated distance extra bits");
                    }
                    size_t dist = static_cast<size_t>(kDistBase[dsym]) + dextra;
                    if (dist == 0 || dist > out.size()) {
                        return Result<std::vector<uint8_t>>::Failure("invalid_distance",
                                                                    "back-reference before start of stream");
                    }
                    if (out.size() + match_len > max_output_bytes) {
                        return Result<std::vector<uint8_t>>::Failure(
                            "output_limit_exceeded", "decompressed size exceeds limit");
                    }
                    for (size_t k = 0; k < match_len; ++k) {
                        out.push_back(out[out.size() - dist]);
                    }
                } else {
                    return Result<std::vector<uint8_t>>::Failure("invalid_symbol",
                                                                "reserved literal/length symbol");
                }
            }
        } else {
            return Result<std::vector<uint8_t>>::Failure("invalid_btype",
                                                        "reserved deflate block type 3");
        }
    } while (!bfinal);

    br.AlignToByte();
    if (out_bytes_consumed != nullptr) {
        *out_bytes_consumed = br.byte_pos;
    }
    return Result<std::vector<uint8_t>>::Success(std::move(out));
}

Result<std::vector<uint8_t>> InflateZlibImpl(const uint8_t* data,
                                             size_t len,
                                             size_t max_output_bytes) {
    if (data == nullptr || len < 6) {
        return Result<std::vector<uint8_t>>::Failure("invalid_zlib", "zlib buffer too short");
    }
    const uint8_t cmf = data[0];
    const uint8_t flg = data[1];
    if ((cmf & 0x0Fu) != 8u || (cmf >> 4) > 7u) {
        return Result<std::vector<uint8_t>>::Failure("invalid_zlib", "unsupported zlib CMF");
    }
    if ((((static_cast<uint16_t>(cmf) << 8) | flg) % 31u) != 0u) {
        return Result<std::vector<uint8_t>>::Failure("invalid_zlib", "zlib header check bits invalid");
    }
    if ((flg & 0x20u) != 0u) {
        return Result<std::vector<uint8_t>>::Failure("unsupported_zlib", "zlib FDICT not supported");
    }

    size_t consumed = 0;
    auto res = InflateRawDeflateImpl(data + 2, len - 2, max_output_bytes, &consumed);
    if (!res.ok) {
        return res;
    }

    const size_t trailer_pos = 2 + consumed;
    if (trailer_pos + 4 > len) {
        return Result<std::vector<uint8_t>>::Failure("truncated_zlib", "missing Adler-32 trailer");
    }
    const uint32_t expected_adler =
        (static_cast<uint32_t>(data[trailer_pos]) << 24) |
        (static_cast<uint32_t>(data[trailer_pos + 1]) << 16) |
        (static_cast<uint32_t>(data[trailer_pos + 2]) << 8) |
        static_cast<uint32_t>(data[trailer_pos + 3]);
    const uint32_t actual_adler = ComputeAdler32(res.value.data(), res.value.size());
    if (expected_adler != actual_adler) {
        return Result<std::vector<uint8_t>>::Failure("adler32_mismatch", "zlib Adler-32 checksum mismatch");
    }
    return res;
}

Result<std::vector<uint8_t>> InflateGzipImpl(const uint8_t* data,
                                             size_t len,
                                             size_t max_output_bytes) {
    if (data == nullptr || len < 18) {
        return Result<std::vector<uint8_t>>::Failure("invalid_gzip", "gzip buffer too short");
    }
    std::vector<uint8_t> total_out;
    size_t offset = 0;
    while (offset < len) {
        if (offset + 18 > len) {
            return Result<std::vector<uint8_t>>::Failure("invalid_gzip", "truncated gzip member");
        }
        if (data[offset] != 0x1Fu || data[offset + 1] != 0x8Bu || data[offset + 2] != 0x08u) {
            return Result<std::vector<uint8_t>>::Failure("invalid_gzip", "invalid gzip header magic/method");
        }
        const uint8_t flg = data[offset + 3];
        if ((flg & 0xE0u) != 0u) {
            return Result<std::vector<uint8_t>>::Failure("invalid_gzip", "reserved gzip flags set");
        }
        size_t pos = offset + 10;
        if ((flg & 0x04u) != 0u) {  // FEXTRA
            if (pos + 2 > len) {
                return Result<std::vector<uint8_t>>::Failure("truncated_gzip", "truncated gzip FEXTRA");
            }
            uint16_t xlen = static_cast<uint16_t>(data[pos]) |
                            (static_cast<uint16_t>(data[pos + 1]) << 8);
            pos += 2;
            if (pos + xlen > len) {
                return Result<std::vector<uint8_t>>::Failure("truncated_gzip", "truncated gzip FEXTRA payload");
            }
            pos += xlen;
        }
        if ((flg & 0x08u) != 0u) {  // FNAME
            while (pos < len && data[pos] != 0) ++pos;
            if (pos >= len) {
                return Result<std::vector<uint8_t>>::Failure("truncated_gzip", "unterminated gzip FNAME");
            }
            ++pos;
        }
        if ((flg & 0x10u) != 0u) {  // FCOMMENT
            while (pos < len && data[pos] != 0) ++pos;
            if (pos >= len) {
                return Result<std::vector<uint8_t>>::Failure("truncated_gzip", "unterminated gzip FCOMMENT");
            }
            ++pos;
        }
        if ((flg & 0x02u) != 0u) {  // FHCRC
            if (pos + 2 > len) {
                return Result<std::vector<uint8_t>>::Failure("truncated_gzip", "truncated gzip FHCRC");
            }
            const uint16_t expected_hcrc =
                static_cast<uint16_t>(data[pos]) | (static_cast<uint16_t>(data[pos + 1]) << 8);
            const uint16_t actual_hcrc =
                static_cast<uint16_t>(ComputeCrc32(data + offset, pos - offset) & 0xFFFFu);
            if (expected_hcrc != actual_hcrc) {
                return Result<std::vector<uint8_t>>::Failure("crc_mismatch", "gzip header CRC16 mismatch");
            }
            pos += 2;
        }

        const size_t remaining_budget =
            (total_out.size() <= max_output_bytes) ? (max_output_bytes - total_out.size()) : 0;
        size_t consumed = 0;
        auto res = InflateRawDeflateImpl(data + pos, len - pos, remaining_budget, &consumed);
        if (!res.ok) {
            return res;
        }
        const size_t trailer_pos = pos + consumed;
        if (trailer_pos + 8 > len) {
            return Result<std::vector<uint8_t>>::Failure("truncated_gzip", "missing gzip CRC32/ISIZE trailer");
        }
        const uint32_t expected_crc =
            static_cast<uint32_t>(data[trailer_pos]) |
            (static_cast<uint32_t>(data[trailer_pos + 1]) << 8) |
            (static_cast<uint32_t>(data[trailer_pos + 2]) << 16) |
            (static_cast<uint32_t>(data[trailer_pos + 3]) << 24);
        const uint32_t expected_isize =
            static_cast<uint32_t>(data[trailer_pos + 4]) |
            (static_cast<uint32_t>(data[trailer_pos + 5]) << 8) |
            (static_cast<uint32_t>(data[trailer_pos + 6]) << 16) |
            (static_cast<uint32_t>(data[trailer_pos + 7]) << 24);
        const uint32_t actual_crc = ComputeCrc32(res.value.data(), res.value.size());
        const uint32_t actual_isize = static_cast<uint32_t>(res.value.size() & 0xFFFFFFFFu);
        if (expected_crc != actual_crc) {
            return Result<std::vector<uint8_t>>::Failure("crc_mismatch", "gzip CRC32 mismatch");
        }
        if (expected_isize != actual_isize) {
            return Result<std::vector<uint8_t>>::Failure("isize_mismatch", "gzip ISIZE mismatch");
        }
        total_out.insert(total_out.end(), res.value.begin(), res.value.end());
        offset = trailer_pos + 8;
    }
    return Result<std::vector<uint8_t>>::Success(std::move(total_out));
}

}  // namespace academic::tec_digital::detail
