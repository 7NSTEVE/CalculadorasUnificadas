#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace academic::tec_digital {

std::array<uint8_t, 32> Sha256Bytes(const uint8_t* data, size_t len);
std::array<uint8_t, 32> Sha256Bytes(std::string_view data);

std::string Sha256Hex(const uint8_t* data, size_t len);
std::string Sha256Hex(std::string_view data);
std::string Sha256FileHex(const std::string& path, std::string& out_error);

std::string Sha1Hex(const uint8_t* data, size_t len);
std::string Sha1Hex(std::string_view data);

std::vector<uint8_t> RandomBytes(size_t count);

std::string Base64Encode(const uint8_t* data, size_t len);
std::string Base64Encode(std::string_view data);
bool Base64Decode(std::string_view encoded, std::vector<uint8_t>& out_bytes);

void SecureZeroMemoryBuffer(void* ptr, size_t len);
void SecureZeroString(std::string& s);
void SecureZeroBytes(std::vector<uint8_t>& b);

}  // namespace academic::tec_digital
