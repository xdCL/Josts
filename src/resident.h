#pragma once
#include <windows.h>
#include <string>

namespace josts {

enum class ResidentState { NotInstalled, Stopped, Running, Error };

// Handles service/maintenance command-line modes before the normal UI.
// Returns false for a normal interactive launch.
bool resident_entry(int& exit_code);

ResidentState resident_state();

bool request_resident_install(HWND owner,DWORD& exit_code,std::wstring& error);
bool request_resident_stop(HWND owner,DWORD& exit_code,std::wstring& error);
bool request_resident_remove(HWND owner,DWORD& exit_code,std::wstring& error);

}
