#pragma once
#include "common.h"
#include <set>

namespace josts {
enum class HostsState { Original, Patched, ThirdParty };
struct Snapshot {
    std::set<std::wstring> own;
    std::set<std::wstring> foreign;
    std::set<std::wstring> mappings; // Active IP/domain pairs, for read-back verification.
    std::set<std::wstring> own_mappings, foreign_mappings;
    std::string revision; // SHA-256 of the global file, never of a portable preset.
    FILETIME write_time{};
    HostsState state = HostsState::Original;
};
class HostsManager {
public:
    HostsManager(const std::wstring& custom_path=L"", const std::wstring& custom_portable_backup=L"");
    const std::wstring& path() const { return path_; }
    bool snapshot(Snapshot& out,std::wstring& error);
    bool apply(const std::vector<Entry>& entries,std::wstring& error,const std::string& expected_revision="");
    bool remove_own(std::wstring& error,const std::string& expected_revision="");
    bool restore(std::wstring& error,const std::string& expected_revision="");
private:
    std::wstring path_, system_backup_;
    bool read_current(std::string& bytes,TextFile& text,std::wstring& error);
    bool ensure_backups(const std::string& original,std::wstring& error);
    bool atomic_replace(const std::string& expected,const std::string& replacement,std::wstring& error);
};
}
