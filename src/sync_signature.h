#pragma once
#include <string>

namespace josts {
// Verifies the RSA/SHA-256 signature carried by the Josts Sync manifest.
// The public key is compiled into Josts; no signing secret is present on clients.
bool verify_sync_signature(const std::string& version,const std::string& sha256_hex,const std::string& signature_base64);
}
