#include <windows.h>
#include <cassert>
#include <cstring>
#include <cwchar>
#include <cstdio>

// Simulated Win32 calls exercise the production function without accessing HKLM.
namespace {
struct Registry {
    LSTATUS create_error=0,set_error=0,query_error=0,close_error=0;
    DWORD value=1,type=REG_DWORD,size=sizeof(DWORD);
    int creates=0,writes=0,queries=0,closes=0;
    bool mismatch=false;
} registry;
HKEY test_key=reinterpret_cast<HKEY>(static_cast<ULONG_PTR>(42));
LSTATUS WINAPI FakeCreate(HKEY root,LPCWSTR path,DWORD reserved,LPWSTR cls,DWORD options,
    REGSAM access,LPSECURITY_ATTRIBUTES security,PHKEY key,LPDWORD disposition) {
    ++registry.creates;
    assert(root==HKEY_LOCAL_MACHINE);
    assert(std::wcscmp(path,L"SOFTWARE\\Policies\\Microsoft\\Edge")==0);
    assert(access==(KEY_SET_VALUE|KEY_QUERY_VALUE|KEY_WOW64_64KEY));
    assert(!reserved&&!cls&&options==REG_OPTION_NON_VOLATILE&&!security&&!disposition);
    if(!registry.create_error) *key=test_key;
    return registry.create_error;
}
LSTATUS WINAPI FakeSet(HKEY key,LPCWSTR name,DWORD reserved,DWORD type,const BYTE* bytes,DWORD size) {
    ++registry.writes;
    assert(key==test_key&&!reserved&&type==REG_DWORD&&size==sizeof(DWORD));
    assert(std::wcscmp(name,L"NewTabPageContentEnabled")==0);
    DWORD value=1; std::memcpy(&value,bytes,size); assert(value==0);
    if(!registry.set_error) registry.value=value;
    return registry.set_error;
}
LSTATUS WINAPI FakeQuery(HKEY key,LPCWSTR name,LPDWORD reserved,LPDWORD type,LPBYTE bytes,LPDWORD size) {
    ++registry.queries;
    assert(key==test_key&&!reserved&&*size==sizeof(DWORD));
    assert(std::wcscmp(name,L"NewTabPageContentEnabled")==0);
    const DWORD value=registry.mismatch?1:registry.value;
    std::memcpy(bytes,&value,sizeof(value)); *type=registry.type; *size=registry.size;
    return registry.query_error;
}
LSTATUS WINAPI FakeClose(HKEY key) {
    assert(key==test_key); ++registry.closes;
    SetLastError(999); // Must not hide the earlier operation's result.
    return registry.close_error;
}
}
#define RegCreateKeyExW FakeCreate
#define RegSetValueExW FakeSet
#define RegQueryValueExW FakeQuery
#define RegCloseKey FakeClose
#include "../src/edge_settings.cpp"
#undef RegCreateKeyExW
#undef RegSetValueExW
#undef RegQueryValueExW
#undef RegCloseKey

int main() {
    using josts::BloquearContenidoNoticiasEdge;
    assert(BloquearContenidoNoticiasEdge()&&GetLastError()==ERROR_SUCCESS);
    assert(registry.value==0&&registry.creates==1&&registry.writes==1&&registry.queries==1&&registry.closes==1);
    assert(BloquearContenidoNoticiasEdge()&&registry.value==0&&registry.closes==2);
    registry=Registry{}; registry.create_error=ERROR_ACCESS_DENIED;
    assert(!BloquearContenidoNoticiasEdge()&&GetLastError()==ERROR_ACCESS_DENIED);
    assert(!registry.writes&&!registry.queries&&!registry.closes);
    registry=Registry{}; registry.set_error=ERROR_ACCESS_DENIED;
    assert(!BloquearContenidoNoticiasEdge()&&GetLastError()==ERROR_ACCESS_DENIED);
    assert(registry.closes==1&&!registry.queries&&registry.value==1);
    registry=Registry{}; registry.query_error=ERROR_MORE_DATA;
    assert(!BloquearContenidoNoticiasEdge()&&GetLastError()==ERROR_MORE_DATA&&registry.closes==1);
    registry=Registry{}; registry.mismatch=true;
    assert(!BloquearContenidoNoticiasEdge()&&GetLastError()==ERROR_INVALID_DATA);
    registry=Registry{}; registry.type=REG_SZ;
    assert(!BloquearContenidoNoticiasEdge()&&GetLastError()==ERROR_INVALID_DATA);
    registry=Registry{}; registry.size=1;
    assert(!BloquearContenidoNoticiasEdge()&&GetLastError()==ERROR_INVALID_DATA);
    registry=Registry{}; registry.close_error=ERROR_INVALID_HANDLE;
    assert(!BloquearContenidoNoticiasEdge()&&GetLastError()==ERROR_INVALID_HANDLE);
    std::puts("Edge policy: native view, DWORD zero, verification, repeated apply and error handling passed without registry writes.");
}
