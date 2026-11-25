#pragma once

#include "academic/tec_digital/vault.hpp"

#include <string>
#include <vector>

namespace academic::tec_digital::detail {

std::string SerializeVaultPayloadJson(const Credentials& creds,
                                      const std::vector<Cookie>& cookies,
                                      bool persist_session_cookies,
                                      std::string_view saved_at_utc);

bool DeserializeVaultPayloadJson(const uint8_t* data,
                                 size_t len,
                                 VaultPayload& out_payload,
                                 std::string& out_error);

}  // namespace academic::tec_digital::detail
