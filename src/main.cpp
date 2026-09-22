#include "ui.h"
#include "common.h"
#include "privileged_ops.h"

// Aplicación Josts — desarrollada por xdCL.
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    // The one-shot helper must never be swallowed by the normal UI's instance mutex.
    int privileged_result=0;
    try { if(josts::privileged_entry(privileged_result)) return privileged_result; }
    catch(...) { return ERROR_UNHANDLED_EXCEPTION; }
    HANDLE single=CreateMutexW(nullptr,TRUE,L"Local\\JostsHostsManager_xdCL");
    if(single&&GetLastError()==ERROR_ALREADY_EXISTS) {
        HWND existing=FindWindowW(L"JostsMain",nullptr);
        if(existing) { ShowWindow(existing,SW_RESTORE); SetForegroundWindow(existing); }
        CloseHandle(single); return 0;
    }
    josts::log(josts::tr(L"Inicio de Josts v1.1.1 — desarrollado por xdCL."));
    josts::Fonts fonts; fonts.load(instance);
    josts::Ui ui(instance,fonts);
    int result=ui.run(show);
    josts::log(josts::tr(L"Cierre de Josts."));
    if(single) { ReleaseMutex(single); CloseHandle(single); }
    return result;
}
