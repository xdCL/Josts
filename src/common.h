#pragma once
#include "i18n.h"
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>

// Josts — desarrollado por xdCL.
namespace josts {
struct Entry {
    std::wstring ip;
    std::wstring domain;
    bool modified = false;
};
enum class Encoding { Utf8, Utf8Bom, Utf16Le, Utf16Be, Ansi };
struct TextFile {
    std::wstring text;
    Encoding encoding = Encoding::Utf8;
    std::wstring newline = L"\r\n";
};
std::wstring trim(const std::wstring& s);
std::wstring lower(std::wstring s);
std::wstring error_message(DWORD code);
std::wstring executable_dir();
std::wstring join(const std::wstring& a, const std::wstring& b);
bool exists(const std::wstring& path);
bool ensure_dir(const std::wstring& path);
bool read_bytes(const std::wstring& path, std::string& out, DWORD& error);
bool write_bytes(const std::wstring& path, const std::string& data, DWORD& error);
std::string sha256(const std::string& bytes);
std::wstring random_id();
bool decode(const std::string& bytes, TextFile& out);
bool encode(const TextFile& source, const std::wstring& text, std::string& out);
std::string utf8(const std::wstring& s);
void log(const std::wstring& message);
}
