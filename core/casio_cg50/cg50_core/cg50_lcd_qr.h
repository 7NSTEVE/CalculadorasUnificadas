#pragma once

#include <cstdint>
#include <string>

namespace cg50 {

// ============================================================================
// Native zero-dependency QR Code decoder for razor-sharp digital LCD frames
// (384x216 RGBA8888, Casio fx-CG50 SHIFT+OPTN QR codes V1..V10).
// Decodes Byte/Alphanumeric/Numeric payloads with full ISO-18004 format BCH,
// unmasking (masks 0..7), and GF(256) Reed-Solomon verification/correction.
// ============================================================================
bool DecodeLcdQrRgba(const std::uint8_t* rgba,
                     int width,
                     int height,
                     std::string& outText);

} // namespace cg50
