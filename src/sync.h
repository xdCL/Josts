#pragma once
#include <windows.h>
#include <string>

namespace josts {
// Handles Josts Sync command-line modes before the normal UI/elevated broker.
// Returns false for a normal interactive launch.
bool sync_entry(int& exit_code);
// One-click UI entrypoint. Requests UAC once when needed, installs the protected
// ProgramData copy and schedules the hourly silent synchronization.
bool request_sync_install(HWND owner,DWORD& exit_code,std::wstring& error);
}
