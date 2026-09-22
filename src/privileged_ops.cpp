#include "privileged_ops.h"
#include "parser.h"
#include "edge_settings.h"
#include <shellapi.h>
#include <objbase.h>
#include <sddl.h>
#include <algorithm>
#include <sstream>
#include <cstring>

namespace josts {
#ifdef JOSTS_BROKER_TESTS
bool broker_test_launch(SHELLEXECUTEINFOW* launch);
HostsManager broker_test_manager();
#endif
namespace {
constexpr DWORD MaxWire=8*1024*1024,IoTimeout=30000;
struct Handle {
    HANDLE h;
    explicit Handle(HANDLE value=nullptr):h(value) {}
    ~Handle() { if(h&&h!=INVALID_HANDLE_VALUE) CloseHandle(h); }
    Handle(const Handle&)=delete; Handle& operator=(const Handle&)=delete;
};
struct Com {
    HRESULT result=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
    ~Com() { if(SUCCEEDED(result)) CoUninitialize(); }
};
bool hex(const std::string& text,size_t size) {
    return text.size()==size&&text.find_first_not_of("0123456789abcdef")==std::string::npos;
}
bool transfer(HANDLE pipe,void* data,DWORD size,bool write) {
    auto* bytes=static_cast<char*>(data);
    const ULONGLONG deadline=GetTickCount64()+IoTimeout;
    while(size) {
        Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));
        if(!event.h) return false;
        OVERLAPPED ov{}; ov.hEvent=event.h; DWORD done=0;
        BOOL ok=write?WriteFile(pipe,bytes,size,&done,&ov):ReadFile(pipe,bytes,size,&done,&ov);
        if(!ok) {
            DWORD code=GetLastError();
            if(code!=ERROR_IO_PENDING) { SetLastError(code); return false; }
            const auto now=GetTickCount64();
            if(now>=deadline) {
                CancelIoEx(pipe,&ov); GetOverlappedResult(pipe,&ov,&done,TRUE); SetLastError(ERROR_TIMEOUT); return false;
            }
            const DWORD waited=WaitForSingleObject(event.h,static_cast<DWORD>(deadline-now));
            if(waited!=WAIT_OBJECT_0) {
                const DWORD wait_error=waited==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();
                CancelIoEx(pipe,&ov); GetOverlappedResult(pipe,&ov,&done,TRUE); SetLastError(wait_error); return false;
            }
            if(!GetOverlappedResult(pipe,&ov,&done,FALSE)) return false;
        }
        if(!done) { SetLastError(ERROR_BROKEN_PIPE); return false; }
        size-=done; bytes+=done;
    }
    SetLastError(ERROR_SUCCESS);
    return true;
}
bool send(HANDLE pipe,const std::string& message) {
    if(message.size()>MaxWire) return false;
    DWORD size=static_cast<DWORD>(message.size());
    return transfer(pipe,&size,sizeof(size),true)&&transfer(pipe,(void*)message.data(),size,true);
}
bool receive(HANDLE pipe,std::string& message) {
    DWORD size=0;
    if(!transfer(pipe,&size,sizeof(size),false)||size>MaxWire) return false;
    message.resize(size); return !size||transfer(pipe,&message[0],size,false);
}
std::wstring image_path(HANDLE process) {
    std::wstring path(32768,L'\0'); DWORD size=static_cast<DWORD>(path.size());
    if(!QueryFullProcessImageNameW(process,0,&path[0],&size)) return L"";
    path.resize(size); return path;
}
bool standard_user() {
    Handle token; if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.h)) return false;
    TOKEN_ELEVATION_TYPE type{}; DWORD size=0;
    if(!GetTokenInformation(token.h,TokenElevationType,&type,sizeof(type),&size)) return false;
    // A split-token administrator is not a standard user.
    return type==TokenElevationTypeDefault&&!token_elevated();
}
bool policy_denies() {
    if(!standard_user()) return false;
    DWORD value=1,size=sizeof(value),type=0; HKEY key=nullptr;
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",0,
                    KEY_QUERY_VALUE|KEY_WOW64_64KEY,&key)!=ERROR_SUCCESS) return false;
    LSTATUS status=RegQueryValueExW(key,L"ConsentPromptBehaviorUser",nullptr,&type,(BYTE*)&value,&size);
    RegCloseKey(key); return status==ERROR_SUCCESS&&type==REG_DWORD&&size==sizeof(value)&&value==0;
}
std::wstring pipe_security() {
    Handle token; if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.h)) return L"";
    DWORD size=0; GetTokenInformation(token.h,TokenUser,nullptr,0,&size);
    std::vector<BYTE> info(size);
    if(!size||!GetTokenInformation(token.h,TokenUser,info.data(),size,&size)) return L"";
    LPWSTR sid=nullptr;
    if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(info.data())->User.Sid,&sid)) return L"";
    std::wstring acl=L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;"+std::wstring(sid)+L")";
    LocalFree(sid); return acl;
}
std::wstring flush_dns() {
    wchar_t system[MAX_PATH]={}; UINT n=GetSystemDirectoryW(system,MAX_PATH);
    if(!n||n>=MAX_PATH) return tr(L"Cambios guardados, pero no se pudo vaciar la caché DNS.");
    const std::wstring exe=join(system,L"ipconfig.exe");
    std::wstring command=L"\""+exe+L"\" /flushdns";
    STARTUPINFOW si{}; si.cb=sizeof(si); PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),&command[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,system,&si,&pi))
        return tr(L"Cambios guardados, pero no se pudo vaciar la caché DNS.");
    Handle process(pi.hProcess),thread(pi.hThread);
    DWORD code=1;
    if(WaitForSingleObject(process.h,10000)==WAIT_OBJECT_0&&GetExitCodeProcess(process.h,&code)&&code==0) return L"";
    return tr(L"Cambios guardados, pero no se confirmó el vaciado de la caché DNS.");
}
PrivilegedResult ipc_failure(const std::wstring& stage,DWORD code=ERROR_INVALID_DATA) {
    if(code==ERROR_SUCCESS) code=ERROR_INVALID_DATA;
    const std::wstring diagnostic=L"[IPC] "+stage+L" — código "+std::to_wstring(code)+L": "+error_message(code);
    log(diagnostic);
    return {false,code,
        std::wstring(tr(L"No se pudo confirmar la operación administrativa. Se volverá a leer hosts; no se supone que el cambio haya funcionado."))+
        L"\r\n"+tr(L"Diagnóstico: ")+stage+L" ["+std::to_wstring(code)+L"] "+error_message(code),L""};
}
PrivilegedResult preparation_failure(const std::wstring& stage,DWORD code=ERROR_SUCCESS) {
    if(code==ERROR_SUCCESS) code=GetLastError();
    if(code==ERROR_SUCCESS) code=ERROR_INVALID_DATA;
    log(L"[IPC] "+stage+L" — código "+std::to_wstring(code)+L": "+error_message(code));
    return {false,code,std::wstring(tr(L"No se pudo preparar el canal administrativo; todavía no se solicitó elevación. No se modificó el sistema."))+
        L"\r\n"+tr(L"Diagnóstico: ")+stage+L" ["+std::to_wstring(code)+L"] "+error_message(code),L""};
}
void helper_failure(const std::wstring& stage,DWORD code,int& exit_code) {
    if(code==ERROR_SUCCESS) code=ERROR_INVALID_DATA;
    log(L"[HELPER] ERROR "+stage+L" — código "+std::to_wstring(code)+L": "+error_message(code));
    exit_code=static_cast<int>(code);
}
}
bool token_elevated() {
    Handle token; if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.h)) return false;
    TOKEN_ELEVATION elevation{}; DWORD size=0;
    return GetTokenInformation(token.h,TokenElevation,&elevation,sizeof(elevation),&size)&&elevation.TokenIsElevated;
}
std::wstring elevation_error(DWORD code,bool denied) {
    if(denied) return tr(L"Windows no permite solicitar credenciales administrativas desde esta cuenta (directiva del equipo). No se cambiaron las políticas de seguridad.");
    if(code==ERROR_CANCELLED) return tr(L"Solicitud de permisos cancelada. Windows devolvió ERROR_CANCELLED; no se ejecutó la operación.");
    if(code==ERROR_ACCESS_DENIED||code==ERROR_ACCESS_DISABLED_BY_POLICY||code==ERROR_PRIVILEGE_NOT_HELD)
        return tr(L"Windows denegó la elevación. Puede deberse a una directiva o a permisos del equipo: ")+error_message(code);
    return tr(L"Se solicitó elevación, pero Windows no pudo iniciar la operación administrativa: ")+error_message(code);
}
bool encode_request(const PrivilegedRequest& request,std::string& wire) {
    const auto op=static_cast<DWORD>(request.operation);
    if(op<1||op>4||(op!=4&&!hex(request.revision,64))||request.entries.size()>100000) return false;
    if(op!=1&&!request.entries.empty()) return false;
    wire="JOSTS-1.1\n"+std::to_string(op)+"\n"+(language()==Language::English?"1":"0")+"\n"+request.revision+"\n";
    for(const auto& e:request.entries) {
        if(!valid_ip(e.ip)||!valid_domain(e.domain)) return false;
        wire+=utf8(e.ip+L" "+lower(e.domain))+"\n";
        if(wire.size()>MaxWire) return false;
    }
    return true;
}
bool decode_request(const std::string& wire,PrivilegedRequest& request) {
    if(wire.size()>MaxWire||wire.find('\0')!=std::string::npos) return false;
    std::istringstream in(wire); std::string magic,op,lang,revision,line;
    if(!std::getline(in,magic)||magic!="JOSTS-1.1"||!std::getline(in,op)||op.size()!=1||op[0]<'1'||op[0]>'4'||
       !std::getline(in,lang)||(lang!="0"&&lang!="1")||!std::getline(in,revision)) return false;
    request={}; request.operation=static_cast<PrivilegedOperation>(op[0]-'0'); request.revision=revision;
    if(request.operation!=PrivilegedOperation::Edge&&!hex(revision,64)) return false;
    if(request.operation==PrivilegedOperation::Edge&&!revision.empty()) return false;
    while(std::getline(in,line)) {
        if(request.operation!=PrivilegedOperation::Apply||line.empty()||line.size()>300) return false;
        // The broker accepts only canonical ASCII IP/domain pairs, not import syntax or file paths.
        if(line.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .:_-")!=std::string::npos) return false;
        std::istringstream words(line); std::string ip,domain,extra; words>>ip>>domain;
        if(words>>extra) return false;
        Entry e{std::wstring(ip.begin(),ip.end()),std::wstring(domain.begin(),domain.end()),false};
        if(!valid_ip(e.ip)||!valid_domain(e.domain)||request.entries.size()>=100000) return false;
        request.entries.push_back(e);
    }
    set_language(lang=="1"?Language::English:Language::SpanishChile); return true;
}
PrivilegedResult execute_privileged(HostsManager& manager,const PrivilegedRequest& request) {
    PrivilegedResult result; std::wstring error;
    switch(request.operation) {
    case PrivilegedOperation::Apply: result.ok=manager.apply(request.entries,error,request.revision); break;
    case PrivilegedOperation::Remove: result.ok=manager.remove_own(error,request.revision); break;
    case PrivilegedOperation::Restore: result.ok=manager.restore(error,request.revision); break;
    case PrivilegedOperation::Edge:
        result.ok=BloquearContenidoNoticiasEdge(); result.code=GetLastError();
        result.message=result.ok?tr(L"Configuración de Edge guardada y verificada. Abre una nueva pestaña; si aún ves noticias, reinicia Edge."):
            std::wstring(tr(L"No se pudo guardar o verificar la política de Edge: "))+error_message(result.code); return result;
    default: error=tr(L"Operación administrativa no válida."); break;
    }
    result.message=result.ok?tr(L"Cambios aplicados y verificados correctamente."):error;
    if(result.ok&&request.operation==PrivilegedOperation::Restore) result.message=tr(L"Se restauró el archivo desde el respaldo original del equipo y se verificó su contenido.");
    if(!result.ok) result.code=ERROR_WRITE_FAULT;
    return result;
}
PrivilegedResult request_privileged(HWND owner,const PrivilegedRequest& request) {
    std::string payload;
    if(!encode_request(request,payload)) return {false,ERROR_INVALID_DATA,tr(L"La solicitud contiene datos inválidos; no se solicitó elevación."),L""};
    if(token_elevated()
#ifdef JOSTS_BROKER_TESTS
       &&false
#endif
    ) {
        HostsManager manager; auto result=execute_privileged(manager,request);
        if(result.ok&&request.operation!=PrivilegedOperation::Edge) result.warning=flush_dns();
        return result;
    }

    // Two independent random values are used: one names the pipe, the other authenticates
    // the elevated helper. This avoids relying on cross-account OpenProcess/GetProcessId
    // access, which can fail when a standard user supplies credentials for another admin.
    const std::wstring nonce=random_id(),auth=random_id(),pipe_name=L"\\\\.\\pipe\\Josts-"+nonce;
    const std::wstring acl=pipe_security(); PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(acl.empty()) return preparation_failure(L"generar ACL del canal",GetLastError());
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&descriptor,nullptr))
        return preparation_failure(L"crear descriptor de seguridad",GetLastError());
    SECURITY_ATTRIBUTES sa{sizeof(sa),descriptor,FALSE};
    Handle pipe(CreateNamedPipeW(pipe_name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,65536,0,&sa));
    const DWORD pipe_code=pipe.h==INVALID_HANDLE_VALUE?GetLastError():ERROR_SUCCESS;
    LocalFree(descriptor);
    if(pipe.h==INVALID_HANDLE_VALUE) return preparation_failure(L"crear named pipe",pipe_code);
    log(L"[IPC] Canal administrativo preparado.");

    const std::wstring exe=image_path(GetCurrentProcess());
    if(exe.empty()) return preparation_failure(L"resolver ruta de Josts.exe",GetLastError());
    const std::wstring arguments=L"--josts-elevated "+nonce+L" "+std::to_wstring(GetCurrentProcessId())+L" "+auth;
    Com com;
    SHELLEXECUTEINFOW launch{}; launch.cbSize=sizeof(launch); launch.hwnd=owner;
    launch.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC|SEE_MASK_FLAG_NO_UI;
    launch.lpVerb=L"runas"; launch.lpFile=exe.c_str(); launch.lpParameters=arguments.c_str();
    // Keep the elevated broker visible to Windows instead of deliberately hiding it.
    // It has no interactive UI, so SW_SHOWNORMAL does not create an extra application window.
    launch.nShow=SW_SHOWNORMAL;
    log(tr(L"Se requieren permisos de administrador. Solicitando elevación UAC para una operación."));
    if(!
#ifdef JOSTS_BROKER_TESTS
       broker_test_launch(&launch)
#else
       ShellExecuteExW(&launch)
#endif
    ) {
        DWORD code=GetLastError();
        log(L"[UAC] No se pudo iniciar el auxiliar — código "+std::to_wstring(code)+L": "+error_message(code));
        return {false,code,elevation_error(code,policy_denies()),L""};
    }
    Handle child(launch.hProcess);
    if(!child.h) return ipc_failure(L"ShellExecuteEx no entregó handle de proceso",ERROR_INVALID_HANDLE);
    log(L"[UAC] Auxiliar administrativo iniciado; esperando conexión segura.");

    Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));
    if(!event.h) return ipc_failure(L"crear evento de conexión",GetLastError());
    OVERLAPPED ov{}; ov.hEvent=event.h;
    BOOL connected=ConnectNamedPipe(pipe.h,&ov);
    DWORD code=connected?ERROR_SUCCESS:GetLastError();
    if(!connected&&code==ERROR_IO_PENDING) {
        HANDLE waiting[]={event.h,child.h};
        const DWORD waited=WaitForMultipleObjects(2,waiting,FALSE,IoTimeout);
        if(waited==WAIT_OBJECT_0+1) {
            DWORD helper_code=ERROR_INVALID_DATA;
            GetExitCodeProcess(child.h,&helper_code);
            CancelIoEx(pipe.h,&ov); DWORD ignored=0; GetOverlappedResult(pipe.h,&ov,&ignored,TRUE);
            return ipc_failure(L"el auxiliar terminó antes de conectarse",helper_code);
        }
        if(waited!=WAIT_OBJECT_0) {
            const DWORD wait_error=waited==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();
            CancelIoEx(pipe.h,&ov); DWORD ignored=0; GetOverlappedResult(pipe.h,&ov,&ignored,TRUE);
            return ipc_failure(L"esperar conexión del auxiliar",wait_error);
        }
        DWORD ignored=0;
        if(!GetOverlappedResult(pipe.h,&ov,&ignored,FALSE)) return ipc_failure(L"completar conexión del auxiliar",GetLastError());
    } else if(!connected&&code!=ERROR_PIPE_CONNECTED) return ipc_failure(L"conectar named pipe",code);
    log(L"[IPC] Auxiliar conectado al canal.");

    // Authenticate possession of the unguessable secret passed only to the elevated launch.
    // We intentionally do not compare process handles/PIDs across user accounts.
    std::string hello;
    if(!receive(pipe.h,hello)) return ipc_failure(L"recibir autenticación del auxiliar",GetLastError());
    const std::string expected_hello="JOSTS-AUTH "+utf8(auth);
    if(hello!=expected_hello) return ipc_failure(L"validar autenticación del auxiliar",ERROR_ACCESS_DENIED);
    ULONG client_pid=0;
    if(GetNamedPipeClientProcessId(pipe.h,&client_pid))
        log(L"[IPC] Auxiliar autenticado. PID cliente: "+std::to_wstring(client_pid)+L".");
    else
        log(L"[IPC] Auxiliar autenticado; Windows no informó PID del cliente (no bloqueante).");

    if(!send(pipe.h,payload)) return ipc_failure(L"enviar solicitud administrativa",GetLastError());
    log(L"[IPC] Solicitud administrativa enviada.");
    std::string reply;
    if(!receive(pipe.h,reply)) return ipc_failure(L"recibir respuesta administrativa",GetLastError());
    if(reply.size()<2||(reply[0]!='0'&&reply[0]!='1')||reply[1]!='\n') return ipc_failure(L"validar formato de respuesta",ERROR_INVALID_DATA);
    TextFile decoded; if(!decode(reply.substr(2),decoded)) return ipc_failure(L"decodificar respuesta administrativa",ERROR_NO_UNICODE_TRANSLATION);
    const auto split=decoded.text.find(L'\n');
    PrivilegedResult result{reply[0]=='1',ERROR_SUCCESS,decoded.text.substr(0,split),split==std::wstring::npos?L"":decoded.text.substr(split+1)};
    if(!send(pipe.h,"ACK")) log(L"[IPC] Advertencia: no se pudo confirmar recepción de la respuesta.");
    const DWORD waited=WaitForSingleObject(child.h,IoTimeout);
    if(waited!=WAIT_OBJECT_0) return ipc_failure(L"esperar cierre del auxiliar",waited==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError());
    DWORD exit_code=1;
    if(!GetExitCodeProcess(child.h,&exit_code)) return ipc_failure(L"leer código de salida del auxiliar",GetLastError());
    if(result.ok&&exit_code!=0) return ipc_failure(L"el auxiliar reportó éxito pero terminó con error",exit_code);
    log(L"[IPC] Operación administrativa confirmada; auxiliar finalizado con código "+std::to_wstring(exit_code)+L".");
    return result;
}

bool privileged_entry(int& exit_code) {
    int argc=0; LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(!argv) { exit_code=ERROR_INVALID_PARAMETER; return true; }
    if(argc==1) { LocalFree(argv); return false; }
    std::wstring command=argv[1],nonce=argc>2?argv[2]:L"",parent=argc>3?argv[3]:L"",auth=argc>4?argv[4]:L"";
    LocalFree(argv); exit_code=ERROR_INVALID_PARAMETER;
    if(command!=L"--josts-elevated"||argc!=5||!hex(utf8(nonce),32)||!hex(utf8(auth),32)||parent.empty()||parent.size()>10||
       parent.find_first_not_of(L"0123456789")!=std::wstring::npos) return true;
#ifndef JOSTS_BROKER_TESTS
    if(!token_elevated()) { helper_failure(L"token no elevado",ERROR_ELEVATION_REQUIRED,exit_code); return true; }
#endif
    const unsigned long long parent_number=std::stoull(parent);
    if(!parent_number||parent_number>MAXDWORD) { helper_failure(L"PID padre inválido",ERROR_INVALID_PARAMETER,exit_code); return true; }
    const DWORD parent_pid=static_cast<DWORD>(parent_number);
    log(L"[HELPER] Auxiliar elevado iniciado. Validando canal del proceso solicitante.");

    const std::wstring name=L"\\\\.\\pipe\\Josts-"+nonce;
    if(!WaitNamedPipeW(name.c_str(),IoTimeout)) { helper_failure(L"esperar named pipe",GetLastError(),exit_code); return true; }
    Handle pipe(CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,nullptr));
    if(pipe.h==INVALID_HANDLE_VALUE) { helper_failure(L"abrir named pipe",GetLastError(),exit_code); return true; }
    ULONG server=0;
    if(!GetNamedPipeServerProcessId(pipe.h,&server)) { helper_failure(L"consultar PID del servidor",GetLastError(),exit_code); return true; }
    if(server!=parent_pid) { helper_failure(L"PID del servidor no coincide",ERROR_ACCESS_DENIED,exit_code); return true; }

    const std::string hello="JOSTS-AUTH "+utf8(auth);
    if(!send(pipe.h,hello)) { helper_failure(L"enviar autenticación",GetLastError(),exit_code); return true; }
    log(L"[HELPER] Canal validado; esperando solicitud limitada de Josts.");
    std::string payload; PrivilegedRequest request;
    if(!receive(pipe.h,payload)) { helper_failure(L"recibir solicitud",GetLastError(),exit_code); return true; }
    if(!decode_request(payload,request)) { helper_failure(L"validar solicitud",ERROR_INVALID_DATA,exit_code); return true; }
    log(L"[HELPER] Solicitud recibida y validada.");

    HostsManager manager
#ifdef JOSTS_BROKER_TESTS
        =broker_test_manager()
#endif
        ;
    auto result=execute_privileged(manager,request);
#ifndef JOSTS_BROKER_TESTS
    if(result.ok&&request.operation!=PrivilegedOperation::Edge) result.warning=flush_dns();
#endif
    const std::string reply=(result.ok?"1\n":"0\n")+utf8(result.message+L"\n"+result.warning);
    if(!send(pipe.h,reply)) { helper_failure(L"enviar resultado",GetLastError(),exit_code); return true; }
    std::string ack;
    if(!receive(pipe.h,ack)) log(L"[HELPER] Advertencia: no se recibió ACK del proceso solicitante.");
    exit_code=result.ok?0:ERROR_WRITE_FAULT;
    log(L"[HELPER] Operación finalizada. Código de salida: "+std::to_wstring(exit_code)+L".");
    return true;
}

}
