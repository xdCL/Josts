#include "privileged_ops.h"
#include <shellapi.h>
#include <cassert>
#include <iostream>

namespace josts {
static DWORD simulated_error=0;
static bool crash_before_connect=false;
bool broker_test_launch(SHELLEXECUTEINFOW* launch) {
    assert(std::wstring(launch->lpVerb)==L"runas");
    if(simulated_error) { SetLastError(simulated_error); return false; }
    // Transport integration only: the test binary uses fixtures and never elevates.
    std::wstring command=L"\""+std::wstring(launch->lpFile)+L"\" "+launch->lpParameters;
    if(crash_before_connect) command=L"\""+std::wstring(launch->lpFile)+L"\" --invalid";
    STARTUPINFOW si{}; si.cb=sizeof(si); PROCESS_INFORMATION pi{};
    if(!CreateProcessW(launch->lpFile,&command[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)) return false;
    CloseHandle(pi.hThread); launch->hProcess=pi.hProcess; return true;
}
HostsManager broker_test_manager() {
    wchar_t root[32768]={}; assert(GetEnvironmentVariableW(L"JOSTS_BROKER_FIXTURE",root,32768));
    return HostsManager(join(root,L"hosts"));
}
}
using namespace josts;
int main() {
    int code=0; if(privileged_entry(code)) return code;
    const auto root=join(executable_dir(),L"broker-"+random_id()); assert(ensure_dir(root));
    assert(SetEnvironmentVariableW(L"JOSTS_BROKER_FIXTURE",root.c_str()));
    const auto path=join(root,L"hosts"); DWORD error=0;
    const std::string original="# shared machine file\r\n127.0.0.1 external.example\r\n";
    assert(write_bytes(path,original,error));
    HostsManager manager(path); Snapshot before,after; std::wstring message;
    assert(manager.snapshot(before,message));
    PrivilegedRequest request; request.revision=before.revision;
    request.entries={{L"0.0.0.0",L"school.example",false}};
    std::string wire; assert(encode_request(request,wire));
    PrivilegedRequest decoded; assert(decode_request(wire,decoded));
    assert(decoded.entries.size()==1&&decoded.revision==request.revision);
    for(const auto& invalid:{std::string("garbage"),std::string("JOSTS-1.1\n9\n0\nx\n"),wire+"../../hosts\n",wire+"# import comment\n",wire+std::string(1,'\0')})
        assert(!decode_request(invalid,decoded));
    assert(!decode_request(std::string(8*1024*1024+1,'a'),decoded));
    auto result=request_privileged(nullptr,request);
    if(!result.ok) std::wcerr<<result.message<<L"\n";
    assert(result.ok);
    assert(manager.snapshot(after,message)&&after.own.count(L"school.example"));
    std::string bytes; assert(read_bytes(path,bytes,error));
    assert(bytes.find(original)==0);
    assert(read_bytes(path+L".bak_original",bytes,error)&&bytes==original);
    // A stale request cannot overwrite changes committed since the UI's snapshot.
    assert(!request_privileged(nullptr,request).ok);
    request.revision=after.revision; request.entries={{L"0.0.0.0",L"other.example",false}};
    for(DWORD failure:{DWORD(ERROR_CANCELLED),DWORD(ERROR_ACCESS_DENIED),DWORD(ERROR_ACCESS_DISABLED_BY_POLICY)}) {
        simulated_error=failure;
        result=request_privileged(nullptr,request); assert(!result.ok&&result.code==failure);
        assert(manager.snapshot(before,message)&&before.revision==after.revision);
    }
    simulated_error=0;
    crash_before_connect=true;
    assert(!request_privileged(nullptr,request).ok);
    assert(manager.snapshot(before,message)&&before.revision==after.revision);
    crash_before_connect=false;
    assert(elevation_error(ERROR_CANCELLED,true).find(L"directiva")!=std::wstring::npos);
    assert(elevation_error(ERROR_CANCELLED,false).find(L"cancelada")!=std::wstring::npos);
    assert(elevation_error(ERROR_ACCESS_DENIED,false).find(L"denegó")!=std::wstring::npos);
    request.operation=PrivilegedOperation::Remove; request.entries.clear();
    assert(request_privileged(nullptr,request).ok);
    assert(read_bytes(path,bytes,error)&&bytes==original);
    // A fresh instance with different portable metadata sees the very same global file.
    HostsManager other_user(path,join(root,L"unreadable-portable-backup"));
    assert(other_user.snapshot(before,message)&&manager.snapshot(after,message)&&before.revision==after.revision);
    assert(write_bytes(path,"# changed outside Josts\r\n",error));
    assert(manager.snapshot(before,message)); request.revision=before.revision;
    request.operation=PrivilegedOperation::Restore;
    assert(request_privileged(nullptr,request).ok);
    assert(read_bytes(path,bytes,error)&&bytes==original);
    const std::wstring missing=join(root,L"missing"); HostsManager missing_manager(missing);
    assert(!missing_manager.snapshot(before,message)&&!exists(missing));
    std::cout<<"IPC, protocol validation, cancellation, denial, stale state, restore and shared-state tests passed\n";
    return 0;
}
