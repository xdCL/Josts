#include "common.h"
#include <mutex>

namespace josts {
void log(const std::wstring& message) {
    static std::mutex guard;
    std::lock_guard<std::mutex> lock(guard);
    std::wstring dir=join(executable_dir(),L"logs");
    if(!ensure_dir(dir)) return;
    std::wstring path=join(dir,L"josts.log");
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if(GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&info)&&info.nFileSizeLow>1024*1024) {
        std::wstring old=path+L".1";
        DeleteFileW(old.c_str()); MoveFileExW(path.c_str(),old.c_str(),MOVEFILE_REPLACE_EXISTING);
    }
    HANDLE file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size{}; GetFileSizeEx(file,&size);
    SYSTEMTIME time{}; GetLocalTime(&time);
    wchar_t stamp[64]; wsprintfW(stamp,L"[%04u-%02u-%02u %02u:%02u:%02u] ",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond);
    std::wstring line=(size.QuadPart==0?tr(L"Josts v1.1.1 — desarrollado por xdCL\r\n"):L"");
    line+=stamp+message+L"\r\n";
    std::string bytes=utf8(line);
    DWORD done=0; WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&done,nullptr);
    CloseHandle(file);
}
}
