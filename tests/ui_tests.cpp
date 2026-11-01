#include "ui.h"
#include "parser.h"
#include <commctrl.h>
#include <cassert>
#include <cstdio>

// Hidden integration windows and private hosts fixtures only — xdCL.
namespace josts {
bool test_localized_dialogs(HINSTANCE instance,HFONT font,HFONT icons);
struct UiTestAccess {
    static HWND create(Ui& ui,const std::wstring& dir) {
        ui.data_dir_=dir;
        ui.edge_action_=[] { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return false; };
        ui.hosts_=HostsManager(join(dir,L"system-hosts"),join(dir,L"portable-backup"));
        ui.privileged_action_=[&ui](HWND,const PrivilegedRequest& request) {
            if(request.operation!=PrivilegedOperation::Edge) return execute_privileged(ui.hosts_,request);
            PrivilegedResult result; result.ok=ui.edge_action_(); result.code=GetLastError();
            result.message=result.ok?tr(L"Configuración de Edge guardada y verificada. Abre una nueva pestaña; si aún ves noticias, reinicia Edge."):
                std::wstring(tr(L"No se pudo guardar o verificar la política de Edge: "))+error_message(result.code);
            return result;
        };
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&controls);
        WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.hInstance=ui.instance_;
        wc.lpfnWndProc=Ui::window_proc; wc.lpszClassName=L"JostsHiddenIntegrationTest";
        RegisterClassExW(&wc);
        return CreateWindowExW(0,wc.lpszClassName,L"Josts UI test",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
            0,0,1100,760,nullptr,nullptr,ui.instance_,&ui);
    }
    static bool busy(const Ui& ui) { return ui.busy_; }
    static bool success(const Ui& ui) { return ui.feedback_==Ui::Feedback::Success; }
    static bool failure(const Ui& ui) { return ui.feedback_==Ui::Feedback::Failure; }
    static bool working(const Ui& ui) { return ui.feedback_==Ui::Feedback::Working; }
    static ULONGLONG deadline(const Ui& ui) { return ui.close_at_; }
    static unsigned frame(const Ui& ui) { return ui.animation_frame_; }
    static HWND quick(const Ui& ui) { return ui.quick_apply_; }
    static HWND hint(const Ui& ui) { return ui.simple_hint_; }
    static HWND heading(const Ui& ui) { return ui.simple_heading_; }
    static HWND state(const Ui& ui) { return ui.simple_count_; }
    static void reload(Ui& ui) { ui.load_preload(); }
    static void verify_welcome_countdown(Ui& ui) {
        const auto deadline=ui.close_at_;
        for(int second=5;second>=1;--second) {
            ui.close_at_=GetTickCount64()+second*1000;
            ui.update_countdown();
            wchar_t text[128]={}; GetWindowTextW(ui.simple_hint_,text,128);
            const auto expected=L"Josts se cerrará en "+std::to_wstring(second)+(second==1?L" segundo.":L" segundos.");
            assert(text==expected);
        }
        ui.close_at_=deadline; ui.update_countdown();
    }
    static HWND progress(const Ui& ui) { return ui.progress_; }
    static HWND edge(const Ui& ui) { return ui.edge_button_; }
    static HWND tagline(const Ui& ui) { return ui.tagline_; }
    static void edge_action(Ui& ui,std::function<bool()> action) { ui.edge_action_=action; }
    static void click(Ui& ui,HWND button) { SendMessageW(ui.window_,WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(button),BN_CLICKED),(LPARAM)button); }
    static void keep(Ui& ui) { click(ui,ui.keep_open_); }
    static void advanced(Ui& ui) { click(ui,ui.mode_button_); }
    static void switch_language(Ui& ui) { click(ui,ui.language_button_); }
    static HWND language_button(const Ui& ui) { return ui.language_button_; }
    static HWND search(const Ui& ui) { return ui.search_; }
    static HWND list(const Ui& ui) { return ui.list_; }
    static bool is_advanced(const Ui& ui) { return ui.advanced_; }
    static void dialogs(const Ui& ui) { assert(test_localized_dialogs(ui.instance_,ui.regular_,ui.icons_)); }
    static std::vector<size_t> selected(const Ui& ui) { return ui.selection(); }
    static std::vector<std::wstring> entries(const Ui& ui) {
        std::vector<std::wstring> values;
        for(const auto& entry:ui.entries_) values.push_back(entry.ip+L" "+entry.domain);
        return values;
    }
    static size_t preload_count(const Ui& ui) { return ui.preload_entries_.size(); }
    static std::wstring message(const Ui& ui) { return ui.activity_text_; }
    static std::wstring status(const Ui& ui,const std::wstring& ip,const std::wstring& domain) {
        return ui.entry_status({ip,domain,false});
    }
    static void refresh(Ui& ui) { ui.refresh_system(); }
    static void cancel_permissions(Ui& ui) {
        ui.privileged_action_=[](HWND,const PrivilegedRequest&) {
            return PrivilegedResult{false,ERROR_CANCELLED,elevation_error(ERROR_CANCELLED,false),L""};
        };
    }
};
}
using namespace josts;

static void pump() {
    MSG msg{};
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) {
        if(msg.message!=WM_QUIT) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
}
static void pump_for(ULONGLONG duration) {
    const ULONGLONG until=GetTickCount64()+duration;
    do { pump(); MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT); } while(GetTickCount64()<until);
    pump();
}
static void await_action(Ui& ui) {
    const ULONGLONG until=GetTickCount64()+10000;
    while(UiTestAccess::busy(ui)&&GetTickCount64()<until) pump_for(10);
    assert(!UiTestAccess::busy(ui));
}
static std::wstring caption(HWND control) {
    wchar_t value[2048]={}; GetWindowTextW(control,value,2048); return value;
}
static std::wstring fixture(const std::wstring& root,const wchar_t* name,const std::string& original,bool embedded=false) {
    const std::wstring dir=join(root,name); assert(ensure_dir(dir)); DWORD error=0;
    assert(write_bytes(join(dir,L"system-hosts"),original,error));
    if(!embedded) assert(write_bytes(join(dir,L"hosts.txt"),"0.0.0.0 quick-test.example\r\n",error));
    return dir;
}
static void capture(HWND window,const std::wstring& path) {
    RECT bounds{}; GetClientRect(window,&bounds);
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=bounds.right; info.bmiHeader.biHeight=-bounds.bottom;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen); void* pixels=nullptr;
    HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0); assert(bitmap);
    HGDIOBJ old=SelectObject(dc,bitmap);
    SendMessageW(window,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT);
    for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
        if(!(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE)) continue;
        RECT child_bounds{}; GetWindowRect(child,&child_bounds);
        MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&child_bounds),2);
        const int saved=SaveDC(dc);
        SetViewportOrgEx(dc,child_bounds.left,child_bounds.top,nullptr);
        IntersectClipRect(dc,0,0,child_bounds.right-child_bounds.left,child_bounds.bottom-child_bounds.top);
        SendMessageW(child,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND);
        RestoreDC(dc,saved);
    }
    GdiFlush();
    const DWORD size=bounds.right*bounds.bottom*4;
    BITMAPFILEHEADER header{}; header.bfType=0x4d42; header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER); header.bfSize=header.bfOffBits+size;
    std::string bytes(reinterpret_cast<const char*>(&header),sizeof(header));
    bytes.append(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));
    bytes.append(reinterpret_cast<const char*>(pixels),size); DWORD error=0; assert(write_bytes(path,bytes,error));
    SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(nullptr,screen);
}
int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    const std::wstring root=join(executable_dir(),L"ui-tests-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    assert(ensure_dir(root));
    HINSTANCE instance=GetModuleHandleW(nullptr); Fonts fonts; assert(fonts.load(instance));
    const std::string original="# contenido ajeno\r\n127.0.0.1 tercero.example\r\n";
    DWORD error=0;
    {
        const std::wstring dir=fixture(root,L"embedded",original,true);
        Ui ui(instance,fonts); HWND window=UiTestAccess::create(ui,dir); assert(window);
        await_action(ui); assert(UiTestAccess::preload_count(ui)>50);
        assert(language()==Language::SpanishChile&&language_id()==0x340a);
        assert(caption(UiTestAccess::language_button(ui))==L"English");
        assert(caption(UiTestAccess::quick(ui))==L"Aplicar y cerrar automáticamente");
        assert(caption(UiTestAccess::tagline(ui))==L"No entenderás su utilidad hasta que lo uses");
        assert(caption(UiTestAccess::edge(ui))==L"Quitar noticias de Edge");
        UiTestAccess::dialogs(ui);
        assert(IsWindowEnabled(UiTestAccess::quick(ui)));
        capture(window,join(root,L"simple.bmp"));
        SetWindowPos(window,nullptr,0,0,1020,680,SWP_NOMOVE|SWP_NOZORDER);
        capture(window,join(root,L"simple-minimum.bmp"));
        SetWindowPos(window,nullptr,0,0,1100,760,SWP_NOMOVE|SWP_NOZORDER);
        UiTestAccess::switch_language(ui);
        assert(language()==Language::English&&language_id()==0x0409);
        assert(caption(UiTestAccess::language_button(ui))==L"Español (Chile)");
        assert(caption(UiTestAccess::quick(ui))==L"Apply and close automatically");
        assert(caption(window)==L"Josts — Hosts manager");
        assert(caption(UiTestAccess::edge(ui))==L"Remove Edge news");
        UiTestAccess::dialogs(ui);
        capture(window,join(root,L"simple-english.bmp"));
        UiTestAccess::advanced(ui); capture(window,join(root,L"advanced-english.bmp"));
        const auto entries=UiTestAccess::entries(ui);
        SetWindowTextW(UiTestAccess::search(ui),L"crazygames");
        ListView_SetItemState(UiTestAccess::list(ui),0,LVIS_SELECTED,LVIS_SELECTED);
        const auto selected=UiTestAccess::selected(ui); assert(!selected.empty());
        UiTestAccess::switch_language(ui);
        assert(language()==Language::SpanishChile&&UiTestAccess::is_advanced(ui));
        assert(caption(window)==L"Josts — Gestor de hosts");
        assert(caption(UiTestAccess::search(ui))==L"crazygames");
        assert(UiTestAccess::selected(ui)==selected&&UiTestAccess::entries(ui)==entries);
        SetWindowTextW(UiTestAccess::search(ui),L"");
        UiTestAccess::advanced(ui);
        UiTestAccess::advanced(ui); capture(window,join(root,L"advanced.bmp"));
        SendMessageW(window,WM_CLOSE,0,0); assert(!IsWindow(window)); pump();
    }
    {
        const std::wstring dir=fixture(root,L"success",original);
        Ui ui(instance,fonts); HWND window=UiTestAccess::create(ui,dir); assert(window); await_action(ui);
        // Hold hosts briefly so the worker must wait while the UI continues animating.
        UiTestAccess::switch_language(ui); assert(language()==Language::English);
        HANDLE locked=CreateFileW(join(dir,L"system-hosts").c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
        assert(locked!=INVALID_HANDLE_VALUE);
        UiTestAccess::click(ui,UiTestAccess::quick(ui));
        assert(UiTestAccess::working(ui)&&UiTestAccess::busy(ui));
        assert(!IsWindowEnabled(UiTestAccess::language_button(ui)));
        UiTestAccess::switch_language(ui); assert(language()==Language::English);
        assert(!IsWindowEnabled(UiTestAccess::quick(ui))&&UiTestAccess::deadline(ui)==0);
        UiTestAccess::click(ui,UiTestAccess::quick(ui)); // duplicate click is ignored
        SendMessageW(window,WM_CLOSE,0,0); assert(IsWindow(window));
        pump_for(120); assert(UiTestAccess::frame(ui)>0&&UiTestAccess::busy(ui));
        capture(window,join(root,L"working.bmp"));
        CloseHandle(locked); await_action(ui);
        assert(UiTestAccess::success(ui));
        assert(UiTestAccess::message(ui)==L"List applied and verified in hosts: 1 domain.");
        assert(!IsWindowEnabled(UiTestAccess::language_button(ui)));
        UiTestAccess::switch_language(ui); assert(language()==Language::English);
        std::string bytes; assert(read_bytes(join(dir,L"system-hosts"),bytes,error));
        assert(bytes.find("0.0.0.0 quick-test.example")!=std::string::npos&&bytes.find(original)==0);
        assert(read_bytes(join(dir,L"system-hosts.bak_original"),bytes,error)&&bytes==original);
        assert(read_bytes(join(dir,L"system-hosts.bak_previous"),bytes,error)&&bytes==original);
        assert(!exists(join(dir,L"portable-backup")));
        const ULONGLONG deadline=UiTestAccess::deadline(ui); assert(deadline>GetTickCount64());
        capture(window,join(root,L"success.bmp"));
        bool saw[4]={};
        while(IsWindow(window)&&GetTickCount64()<deadline+1500) {
            const std::wstring text=caption(UiTestAccess::hint(ui));
            for(int second=1;second<=3;++second) if(text.find(std::to_wstring(second)+L" second")!=std::wstring::npos) saw[second]=true;
            pump_for(10);
        }
        assert(!IsWindow(window)&&GetTickCount64()>=deadline);
        assert(saw[1]&&saw[2]&&saw[3]); pump();
    }
    {
        const std::wstring dir=fixture(root,L"keep-open",original);
        Ui ui(instance,fonts); HWND window=UiTestAccess::create(ui,dir); assert(window); await_action(ui);
        assert(language()==Language::SpanishChile);
        UiTestAccess::click(ui,UiTestAccess::quick(ui)); await_action(ui);
        assert(UiTestAccess::success(ui)&&UiTestAccess::deadline(ui));
        const auto countdown=caption(UiTestAccess::hint(ui));
        assert(countdown==L"Josts se cerrará en 3 segundos."||countdown==L"Josts se cerrará en 2 segundos."||countdown==L"Josts se cerrará en 1 segundo.");
        UiTestAccess::keep(ui); assert(UiTestAccess::deadline(ui)==0);
        pump_for(3200); assert(IsWindow(window)&&IsWindowEnabled(UiTestAccess::quick(ui)));
        SendMessageW(window,WM_CLOSE,0,0); pump();
    }
    {
        const std::wstring dir=fixture(root,L"read-only",original);
        Ui ui(instance,fonts); HWND window=UiTestAccess::create(ui,dir); assert(window); await_action(ui);
        UiTestAccess::switch_language(ui);
        assert(SetFileAttributesW(join(dir,L"system-hosts").c_str(),FILE_ATTRIBUTE_READONLY));
        UiTestAccess::click(ui,UiTestAccess::quick(ui)); await_action(ui);
        assert(UiTestAccess::failure(ui)&&UiTestAccess::deadline(ui)==0);
        assert(UiTestAccess::message(ui)==L"Hosts is read-only.");
        std::string bytes; assert(read_bytes(join(dir,L"system-hosts"),bytes,error)&&bytes==original);
        capture(window,join(root,L"failure.bmp"));
        pump_for(3200); assert(IsWindow(window));
        assert(SetFileAttributesW(join(dir,L"system-hosts").c_str(),FILE_ATTRIBUTE_NORMAL));
        SendMessageW(window,WM_CLOSE,0,0); pump();
    }
    {
        // A conflicting external mapping no longer aborts the whole battery. Josts preserves it,
        // applies the remaining rules and stays open so the conflict can be reviewed.
        const std::string conflict="120.0.0.1 quick-test.example\r\n";
        const std::wstring dir=fixture(root,L"conflict",conflict);
        Ui ui(instance,fonts); HWND window=UiTestAccess::create(ui,dir); assert(window); await_action(ui);
        UiTestAccess::click(ui,UiTestAccess::quick(ui)); await_action(ui);
        assert(UiTestAccess::success(ui)&&UiTestAccess::deadline(ui)==0);
        assert(UiTestAccess::status(ui,L"0.0.0.0",L"quick-test.example")==L"Conflicto externo");
        std::string bytes; assert(read_bytes(join(dir,L"system-hosts"),bytes,error));
        assert(bytes.find("120.0.0.1 quick-test.example")!=std::string::npos);
        assert(bytes.find("0.0.0.0 quick-test.example")==std::string::npos);
        assert(exists(join(dir,L"system-hosts.bak_original")));
        SendMessageW(window,WM_CLOSE,0,0); pump();
    }
    {
        const std::wstring dir=fixture(root,L"invalid-list",original);
        assert(write_bytes(join(dir,L"hosts.txt"),"0.0.0.0 bad/domain\r\n",error));
        Ui ui(instance,fonts); HWND window=UiTestAccess::create(ui,dir); assert(window); await_action(ui);
        assert(UiTestAccess::failure(ui)&&!IsWindowEnabled(UiTestAccess::quick(ui))&&UiTestAccess::deadline(ui)==0);
        SendMessageW(window,WM_CLOSE,0,0); pump();
    }
    {
        const std::wstring dir=fixture(root,L"edge-policy",original);
        Ui ui(instance,fonts); HWND window=UiTestAccess::create(ui,dir); assert(window); await_action(ui);
        int calls=0;
        UiTestAccess::edge_action(ui,[&calls] { ++calls; Sleep(120); SetLastError(ERROR_SUCCESS); return true; });
        UiTestAccess::click(ui,UiTestAccess::edge(ui));
        assert(UiTestAccess::busy(ui)&&!IsWindowEnabled(UiTestAccess::edge(ui)));
        UiTestAccess::click(ui,UiTestAccess::edge(ui)); // Duplicate actions stay blocked.
        await_action(ui); assert(calls==1&&UiTestAccess::success(ui)&&!UiTestAccess::deadline(ui));
        assert(UiTestAccess::message(ui).find(L"Configuración de Edge guardada y verificada.")==0);
        capture(window,join(root,L"edge-success.bmp"));
        UiTestAccess::switch_language(ui);
        UiTestAccess::edge_action(ui,[] { SetLastError(ERROR_ACCESS_DENIED); return false; });
        UiTestAccess::click(ui,UiTestAccess::edge(ui)); await_action(ui);
        assert(UiTestAccess::failure(ui)&&!UiTestAccess::deadline(ui)&&IsWindow(window));
        assert(UiTestAccess::message(ui).find(L"Could not save or verify the Edge policy: ")==0);
        capture(window,join(root,L"edge-error-english.bmp"));
        std::string bytes; assert(read_bytes(join(dir,L"system-hosts"),bytes,error)&&bytes==original);
        assert(!exists(join(dir,L"system-hosts.bak_original")));
        SendMessageW(window,WM_CLOSE,0,0); pump();
    }
    const std::string current_patch="# ===== INICIO Josts =====\r\n0.0.0.0 quick-test.example\r\n# ===== FIN Josts =====\r\n";
    {
        const std::string contents=original+current_patch;
        const std::wstring dir=fixture(root,L"welcome-current",contents);
        const auto started=GetTickCount64();
        Ui ui(instance,fonts); HWND window=UiTestAccess::create(ui,dir); assert(window);
        assert(caption(UiTestAccess::heading(ui))==L"Bienvenido a Josts");
        assert(caption(UiTestAccess::state(ui))==L"Estado: comprobando el equipo…");
        await_action(ui);
        const ULONGLONG deadline=UiTestAccess::deadline(ui);
        assert(deadline>GetTickCount64()&&deadline-GetTickCount64()<=5000);
        assert(deadline>=started+5000);
        assert(caption(UiTestAccess::state(ui))==L"Estado: parche al día");
        assert(UiTestAccess::message(ui)==L"Parche al día. No es necesario hacer cambios en hosts.");
        UiTestAccess::verify_welcome_countdown(ui);
        while(IsWindow(window)&&GetTickCount64()<deadline+1500) {
            pump_for(10);
        }
        assert(!IsWindow(window)&&GetTickCount64()>=deadline);
        std::string bytes; assert(read_bytes(join(dir,L"system-hosts"),bytes,error)&&bytes==contents);
        assert(!exists(join(dir,L"system-hosts.bak_original"))&&!exists(join(dir,L"portable-backup")));
        pump();
    }
    {
        const std::wstring dir=fixture(root,L"welcome-keep-open",original+"127.0.0.1 localhost\r\n::1 localhost\r\n"+current_patch);
        assert(write_bytes(join(dir,L"hosts.txt"),"127.0.0.1 localhost\r\n0.0.0.0 quick-test.example\r\n",error));
        Ui ui(instance,fonts); HWND window=UiTestAccess::create(ui,dir); assert(window); await_action(ui);
        capture(window,join(root,L"welcome-current.bmp"));
        assert(UiTestAccess::deadline(ui)); UiTestAccess::keep(ui);
        assert(!UiTestAccess::deadline(ui)&&IsWindowEnabled(UiTestAccess::edge(ui)));
        assert(UiTestAccess::message(ui)==L"No se realizaron cambios en hosts. Josts permanecerá abierto.");
        UiTestAccess::switch_language(ui); UiTestAccess::advanced(ui);
        // Reloading later must not unexpectedly restart the welcome countdown.
        UiTestAccess::reload(ui); await_action(ui); assert(!UiTestAccess::deadline(ui));
        pump_for(5200); assert(IsWindow(window)); SendMessageW(window,WM_CLOSE,0,0); pump();
    }
    const std::vector<std::pair<const wchar_t*,std::string>> incomplete={
        {L"welcome-missing",original},
        {L"welcome-foreign-only",original+"0.0.0.0 quick-test.example\r\n"},
        {L"welcome-old",original+"# ===== INICIO Josts =====\r\n0.0.0.0 old.example\r\n# ===== FIN Josts =====\r\n"},
        {L"welcome-wrong-ip",original+"# ===== INICIO Josts =====\r\n127.0.0.1 quick-test.example\r\n# ===== FIN Josts =====\r\n"},
        {L"welcome-conflict",original+current_patch+"127.0.0.1 quick-test.example\r\n"},
        {L"welcome-obsolete",original+"# ===== INICIO Josts =====\r\n0.0.0.0 quick-test.example\r\n0.0.0.0 old.example\r\n# ===== FIN Josts =====\r\n"},
        {L"welcome-malformed",original+"# ===== INICIO Josts =====\r\n0.0.0.0 quick-test.example\r\n"}
    };
    for(size_t i=0;i<incomplete.size();++i) {
        const auto& item=incomplete[i]; const std::wstring dir=fixture(root,item.first,item.second);
        Ui ui(instance,fonts); HWND window=UiTestAccess::create(ui,dir); assert(window); await_action(ui);
        assert(!UiTestAccess::deadline(ui));
        const auto expected=i<2?L"Estado: sin parche de Josts":i==6?L"Estado: no se pudo verificar":L"Estado: Josts activo; lista diferente";
        assert(caption(UiTestAccess::state(ui))==expected);
        if(i==2) {
            capture(window,join(root,L"welcome-update.bmp"));
            UiTestAccess::switch_language(ui);
            assert(caption(UiTestAccess::heading(ui))==L"Welcome to Josts");
            assert(caption(UiTestAccess::state(ui))==L"Status: Josts active; different list");
        }
        std::string bytes; assert(read_bytes(join(dir,L"system-hosts"),bytes,error)&&bytes==item.second);
        assert(!exists(join(dir,L"system-hosts.bak_original")));
        SendMessageW(window,WM_CLOSE,0,0); pump();
    }
    for(bool empty:{false,true}) {
        const std::wstring dir=fixture(root,empty?L"welcome-empty":L"welcome-invalid",original+current_patch);
        assert(write_bytes(join(dir,L"hosts.txt"),empty?"":"0.0.0.0 bad/domain\r\n",error));
        Ui ui(instance,fonts); HWND window=UiTestAccess::create(ui,dir); assert(window); await_action(ui);
        assert(!UiTestAccess::deadline(ui)&&!IsWindowEnabled(UiTestAccess::quick(ui)));
        assert(caption(UiTestAccess::state(ui))==L"Estado: Josts activo; precarga no verificable");
        SendMessageW(window,WM_CLOSE,0,0); pump();
    }
    {
        // The student's portable preset differs from the active administrator-applied block.
        const std::wstring dir=fixture(root,L"shared-global-state",original+current_patch);
        assert(write_bytes(join(dir,L"hosts.txt"),"0.0.0.0 different-preset.example\r\n",error));
        Ui ui(instance,fonts); HWND window=UiTestAccess::create(ui,dir); await_action(ui);
        const auto entries=UiTestAccess::entries(ui);
        assert(std::find(entries.begin(),entries.end(),L"0.0.0.0 quick-test.example")!=entries.end());
        assert(UiTestAccess::status(ui,L"0.0.0.0",L"quick-test.example")==L"Aplicada por Josts");
        assert(UiTestAccess::status(ui,L"127.0.0.1",L"quick-test.example")==L"No aplicada");
        assert(!UiTestAccess::deadline(ui));
        UiTestAccess::cancel_permissions(ui);
        UiTestAccess::click(ui,UiTestAccess::quick(ui)); await_action(ui);
        assert(UiTestAccess::failure(ui)&&!UiTestAccess::deadline(ui)&&IsWindow(window));
        assert(UiTestAccess::message(ui).find(L"Solicitud de permisos cancelada.")==0);
        std::string bytes; assert(read_bytes(join(dir,L"system-hosts"),bytes,error)&&bytes==original+current_patch);
        assert(write_bytes(join(dir,L"system-hosts"),original,error));
        UiTestAccess::refresh(ui); await_action(ui);
        assert(UiTestAccess::status(ui,L"0.0.0.0",L"quick-test.example")==L"No aplicada");
        const auto prepared=UiTestAccess::entries(ui);
        assert(std::find(prepared.begin(),prepared.end(),L"0.0.0.0 different-preset.example")!=prepared.end());
        SendMessageW(window,WM_CLOSE,0,0); pump();
    }
    std::printf("UI: global state, external refresh, UAC cancellation, welcome, Edge, languages and countdowns passed.\n");
    return 0;
}
