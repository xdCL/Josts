#include "resident.h"
#include "common.h"
#include "privileged_ops.h"
#include "sync.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <sddl.h>
#include <vector>

namespace josts {
namespace {
constexpr wchar_t ServiceName[]=L"JostsResident";
constexpr wchar_t ServiceDisplayName[]=L"Josts Resident";
constexpr DWORD ResidentIntervalMs=5*60*1000;

SERVICE_STATUS_HANDLE service_handle=nullptr;
SERVICE_STATUS service_status{};
HANDLE stop_event=nullptr;

std::wstring module_path() {
    std::vector<wchar_t> path(MAX_PATH);
    for(;;) {
        DWORD n=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
        if(!n) return L"";
        if(n<path.size()-1) return std::wstring(path.data(),n);
        path.resize(path.size()*2);
    }
}

std::wstring install_root() {
    wchar_t path[MAX_PATH]={};
    if(SHGetFolderPathW(nullptr,CSIDL_COMMON_APPDATA,nullptr,SHGFP_TYPE_CURRENT,path)!=S_OK) return L"";
    return join(path,L"Josts");
}

bool protect_path(const std::wstring& path,bool directory,std::wstring& error) {
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    const wchar_t* sddl=directory?
        L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;GRGX;;;BU)":
        L"D:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;GRGX;;;BU)";
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl,SDDL_REVISION_1,&descriptor,nullptr)) {
        error=L"No se pudo preparar la seguridad de Josts Resident: "+error_message(GetLastError());
        return false;
    }
    const BOOL ok=SetFileSecurityW(path.c_str(),DACL_SECURITY_INFORMATION,descriptor);
    const DWORD code=ok?ERROR_SUCCESS:GetLastError();
    LocalFree(descriptor);
    if(!ok) {
        error=L"No se pudo proteger Josts Resident: "+error_message(code);
        return false;
    }
    return true;
}

bool run_hidden(const std::wstring& exe,std::wstring command,DWORD& exit_code) {
    STARTUPINFOW si{}; si.cb=sizeof(si); PROCESS_INFORMATION pi{};
    if(command.empty()||command.back()!=L'\0') command.push_back(L'\0');
    if(!CreateProcessW(exe.c_str(),&command[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)) return false;
    CloseHandle(pi.hThread);
    const DWORD waited=WaitForSingleObject(pi.hProcess,30000);
    if(waited!=WAIT_OBJECT_0) {
        TerminateProcess(pi.hProcess,ERROR_TIMEOUT);
        CloseHandle(pi.hProcess);
        SetLastError(ERROR_TIMEOUT);
        return false;
    }
    BOOL ok=GetExitCodeProcess(pi.hProcess,&exit_code);
    const DWORD code=ok?ERROR_SUCCESS:GetLastError();
    CloseHandle(pi.hProcess);
    if(!ok) SetLastError(code);
    return ok!=FALSE;
}

void remove_legacy_task() {
    wchar_t system[MAX_PATH]={};
    UINT n=GetSystemDirectoryW(system,MAX_PATH);
    if(!n||n>=MAX_PATH) return;
    const std::wstring schtasks=join(system,L"schtasks.exe");
    DWORD ignored=0;
    run_hidden(schtasks,L"\""+schtasks+L"\" /Delete /F /TN \"Josts Sync\"",ignored);
}

bool wait_service_state(SC_HANDLE service,DWORD desired,DWORD timeout_ms) {
    const ULONGLONG deadline=GetTickCount64()+timeout_ms;
    SERVICE_STATUS_PROCESS status{};
    DWORD needed=0;
    do {
        if(!QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,reinterpret_cast<LPBYTE>(&status),sizeof(status),&needed))
            return false;
        if(status.dwCurrentState==desired) return true;
        Sleep(100);
    } while(GetTickCount64()<deadline);
    return false;
}

bool stop_service_admin(std::wstring& error) {
    if(!token_elevated()) {
        error=L"Se requieren permisos administrativos para detener Josts Resident.";
        return false;
    }
    SC_HANDLE scm=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);
    if(!scm) { error=L"No se pudo abrir el Administrador de servicios: "+error_message(GetLastError()); return false; }
    SC_HANDLE service=OpenServiceW(scm,ServiceName,SERVICE_STOP|SERVICE_QUERY_STATUS);
    if(!service) {
        const DWORD code=GetLastError();
        CloseServiceHandle(scm);
        if(code==ERROR_SERVICE_DOES_NOT_EXIST) return true;
        error=L"No se pudo abrir Josts Resident: "+error_message(code);
        return false;
    }
    SERVICE_STATUS status{};
    bool ok=true;
    if(ControlService(service,SERVICE_CONTROL_STOP,&status)) {
        ok=wait_service_state(service,SERVICE_STOPPED,15000);
        if(!ok) error=L"Josts Resident no confirmó su detención dentro del plazo esperado.";
    } else {
        const DWORD code=GetLastError();
        if(code!=ERROR_SERVICE_NOT_ACTIVE) {
            ok=false;
            error=L"No se pudo detener Josts Resident: "+error_message(code);
        }
    }
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    if(ok) log(L"[RESIDENT] Servicio detenido por una operación administrativa.");
    return ok;
}

bool apply_service_acl(SC_HANDLE service,std::wstring& error) {
    // SYSTEM and Administrators retain full control. Authenticated users can only
    // inspect the service; they cannot stop, reconfigure or delete it.
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;CCLCLORC;;;AU)",
        SDDL_REVISION_1,&descriptor,nullptr)) {
        error=L"No se pudo preparar la ACL del servicio: "+error_message(GetLastError());
        return false;
    }
    const BOOL ok=SetServiceObjectSecurity(service,DACL_SECURITY_INFORMATION,descriptor);
    const DWORD code=ok?ERROR_SUCCESS:GetLastError();
    LocalFree(descriptor);
    if(!ok) {
        error=L"No se pudo proteger el servicio Josts Resident: "+error_message(code);
        return false;
    }
    return true;
}

bool install_service_admin(std::wstring& error) {
    if(!token_elevated()) {
        error=L"Josts Resident necesita permisos administrativos para instalarse.";
        return false;
    }

    const std::wstring root=install_root();
    if(root.empty()) { error=L"No se pudo localizar ProgramData."; return false; }
    if(!ensure_dir(root)) { error=L"No se pudo crear "+root+L": "+error_message(GetLastError()); return false; }
    if(!protect_path(root,true,error)) return false;

    const std::wstring source=module_path();
    const std::wstring target=join(root,L"Josts.exe");
    if(source.empty()) { error=L"No se pudo resolver la ruta de Josts.exe."; return false; }

    SC_HANDLE scm=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT|SC_MANAGER_CREATE_SERVICE);
    if(!scm) { error=L"No se pudo abrir el Administrador de servicios: "+error_message(GetLastError()); return false; }

    SC_HANDLE existing=OpenServiceW(scm,ServiceName,SERVICE_STOP|SERVICE_QUERY_STATUS|SERVICE_CHANGE_CONFIG|SERVICE_START|WRITE_DAC);
    if(existing) {
        SERVICE_STATUS status{};
        if(ControlService(existing,SERVICE_CONTROL_STOP,&status))
            wait_service_state(existing,SERVICE_STOPPED,15000);
    }

    if(lower(source)!=lower(target)) {
        if(!CopyFileW(source.c_str(),target.c_str(),FALSE)) {
            const DWORD code=GetLastError();
            if(existing) CloseServiceHandle(existing);
            CloseServiceHandle(scm);
            error=L"No se pudo instalar Josts Resident en ProgramData: "+error_message(code);
            return false;
        }
    }
    if(!protect_path(target,false,error)) {
        if(existing) CloseServiceHandle(existing);
        CloseServiceHandle(scm);
        return false;
    }

    const std::wstring binary=L"\""+target+L"\" --josts-resident-service";
    SC_HANDLE service=existing;
    if(service) {
        if(!ChangeServiceConfigW(service,SERVICE_NO_CHANGE,SERVICE_AUTO_START,SERVICE_NO_CHANGE,
            binary.c_str(),nullptr,nullptr,nullptr,nullptr,nullptr,ServiceDisplayName)) {
            const DWORD code=GetLastError();
            CloseServiceHandle(service); CloseServiceHandle(scm);
            error=L"No se pudo actualizar Josts Resident: "+error_message(code);
            return false;
        }
    } else {
        service=CreateServiceW(scm,ServiceName,ServiceDisplayName,SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS,SERVICE_AUTO_START,SERVICE_ERROR_NORMAL,binary.c_str(),
            nullptr,nullptr,nullptr,L"LocalSystem",nullptr);
        if(!service) {
            const DWORD code=GetLastError();
            CloseServiceHandle(scm);
            error=L"No se pudo crear Josts Resident: "+error_message(code);
            return false;
        }
    }

    SERVICE_DESCRIPTIONW description{};
    description.lpDescription=const_cast<LPWSTR>(L"Sincroniza en segundo plano la batería firmada de dominios de Josts.");
    ChangeServiceConfig2W(service,SERVICE_CONFIG_DESCRIPTION,&description);
    SERVICE_DELAYED_AUTO_START_INFO delayed{TRUE};
    ChangeServiceConfig2W(service,SERVICE_CONFIG_DELAYED_AUTO_START_INFO,&delayed);

    if(!apply_service_acl(service,error)) {
        CloseServiceHandle(service); CloseServiceHandle(scm); return false;
    }

    if(!StartServiceW(service,0,nullptr)) {
        const DWORD code=GetLastError();
        if(code!=ERROR_SERVICE_ALREADY_RUNNING) {
            CloseServiceHandle(service); CloseServiceHandle(scm);
            error=L"No se pudo iniciar Josts Resident: "+error_message(code);
            return false;
        }
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    remove_legacy_task();
    log(L"[RESIDENT] Josts Resident instalado e iniciado. Intervalo: 5 minutos.");
    return true;
}

bool remove_service_admin(std::wstring& error) {
    if(!token_elevated()) {
        error=L"Se requieren permisos administrativos para desinstalar Josts Resident.";
        return false;
    }

    SC_HANDLE scm=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);
    if(!scm) { error=L"No se pudo abrir el Administrador de servicios: "+error_message(GetLastError()); return false; }
    SC_HANDLE service=OpenServiceW(scm,ServiceName,SERVICE_STOP|SERVICE_QUERY_STATUS|DELETE);
    if(!service) {
        const DWORD code=GetLastError();
        CloseServiceHandle(scm);
        if(code!=ERROR_SERVICE_DOES_NOT_EXIST) {
            error=L"No se pudo abrir Josts Resident: "+error_message(code);
            return false;
        }
    } else {
        SERVICE_STATUS status{};
        if(ControlService(service,SERVICE_CONTROL_STOP,&status))
            wait_service_state(service,SERVICE_STOPPED,15000);
        if(!DeleteService(service)) {
            const DWORD code=GetLastError();
            if(code!=ERROR_SERVICE_MARKED_FOR_DELETE) {
                CloseServiceHandle(service); CloseServiceHandle(scm);
                error=L"No se pudo eliminar Josts Resident: "+error_message(code);
                return false;
            }
        }
        CloseServiceHandle(service);
        CloseServiceHandle(scm);
    }

    remove_legacy_task();

    const std::wstring target=join(install_root(),L"Josts.exe");
    if(!target.empty()&&lower(module_path())!=lower(target)) {
        if(!DeleteFileW(target.c_str())&&GetLastError()!=ERROR_FILE_NOT_FOUND)
            MoveFileExW(target.c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT);
    } else if(!target.empty()) {
        MoveFileExW(target.c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT);
    }

    log(L"[RESIDENT] Josts Resident desinstalado mediante operación administrativa.");
    return true;
}

bool elevate(HWND owner,const wchar_t* command,DWORD& exit_code,std::wstring& error) {
    const std::wstring exe=module_path();
    if(exe.empty()) { error=L"No se pudo resolver la ruta de Josts.exe."; return false; }
    SHELLEXECUTEINFOW launch{}; launch.cbSize=sizeof(launch); launch.hwnd=owner;
    launch.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC|SEE_MASK_FLAG_NO_UI;
    launch.lpVerb=L"runas"; launch.lpFile=exe.c_str(); launch.lpParameters=command; launch.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&launch)) {
        const DWORD code=GetLastError();
        error=code==ERROR_CANCELLED?L"Autorización UAC cancelada.":L"No se pudo solicitar autorización UAC: "+error_message(code);
        exit_code=code;
        return false;
    }
    if(!launch.hProcess) { error=L"Windows no entregó el proceso administrativo."; exit_code=ERROR_INVALID_HANDLE; return false; }
    const DWORD waited=WaitForSingleObject(launch.hProcess,60000);
    if(waited!=WAIT_OBJECT_0) {
        CloseHandle(launch.hProcess);
        error=L"La operación administrativa no terminó dentro del plazo esperado.";
        exit_code=ERROR_TIMEOUT;
        return false;
    }
    DWORD child=ERROR_GEN_FAILURE;
    GetExitCodeProcess(launch.hProcess,&child);
    CloseHandle(launch.hProcess);
    exit_code=child;
    if(child!=ERROR_SUCCESS) {
        error=L"La operación administrativa terminó con código "+std::to_wstring(child)+L".";
        return false;
    }
    return true;
}

void set_status(DWORD state,DWORD win32=NO_ERROR,DWORD wait_hint=0) {
    service_status.dwServiceType=SERVICE_WIN32_OWN_PROCESS;
    service_status.dwCurrentState=state;
    service_status.dwWin32ExitCode=win32;
    service_status.dwWaitHint=wait_hint;
    service_status.dwControlsAccepted=(state==SERVICE_RUNNING)?(SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN):0;
    SetServiceStatus(service_handle,&service_status);
}

DWORD WINAPI service_handler(DWORD control,DWORD,DWORD,LPVOID) {
    if(control==SERVICE_CONTROL_STOP||control==SERVICE_CONTROL_SHUTDOWN) {
        set_status(SERVICE_STOP_PENDING,NO_ERROR,10000);
        if(stop_event) SetEvent(stop_event);
    }
    return NO_ERROR;
}

void WINAPI service_main(DWORD,LPWSTR*) {
    service_handle=RegisterServiceCtrlHandlerExW(ServiceName,service_handler,nullptr);
    if(!service_handle) return;
    set_status(SERVICE_START_PENDING,NO_ERROR,5000);
    stop_event=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!stop_event) {
        set_status(SERVICE_STOPPED,GetLastError());
        return;
    }

    set_status(SERVICE_RUNNING);
    log(L"[RESIDENT] Servicio iniciado.");

    // First check happens immediately, then the service sleeps. CPU usage while
    // waiting is effectively zero; only a tiny manifest is requested per cycle.
    sync_run_once();
    while(WaitForSingleObject(stop_event,ResidentIntervalMs)==WAIT_TIMEOUT)
        sync_run_once();

    log(L"[RESIDENT] Servicio detenido.");
    CloseHandle(stop_event); stop_event=nullptr;
    set_status(SERVICE_STOPPED);
}

bool run_service_dispatcher() {
    SERVICE_TABLE_ENTRYW table[]={
        {const_cast<LPWSTR>(ServiceName),service_main},
        {nullptr,nullptr}
    };
    return StartServiceCtrlDispatcherW(table)!=FALSE;
}

bool command_line(std::vector<std::wstring>& args) {
    int argc=0; LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(!argv) return false;
    for(int i=0;i<argc;++i) args.push_back(argv[i]);
    LocalFree(argv);
    return true;
}
}

ResidentState resident_state() {
    SC_HANDLE scm=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);
    if(!scm) return ResidentState::Error;
    SC_HANDLE service=OpenServiceW(scm,ServiceName,SERVICE_QUERY_STATUS);
    if(!service) {
        const DWORD code=GetLastError();
        CloseServiceHandle(scm);
        return code==ERROR_SERVICE_DOES_NOT_EXIST?ResidentState::NotInstalled:ResidentState::Error;
    }
    SERVICE_STATUS_PROCESS status{};
    DWORD needed=0;
    const BOOL ok=QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,reinterpret_cast<LPBYTE>(&status),sizeof(status),&needed);
    CloseServiceHandle(service); CloseServiceHandle(scm);
    if(!ok) return ResidentState::Error;
    return status.dwCurrentState==SERVICE_RUNNING?ResidentState::Running:ResidentState::Stopped;
}

bool request_resident_install(HWND owner,DWORD& exit_code,std::wstring& error) {
    if(token_elevated()) {
        const bool ok=install_service_admin(error);
        exit_code=ok?ERROR_SUCCESS:ERROR_INSTALL_FAILURE;
        return ok;
    }
    return elevate(owner,L"--install-resident-admin",exit_code,error);
}

bool request_resident_stop(HWND owner,DWORD& exit_code,std::wstring& error) {
    if(token_elevated()) {
        const bool ok=stop_service_admin(error);
        exit_code=ok?ERROR_SUCCESS:ERROR_SERVICE_REQUEST_TIMEOUT;
        return ok;
    }
    return elevate(owner,L"--stop-resident-admin",exit_code,error);
}

bool request_resident_remove(HWND owner,DWORD& exit_code,std::wstring& error) {
    if(token_elevated()) {
        const bool ok=remove_service_admin(error);
        exit_code=ok?ERROR_SUCCESS:ERROR_INSTALL_FAILURE;
        return ok;
    }
    return elevate(owner,L"--remove-resident-admin",exit_code,error);
}

bool resident_entry(int& exit_code) {
    std::vector<std::wstring> args;
    if(!command_line(args)||args.size()<2) return false;
    const std::wstring command=args[1];

    if(command==L"--josts-resident-service") {
        exit_code=run_service_dispatcher()?ERROR_SUCCESS:GetLastError();
        return true;
    }
    if(command==L"--install-resident-admin") {
        std::wstring error;
        const bool ok=install_service_admin(error);
        if(!ok) log(L"[RESIDENT] Instalación fallida: "+error);
        exit_code=ok?ERROR_SUCCESS:ERROR_INSTALL_FAILURE;
        return true;
    }
    if(command==L"--stop-resident-admin") {
        std::wstring error;
        const bool ok=stop_service_admin(error);
        if(!ok) log(L"[RESIDENT] Detención fallida: "+error);
        exit_code=ok?ERROR_SUCCESS:ERROR_SERVICE_REQUEST_TIMEOUT;
        return true;
    }
    if(command==L"--remove-resident-admin") {
        std::wstring error;
        const bool ok=remove_service_admin(error);
        if(!ok) log(L"[RESIDENT] Desinstalación fallida: "+error);
        exit_code=ok?ERROR_SUCCESS:ERROR_INSTALL_FAILURE;
        return true;
    }
    return false;
}

}
