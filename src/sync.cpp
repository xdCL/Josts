#include "sync.h"
#include "common.h"
#include "hosts_manager.h"
#include "parser.h"
#include "privileged_ops.h"
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <shlobj.h>
#include <sddl.h>
#include <algorithm>
#include <sstream>
#include <vector>

namespace josts {
namespace {
constexpr wchar_t SyncManifestHost[]=L"raw.githubusercontent.com";
constexpr wchar_t SyncManifestPath[]=L"/xdCL/Josts/sync-data/sync/manifest.json";
constexpr wchar_t SyncPackPath[]=L"/xdCL/Josts/sync-data/sync/hosts-pack.txt";
constexpr size_t MaxManifest=64*1024;
constexpr size_t MaxPack=4*1024*1024;

struct SyncManifest {
    int schema=0;
    std::string version;
    std::string sha;
};

std::wstring module_path() {
    std::vector<wchar_t> path(MAX_PATH);
    for(;;) {
        DWORD n=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
        if(!n) return L"";
        if(n<path.size()-1) return std::wstring(path.data(),n);
        path.resize(path.size()*2);
    }
}

std::wstring sync_root() {
    wchar_t path[MAX_PATH]={};
    if(SHGetFolderPathW(nullptr,CSIDL_COMMON_APPDATA,nullptr,SHGFP_TYPE_CURRENT,path)!=S_OK) return L"";
    return join(path,L"Josts");
}

bool set_protected_acl(const std::wstring& path,std::wstring& error) {
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    const wchar_t* sddl=L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;GRGX;;;BU)";
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl,SDDL_REVISION_1,&descriptor,nullptr)) {
        error=L"No se pudo preparar la ACL de Josts Sync: "+error_message(GetLastError()); return false;
    }
    const BOOL ok=SetFileSecurityW(path.c_str(),DACL_SECURITY_INFORMATION,descriptor);
    DWORD code=ok?ERROR_SUCCESS:GetLastError();
    LocalFree(descriptor);
    if(!ok) { error=L"No se pudo proteger la carpeta de Josts Sync: "+error_message(code); return false; }
    return true;
}

bool run_process(const std::wstring& exe,std::wstring command,DWORD& exit_code) {
    STARTUPINFOW si{}; si.cb=sizeof(si); PROCESS_INFORMATION pi{};
    if(command.empty()||command.back()!=L'\0') command.push_back(L'\0');
    if(!CreateProcessW(exe.c_str(),&command[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)) return false;
    CloseHandle(pi.hThread);
    const DWORD waited=WaitForSingleObject(pi.hProcess,60000);
    if(waited!=WAIT_OBJECT_0) { TerminateProcess(pi.hProcess,ERROR_TIMEOUT); CloseHandle(pi.hProcess); SetLastError(ERROR_TIMEOUT); return false; }
    BOOL ok=GetExitCodeProcess(pi.hProcess,&exit_code);
    DWORD code=ok?ERROR_SUCCESS:GetLastError();
    CloseHandle(pi.hProcess);
    if(!ok) { SetLastError(code); return false; }
    return true;
}

bool schedule_sync(const std::wstring& installed,std::wstring& error) {
    wchar_t system[MAX_PATH]={}; UINT n=GetSystemDirectoryW(system,MAX_PATH);
    if(!n||n>=MAX_PATH) { error=L"No se pudo localizar schtasks.exe."; return false; }
    const std::wstring schtasks=join(system,L"schtasks.exe");
    std::wstring command=L"\""+schtasks+L"\" /Create /F /TN \"Josts Sync\" /SC MINUTE /MO 60 /RU SYSTEM /RL HIGHEST /TR \"\\\""+
                         installed+L"\\\" --sync --silent\"";
    DWORD code=0;
    if(!run_process(schtasks,command,code)) { error=L"No se pudo crear la tarea Josts Sync: "+error_message(GetLastError()); return false; }
    if(code!=0) { error=L"schtasks.exe no pudo crear Josts Sync. Código "+std::to_wstring(code)+L"."; return false; }
    return true;
}

bool install_sync(std::wstring& error) {
    if(!token_elevated()) { error=L"Josts Sync necesita permisos administrativos para instalarse."; return false; }
    const std::wstring root=sync_root();
    if(root.empty()) { error=L"No se pudo localizar ProgramData."; return false; }
    if(!ensure_dir(root)) { error=L"No se pudo crear "+root+L": "+error_message(GetLastError()); return false; }
    if(!set_protected_acl(root,error)) return false;

    const std::wstring source=module_path(),target=join(root,L"Josts.exe");
    if(source.empty()) { error=L"No se pudo resolver la ruta de Josts.exe."; return false; }
    if(lower(source)!=lower(target)) {
        if(!CopyFileW(source.c_str(),target.c_str(),FALSE)) {
            error=L"No se pudo instalar Josts Sync en ProgramData: "+error_message(GetLastError()); return false;
        }
    }
    if(!schedule_sync(target,error)) return false;
    log(L"[SYNC] Josts Sync instalado. Tarea programada: cada 60 minutos.");
    return true;
}

bool elevate_install(bool remove,DWORD& exit_code) {
    const std::wstring exe=module_path();
    if(exe.empty()) return false;
    const std::wstring args=remove?L"--remove-sync-admin":L"--install-sync-admin";
    SHELLEXECUTEINFOW launch{}; launch.cbSize=sizeof(launch);
    launch.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC|SEE_MASK_FLAG_NO_UI;
    launch.lpVerb=L"runas"; launch.lpFile=exe.c_str(); launch.lpParameters=args.c_str(); launch.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&launch)) return false;
    if(!launch.hProcess) { SetLastError(ERROR_INVALID_HANDLE); return false; }
    const DWORD waited=WaitForSingleObject(launch.hProcess,60000);
    if(waited!=WAIT_OBJECT_0) { CloseHandle(launch.hProcess); SetLastError(ERROR_TIMEOUT); return false; }
    BOOL ok=GetExitCodeProcess(launch.hProcess,&exit_code);
    DWORD code=ok?ERROR_SUCCESS:GetLastError();
    CloseHandle(launch.hProcess);
    if(!ok) SetLastError(code);
    return ok!=FALSE;
}

bool remove_sync(std::wstring& error) {
    if(!token_elevated()) { error=L"Josts Sync necesita permisos administrativos para desinstalar la tarea."; return false; }
    wchar_t system[MAX_PATH]={}; UINT n=GetSystemDirectoryW(system,MAX_PATH);
    if(!n||n>=MAX_PATH) { error=L"No se pudo localizar schtasks.exe."; return false; }
    const std::wstring schtasks=join(system,L"schtasks.exe");
    std::wstring command=L"\""+schtasks+L"\" /Delete /F /TN \"Josts Sync\"";
    DWORD code=0;
    if(!run_process(schtasks,command,code)) { error=L"No se pudo eliminar la tarea Josts Sync: "+error_message(GetLastError()); return false; }
    if(code!=0) { error=L"schtasks.exe no pudo eliminar Josts Sync. Código "+std::to_wstring(code)+L"."; return false; }
    log(L"[SYNC] Tarea programada Josts Sync eliminada.");
    return true;
}

bool https_get(const wchar_t* path,std::string& out,size_t maximum,std::wstring& error) {
    out.clear();
    HINTERNET session=WinHttpOpen(L"Josts/1.3 Sync",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!session) { error=L"No se pudo iniciar WinHTTP: "+error_message(GetLastError()); return false; }
    WinHttpSetTimeouts(session,5000,5000,10000,15000);
    HINTERNET connect=WinHttpConnect(session,SyncManifestHost,INTERNET_DEFAULT_HTTPS_PORT,0);
    if(!connect) { DWORD code=GetLastError(); WinHttpCloseHandle(session); error=L"No se pudo conectar al origen de Josts Sync: "+error_message(code); return false; }
    HINTERNET request=WinHttpOpenRequest(connect,L"GET",path,nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if(!request) {
        DWORD code=GetLastError(); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        error=L"No se pudo preparar la descarga de Josts Sync: "+error_message(code); return false;
    }
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
    BOOL ok=WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)&&WinHttpReceiveResponse(request,nullptr);
    if(!ok) {
        DWORD code=GetLastError(); WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        error=L"No se pudo descargar Josts Sync: "+error_message(code); return false;
    }
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200) {
        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        error=L"El origen de Josts Sync respondió HTTP "+std::to_wstring(status)+L"."; return false;
    }
    for(;;) {
        DWORD available=0;
        if(!WinHttpQueryDataAvailable(request,&available)) { error=L"Error leyendo Josts Sync: "+error_message(GetLastError()); ok=FALSE; break; }
        if(!available) break;
        if(out.size()+available>maximum) { error=L"La respuesta de Josts Sync excede el tamaño permitido."; ok=FALSE; break; }
        const size_t base=out.size(); out.resize(base+available);
        DWORD read=0;
        if(!WinHttpReadData(request,&out[base],available,&read)) { error=L"Error leyendo Josts Sync: "+error_message(GetLastError()); ok=FALSE; break; }
        out.resize(base+read);
    }
    WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
    return ok!=FALSE;
}

bool json_string(const std::string& json,const char* key,std::string& value) {
    const std::string needle="\""+std::string(key)+"\"";
    size_t p=json.find(needle); if(p==std::string::npos) return false;
    p=json.find(':',p+needle.size()); if(p==std::string::npos) return false;
    p=json.find('"',p+1); if(p==std::string::npos) return false;
    const size_t end=json.find('"',p+1); if(end==std::string::npos) return false;
    value=json.substr(p+1,end-p-1); return true;
}

bool json_int(const std::string& json,const char* key,int& value) {
    const std::string needle="\""+std::string(key)+"\"";
    size_t p=json.find(needle); if(p==std::string::npos) return false;
    p=json.find(':',p+needle.size()); if(p==std::string::npos) return false;
    ++p; while(p<json.size()&&(json[p]==' '||json[p]=='\t'||json[p]=='\r'||json[p]=='\n')) ++p;
    size_t end=p; while(end<json.size()&&json[end]>='0'&&json[end]<='9') ++end;
    if(end==p) return false;
    try { value=std::stoi(json.substr(p,end-p)); } catch(...) { return false; }
    return true;
}

bool parse_manifest(const std::string& bytes,SyncManifest& manifest) {
    if(!json_int(bytes,"schema",manifest.schema)||manifest.schema!=1) return false;
    if(!json_string(bytes,"version",manifest.version)||manifest.version.empty()||manifest.version.size()>128) return false;
    if(manifest.version.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-")!=std::string::npos) return false;
    if(!json_string(bytes,"sha256",manifest.sha)||manifest.sha.size()!=64||
       manifest.sha.find_first_not_of("0123456789abcdef")!=std::string::npos) return false;
    return true;
}

bool parse_pack(const std::string& bytes,std::vector<Entry>& entries,std::wstring& error) {
    TextFile file;
    if(!decode(bytes,file)) { error=L"La batería remota no tiene una codificación válida."; return false; }
    ParseResult parsed=parse_list(file.text);
    if(parsed.errors) { error=L"La batería remota contiene "+std::to_wstring(parsed.errors)+L" entradas inválidas."; return false; }
    if(parsed.entries.empty()) { error=L"La batería remota está vacía."; return false; }
    if(parsed.entries.size()>100000) { error=L"La batería remota supera el límite de entradas."; return false; }
    entries=std::move(parsed.entries); return true;
}

bool apply_entries(const std::vector<Entry>& entries,std::wstring& warning,std::wstring& error) {
    HostsManager manager;
    Snapshot snapshot;
    if(!manager.snapshot(snapshot,error)) return false;
    std::wstring apply_message;
    if(!manager.apply(entries,apply_message,snapshot.revision)) { error=apply_message; return false; }
    warning=apply_message;
    return true;
}

bool write_state(const std::wstring& root,const SyncManifest& manifest,const std::string& pack,std::wstring& error) {
    DWORD code=0;
    if(!write_bytes(join(root,L"hosts-pack.cache"),pack,code)) { error=L"No se pudo guardar la caché de Josts Sync: "+error_message(code); return false; }
    const std::string state=manifest.version+"\n"+manifest.sha+"\n";
    if(!write_bytes(join(root,L"sync-state.txt"),state,code)) { error=L"No se pudo guardar el estado de Josts Sync: "+error_message(code); return false; }
    return true;
}

bool cached_version(const std::wstring& root,std::string& version) {
    std::string bytes; DWORD code=0;
    if(!read_bytes(join(root,L"sync-state.txt"),bytes,code)) return false;
    const size_t end=bytes.find('\n');
    if(end==std::string::npos||!end) return false;
    version=bytes.substr(0,end); return true;
}

bool apply_cache(const std::wstring& root,std::wstring& error) {
    std::string pack; DWORD code=0;
    if(!read_bytes(join(root,L"hosts-pack.cache"),pack,code)) return false;
    std::vector<Entry> entries;
    if(!parse_pack(pack,entries,error)) return false;
    std::wstring warning;
    if(!apply_entries(entries,warning,error)) return false;
    log(L"[SYNC] Sin conexión al origen; se reaplicó la última batería válida en caché ("+std::to_wstring(entries.size())+L" dominios).");
    if(!warning.empty()) log(L"[SYNC] "+warning);
    return true;
}

int sync_now() {
    if(!token_elevated()) {
        log(L"[SYNC] Se rechazó --sync sin token elevado.");
        return ERROR_ELEVATION_REQUIRED;
    }
    const std::wstring root=sync_root();
    if(root.empty()||(!exists(root)&&!ensure_dir(root))) {
        log(L"[SYNC] No se pudo preparar ProgramData\\Josts.");
        return ERROR_PATH_NOT_FOUND;
    }

    std::string manifest_bytes; std::wstring error;
    if(!https_get(SyncManifestPath,manifest_bytes,MaxManifest,error)) {
        log(L"[SYNC] "+error);
        std::wstring cache_error;
        if(apply_cache(root,cache_error)) return 0;
        if(!cache_error.empty()) log(L"[SYNC] Caché no utilizable: "+cache_error);
        return ERROR_WINHTTP_CANNOT_CONNECT;
    }

    SyncManifest manifest;
    if(!parse_manifest(manifest_bytes,manifest)) {
        log(L"[SYNC] Manifest remoto inválido; no se modificó hosts.");
        return ERROR_INVALID_DATA;
    }

    std::string current;
    if(cached_version(root,current)&&current==manifest.version) {
        std::wstring cache_error;
        if(apply_cache(root,cache_error)) {
            log(L"[SYNC] Batería al día ("+std::wstring(manifest.version.begin(),manifest.version.end())+L"); estado local verificado.");
            return 0;
        }
        if(!cache_error.empty()) log(L"[SYNC] No se pudo verificar la caché actual: "+cache_error);
    }

    std::string pack;
    if(!https_get(SyncPackPath,pack,MaxPack,error)) {
        log(L"[SYNC] "+error);
        return ERROR_WINHTTP_CANNOT_CONNECT;
    }
    std::string digest;
    try { digest=sha256(pack); }
    catch(...) { log(L"[SYNC] No se pudo calcular SHA-256 de la batería."); return ERROR_INVALID_DATA; }
    if(digest!=manifest.sha) {
        log(L"[SYNC] SHA-256 remoto no coincide; batería rechazada.");
        return ERROR_CRC;
    }

    std::vector<Entry> entries;
    if(!parse_pack(pack,entries,error)) { log(L"[SYNC] "+error); return ERROR_INVALID_DATA; }

    std::wstring warning;
    if(!apply_entries(entries,warning,error)) {
        log(L"[SYNC] No se pudo aplicar la batería: "+error);
        return ERROR_WRITE_FAULT;
    }
    if(!write_state(root,manifest,pack,error)) {
        log(L"[SYNC] Hosts fue actualizado, pero no se pudo guardar estado: "+error);
        return ERROR_WRITE_FAULT;
    }
    log(L"[SYNC] Batería "+std::wstring(manifest.version.begin(),manifest.version.end())+
        L" aplicada y verificada: "+std::to_wstring(entries.size())+L" dominios.");
    if(!warning.empty()) log(L"[SYNC] "+warning);
    return 0;
}

bool command_line(std::vector<std::wstring>& args) {
    int argc=0; LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(!argv) return false;
    for(int i=0;i<argc;++i) args.push_back(argv[i]);
    LocalFree(argv); return true;
}
}

bool sync_entry(int& exit_code) {
    std::vector<std::wstring> args;
    if(!command_line(args)||args.size()<2) return false;
    const std::wstring command=args[1];
    if(command==L"--sync") {
        exit_code=sync_now(); return true;
    }
    if(command==L"--install-sync") {
        if(token_elevated()) {
            std::wstring error; const bool ok=install_sync(error);
            if(!ok) log(L"[SYNC] Instalación fallida: "+error);
            exit_code=ok?0:ERROR_INSTALL_FAILURE;
            return true;
        }
        DWORD child=ERROR_CANCELLED;
        if(!elevate_install(false,child)) {
            log(L"[SYNC] No se pudo solicitar elevación para instalar Josts Sync: "+error_message(GetLastError()));
            exit_code=GetLastError(); return true;
        }
        exit_code=static_cast<int>(child); return true;
    }
    if(command==L"--install-sync-admin") {
        std::wstring error; const bool ok=install_sync(error);
        if(!ok) log(L"[SYNC] Instalación fallida: "+error);
        exit_code=ok?0:ERROR_INSTALL_FAILURE;
        return true;
    }
    if(command==L"--remove-sync") {
        if(token_elevated()) {
            std::wstring error; const bool ok=remove_sync(error);
            if(!ok) log(L"[SYNC] Desinstalación fallida: "+error);
            exit_code=ok?0:ERROR_INSTALL_FAILURE; return true;
        }
        DWORD child=ERROR_CANCELLED;
        if(!elevate_install(true,child)) { exit_code=GetLastError(); return true; }
        exit_code=static_cast<int>(child); return true;
    }
    if(command==L"--remove-sync-admin") {
        std::wstring error; const bool ok=remove_sync(error);
        if(!ok) log(L"[SYNC] Desinstalación fallida: "+error);
        exit_code=ok?0:ERROR_INSTALL_FAILURE; return true;
    }
    return false;
}
}
