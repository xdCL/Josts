#pragma once
#include "hosts_manager.h"

namespace josts {
enum class PrivilegedOperation : DWORD { Apply=1, Remove=2, Restore=3, Edge=4 };
struct PrivilegedRequest {
    PrivilegedOperation operation=PrivilegedOperation::Apply;
    std::string revision;
    std::vector<Entry> entries;
};
struct PrivilegedResult {
    bool ok=false;
    DWORD code=ERROR_SUCCESS;
    std::wstring message,warning;
};
bool token_elevated();
// Pure classification: denied-policy and cancellation are not reported as success.
std::wstring elevation_error(DWORD code,bool policy_denies);
PrivilegedResult request_privileged(HWND owner,const PrivilegedRequest& request);
// Runs against fixtures in tests; the production helper always constructs the system manager itself.
PrivilegedResult execute_privileged(HostsManager& manager,const PrivilegedRequest& request);
bool encode_request(const PrivilegedRequest& request,std::string& wire);
bool decode_request(const std::string& wire,PrivilegedRequest& request);
// Returns false for a normal UI launch. Handles helper arguments before the UI mutex.
bool privileged_entry(int& exit_code);
}
