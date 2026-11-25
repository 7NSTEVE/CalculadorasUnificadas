#pragma once

#include "academic/tec_digital/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace academic::tec_digital::detail {

Result<std::vector<uint8_t>> InflateRawDeflateImpl(const uint8_t* data,
                                                   size_t len,
                                                   size_t max_output_bytes,
                                                   size_t* out_bytes_consumed = nullptr);

Result<std::vector<uint8_t>> InflateZlibImpl(const uint8_t* data,
                                             size_t len,
                                             size_t max_output_bytes);

Result<std::vector<uint8_t>> InflateGzipImpl(const uint8_t* data,
                                             size_t len,
                                             size_t max_output_bytes);

}  // namespace academic::tec_digital::detail
