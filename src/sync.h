#pragma once
#include <windows.h>
#include <string>

namespace josts {
// Handles Josts Sync command-line modes before the normal UI/elevated broker.
// Returns false for a normal interactive launch.
bool sync_entry(int& exit_code);
// Runs one signed synchronization cycle. Intended for the protected resident service.
int sync_run_once();

// Legacy one-click entrypoint retained for compatibility with earlier v1.3 previews.
bool request_sync_install(HWND owner,DWORD& exit_code,std::wstring& error);
}
