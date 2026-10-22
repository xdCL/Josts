#include "common.h"
#include <algorithm>
#include <cwctype>
#include <bcrypt.h>
#include <stdexcept>

namespace josts {
std::string sha256(const std::string& bytes) {
    BCRYPT_ALG_HANDLE alg=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
    unsigned char digest[32]{};
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) throw std::runtime_error("SHA256");
    NTSTATUS status=BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0);
    if(status>=0) status=BCryptHashData(hash,(PUCHAR)bytes.data(),static_cast<ULONG>(bytes.size()),0);
    if(status>=0) status=BCryptFinishHash(hash,digest,sizeof(digest),0);
    if(hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg,0);
    if(status<0) throw std::runtime_error("SHA256");
    std::string result; for(auto b:digest) { result+="0123456789abcdef"[b>>4]; result+="0123456789abcdef"[b&15]; }
    return result;
}
std::wstring random_id() {
    unsigned char bytes[16];
    if(BCryptGenRandom(nullptr,bytes,sizeof(bytes),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) throw std::runtime_error("Random");
    std::wstring result; for(auto b:bytes) { result+=L"0123456789abcdef"[b>>4]; result+=L"0123456789abcdef"[b&15]; }
    return result;
}
std::wstring trim(const std::wstring& s) {
    const size_t a=s.find_first_not_of(L" \t\r\n");
    if(a==std::wstring::npos) return L"";
    return s.substr(a,s.find_last_not_of(L" \t\r\n")-a+1);
}
std::wstring lower(std::wstring s) { std::transform(s.begin(),s.end(),s.begin(),towlower); return s; }
std::wstring error_message(DWORD code) {
    LPWSTR buffer=nullptr;
    const DWORD flags=FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD n=FormatMessageW(flags,nullptr,code,language_id(),(LPWSTR)&buffer,0,nullptr);
    if(!n) n=FormatMessageW(flags,nullptr,code,0,(LPWSTR)&buffer,0,nullptr);
    std::wstring result=n?trim(std::wstring(buffer,n)):L"Error "+std::to_wstring(code);
    if(buffer) LocalFree(buffer);
    return result;
}
std::wstring executable_dir() {
    std::vector<wchar_t> path(MAX_PATH);
    for(;;) {
        DWORD n=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
        if(!n) return L".";
        if(n<path.size()-1) { std::wstring s(path.data(),n); return s.substr(0,s.find_last_of(L"\\/")); }
        path.resize(path.size()*2);
    }
}
std::wstring join(const std::wstring& a,const std::wstring& b) { return a+(a.empty()||a.back()==L'\\'?L"":L"\\")+b; }
bool exists(const std::wstring& path) { return GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES; }
bool ensure_dir(const std::wstring& path) { return CreateDirectoryW(path.c_str(),nullptr)||GetLastError()==ERROR_ALREADY_EXISTS; }
bool read_bytes(const std::wstring& path,std::string& out,DWORD& error) {
    HANDLE f=INVALID_HANDLE_VALUE;
    for(int i=0;i<5;i++) {
        f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(f!=INVALID_HANDLE_VALUE) break;
        error=GetLastError(); if(error!=ERROR_SHARING_VIOLATION) return false; Sleep(200);
    }
    if(f==INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(f,&size)||size.QuadPart<0||size.QuadPart>64*1024*1024) { error=GetLastError(); if(!error) error=ERROR_FILE_TOO_LARGE; CloseHandle(f); return false; }
    out.resize(static_cast<size_t>(size.QuadPart));
    size_t done=0;
    while(done<out.size()) { DWORD got=0; DWORD want=static_cast<DWORD>(std::min<size_t>(out.size()-done,1024*1024)); if(!ReadFile(f,&out[done],want,&got,nullptr)||got==0) { error=GetLastError(); if(!error) error=ERROR_HANDLE_EOF; CloseHandle(f); return false; } done+=got; }
    CloseHandle(f); error=ERROR_SUCCESS; return true;
}
bool write_bytes(const std::wstring& path,const std::string& data,DWORD& error) {
    HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE) { error=GetLastError(); return false; }
    size_t done=0;
    while(done<data.size()) { DWORD written=0; DWORD n=static_cast<DWORD>(std::min<size_t>(data.size()-done,1024*1024)); if(!WriteFile(f,data.data()+done,n,&written,nullptr)||!written) { error=GetLastError(); CloseHandle(f); return false; } done+=written; }
    if(!FlushFileBuffers(f)) { error=GetLastError(); CloseHandle(f); return false; }
    CloseHandle(f); error=ERROR_SUCCESS; return true;
}
static bool convert(UINT cp,DWORD flags,const char* p,int n,std::wstring& out) {
    if(!n) { out.clear(); return true; }
    int count=MultiByteToWideChar(cp,flags,p,n,nullptr,0);
    if(!count) return false;
    out.resize(count);
    return MultiByteToWideChar(cp,flags,p,n,&out[0],count)==count;
}
bool decode(const std::string& bytes,TextFile& out) {
    const unsigned char* b=reinterpret_cast<const unsigned char*>(bytes.data()); size_t n=bytes.size();
    if(n>=2&&((b[0]==0xff&&b[1]==0xfe)||(b[0]==0xfe&&b[1]==0xff))) {
        if((n-2)%2) return false;
        out.encoding=b[0]==0xff?Encoding::Utf16Le:Encoding::Utf16Be;
        out.text.resize((n-2)/2);
        for(size_t i=2,j=0;i<n;i+=2,++j) out.text[j]=static_cast<wchar_t>(out.encoding==Encoding::Utf16Le?(b[i]|(b[i+1]<<8)):((b[i]<<8)|b[i+1]));
    } else if(n>=3&&b[0]==0xef&&b[1]==0xbb&&b[2]==0xbf) {
        out.encoding=Encoding::Utf8Bom;
        if(!convert(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data()+3,static_cast<int>(n-3),out.text)) return false;
    } else if(convert(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(n),out.text)) out.encoding=Encoding::Utf8;
    else { out.encoding=Encoding::Ansi; if(!convert(CP_ACP,0,bytes.data(),static_cast<int>(n),out.text)) return false; }
    out.newline=out.text.find(L"\r\n")!=std::wstring::npos?L"\r\n":(out.text.find(L'\n')!=std::wstring::npos?L"\n":L"\r\n");
    return true;
}
static bool to_multibyte(UINT cp,DWORD flags,const std::wstring& s,std::string& out) {
    if(s.empty()) { out.clear(); return true; }
    BOOL replaced=FALSE;
    BOOL* used=cp==CP_ACP?&replaced:nullptr;
    int count=WideCharToMultiByte(cp,flags,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,used);
    if(!count) return false;
    out.resize(count);
    replaced=FALSE;
    return WideCharToMultiByte(cp,flags,s.data(),static_cast<int>(s.size()),&out[0],count,nullptr,used)==count&&!replaced;
}
bool encode(const TextFile& source,const std::wstring& text,std::string& out) {
    if(source.encoding==Encoding::Utf16Le||source.encoding==Encoding::Utf16Be) {
        out.clear(); out.reserve(text.size()*2+2);
        out.push_back(source.encoding==Encoding::Utf16Le?char(0xff):char(0xfe)); out.push_back(source.encoding==Encoding::Utf16Le?char(0xfe):char(0xff));
        for(wchar_t ch:text) { if(source.encoding==Encoding::Utf16Le) { out.push_back(char(ch&255)); out.push_back(char(ch>>8)); } else { out.push_back(char(ch>>8)); out.push_back(char(ch&255)); } }
        return true;
    }
    if(!to_multibyte(source.encoding==Encoding::Ansi?CP_ACP:CP_UTF8,source.encoding==Encoding::Ansi?0:WC_ERR_INVALID_CHARS,text,out)) return false;
    if(source.encoding==Encoding::Utf8Bom) out.insert(0,"\xef\xbb\xbf",3);
    return true;
}
std::string utf8(const std::wstring& s) { std::string out; to_multibyte(CP_UTF8,0,s,out); return out; }
}
