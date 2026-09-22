#include "ui.h"
#include "parser.h"
#include "resource.h"
#include "theme.h"
#include <commctrl.h>
#include <commdlg.h>
#include <algorithm>
#include <set>
#include <thread>
#include <memory>
#include <cstring>

namespace josts {
static void dark_caption_when_supported(HWND window);
enum { ID_LIST=1001,ID_SEARCH,ID_APPLY,ID_RESTORE,ID_UNPATCH,ID_IMPORT,ID_ADD,ID_RELOAD,ID_EXPORT,ID_EDIT,ID_DELETE,ID_UP,ID_DOWN,ID_APPLY_SELECTED,ID_ABOUT,ID_HEADER,ID_MODE,ID_SIMPLE_APPLY,ID_SIMPLE_UNPATCH,ID_QUICK_APPLY,ID_KEEP_OPEN,ID_PROGRESS,ID_LANGUAGE,ID_EDGE_NEWS };
static const std::pair<int,const wchar_t*> button_labels[]={
        {ID_APPLY,L"Aplicar parche"},{ID_APPLY_SELECTED,L"Aplicar selección"},{ID_RESTORE,L"Restablecer hosts"},
        {ID_UNPATCH,L"Desparchear solo mis entradas"},{ID_IMPORT,L"Cargar lista (.txt / .md)"},
        {ID_ADD,L"Agregar entrada manual"},{ID_EDIT,L"Editar"},{ID_DELETE,L"Eliminar"},
        {ID_UP,L"Subir selección"},{ID_DOWN,L"Bajar selección"},
        {ID_RELOAD,L"Recargar precarga (hosts.txt)"},{ID_EXPORT,L"Exportar lista actual"}};
enum { ACTION_APPLY,ACTION_UNPATCH,ACTION_RESTORE,ACTION_IMPORT,ACTION_EXPORT,ACTION_PRELOAD,ACTION_EDGE_NEWS,ACTION_REFRESH };
enum { STAGE_READING,STAGE_WRITING,STAGE_VERIFYING };
static const UINT WM_ACTION_DONE=WM_APP+1,WM_ACTION_STAGE=WM_APP+2;
static const UINT_PTR TIMER_PROGRESS=1,TIMER_CLOSE=2,TIMER_HOSTS=3;
static wchar_t material_glyph(int id) {
    switch(id) {
        case ID_APPLY: return 0xe171;            // save_alt
        case ID_SIMPLE_APPLY: return 0xe171;     // save_alt
        case ID_QUICK_APPLY: return 0xe065;      // playlist_add_check
        case ID_KEEP_OPEN: return 0xe5cd;        // close (cancel countdown)
        case ID_APPLY_SELECTED: return 0xe065;   // playlist_add_check
        case ID_RESTORE: return 0xe8b3;          // restore
        case ID_UNPATCH: return 0xe16c;          // delete_sweep
        case ID_SIMPLE_UNPATCH: return 0xe16c;   // delete_sweep
        case ID_IMPORT: return 0xe2c6;           // file_upload
        case ID_ADD: return 0xe148;              // add_circle_outline
        case ID_EDIT: return 0xe3c9;             // edit
        case ID_DELETE: return 0xe92e;           // delete_outline
        case ID_UP: return 0xe5d8;               // arrow_upward
        case ID_DOWN: return 0xe5db;             // arrow_downward
        case ID_RELOAD: return 0xe5d5;           // refresh
        case ID_EXPORT: return 0xe2c4;           // file_download
        case ID_ABOUT: return 0xe88f;            // info_outline
        case ID_EDGE_NEWS: return 0xe0e5;        // rss_feed
        case ID_LANGUAGE: return 0xe894;         // language
        case ID_MODE: return 0xe8ef;             // view_list
        default: return 0;
    }
}
struct ActionResult {
    bool ok=false,quick=false,snapshot_valid=false;
    int operation=0;
    std::wstring message,warning;
    Snapshot snapshot;
    std::set<std::wstring> applied;
    std::vector<Entry> entries;
    size_t duplicates=0,errors=0;
};
struct NoticeData { HINSTANCE instance; HFONT font,icons; const wchar_t* message; const wchar_t* title; UINT flags; };
static INT_PTR CALLBACK notice_dialog(HWND h,UINT msg,WPARAM w,LPARAM l) {
    NoticeData* data=reinterpret_cast<NoticeData*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(msg==WM_INITDIALOG) {
        data=reinterpret_cast<NoticeData*>(l); SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)data);
        dark_caption_when_supported(h);
        SetWindowTextW(h,data->title); SetDlgItemTextW(h,IDC_NOTICE_TEXT,data->message);
        for(HWND child=GetWindow(h,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) SendMessageW(child,WM_SETFONT,(WPARAM)data->font,TRUE);
        SetDlgItemTextW(h,IDOK,tr(L"Aceptar")); SetDlgItemTextW(h,IDYES,tr(L"Sí"));
        SetDlgItemTextW(h,IDNO,tr(L"No")); SetDlgItemTextW(h,IDCANCEL,tr(L"Cancelar"));
        UINT type=data->flags&MB_TYPEMASK;
        ShowWindow(GetDlgItem(h,IDOK),type==MB_OK?SW_SHOW:SW_HIDE);
        ShowWindow(GetDlgItem(h,IDYES),type==MB_OK?SW_HIDE:SW_SHOW);
        ShowWindow(GetDlgItem(h,IDNO),type==MB_OK?SW_HIDE:SW_SHOW);
        ShowWindow(GetDlgItem(h,IDCANCEL),type==MB_YESNOCANCEL?SW_SHOW:SW_HIDE);
        HWND focus=GetDlgItem(h,type==MB_OK?IDOK:(data->flags&MB_DEFBUTTON2)?IDNO:IDYES);
        SetFocus(focus); return FALSE;
    }
    if(msg==WM_ERASEBKGND) { RECT r{}; GetClientRect(h,&r); FillRect((HDC)w,&r,theme::background_brush()); return TRUE; }
    if(msg==WM_CTLCOLORDLG) return reinterpret_cast<INT_PTR>(theme::background_brush());
    if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLOREDIT) return theme::color_control(msg,(HDC)w,(HWND)l);
    if(msg==WM_DRAWITEM&&data) {
        int id=LOWORD(w); wchar_t glyph=id==IDYES||id==IDOK?0xe5ca:id==IDNO?0xe14b:0xe5cd;
        theme::paint_button(reinterpret_cast<DRAWITEMSTRUCT*>(l),data->font,data->icons,glyph,id==IDOK);
        return TRUE;
    }
    if(msg==WM_COMMAND) {
        int id=LOWORD(w);
        if(id==IDOK||id==IDYES||id==IDNO||id==IDCANCEL) { EndDialog(h,id); return TRUE; }
    }
    return FALSE;
}
static int dark_message(HINSTANCE instance,HFONT font,HFONT icons,HWND owner,const wchar_t* message,const wchar_t* title,UINT flags) {
    NoticeData data{instance,font,icons,message,title,flags};
    return static_cast<int>(DialogBoxParamW(instance,MAKEINTRESOURCEW(IDD_NOTICE),owner,notice_dialog,(LPARAM)&data));
}
struct EntryDialog { Entry value; HFONT font=nullptr,icons=nullptr; };
static void font_children(HWND parent,HFONT font) {
    for(HWND child=GetWindow(parent,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) SendMessageW(child,WM_SETFONT,(WPARAM)font,TRUE);
}
static INT_PTR CALLBACK entry_dialog(HWND h,UINT msg,WPARAM w,LPARAM l) {
    EntryDialog* data=reinterpret_cast<EntryDialog*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(msg==WM_INITDIALOG) {
        data=reinterpret_cast<EntryDialog*>(l); SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)data);
        dark_caption_when_supported(h);
        font_children(h,data->font);
        SetWindowTextW(h,tr(L"Entrada de hosts"));
        SetDlgItemTextW(h,IDC_IP_LABEL,tr(L"Dirección IP:")); SetDlgItemTextW(h,IDC_DOMAIN_LABEL,tr(L"Dominio:"));
        SetDlgItemTextW(h,IDOK,tr(L"Guardar")); SetDlgItemTextW(h,IDCANCEL,tr(L"Cancelar"));
        SetDlgItemTextW(h,IDC_IP,data->value.ip.c_str()); SetDlgItemTextW(h,IDC_DOMAIN,data->value.domain.c_str());
        SetFocus(GetDlgItem(h,IDC_DOMAIN)); return FALSE;
    }
    if(msg==WM_ERASEBKGND) { RECT r{}; GetClientRect(h,&r); FillRect((HDC)w,&r,theme::background_brush()); return TRUE; }
    if(msg==WM_CTLCOLORDLG) return reinterpret_cast<INT_PTR>(theme::background_brush());
    if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLOREDIT) return theme::color_control(msg,(HDC)w,(HWND)l);
    if(msg==WM_DRAWITEM&&data) {
        DRAWITEMSTRUCT* item=reinterpret_cast<DRAWITEMSTRUCT*>(l);
        theme::paint_button(item,data->font,data->icons,LOWORD(w)==IDOK?0xe5ca:0xe5cd,LOWORD(w)==IDOK);
        return TRUE;
    }
    if(msg==WM_COMMAND) {
        if(LOWORD(w)==IDCANCEL) { EndDialog(h,IDCANCEL); return TRUE; }
        if(LOWORD(w)==IDOK) {
            wchar_t ip[128]={},domain[512]={};
            GetDlgItemTextW(h,IDC_IP,ip,128); GetDlgItemTextW(h,IDC_DOMAIN,domain,512);
            data->value.ip=trim(ip); data->value.domain=lower(trim(domain));
            if(!valid_ip(data->value.ip)||!valid_domain(data->value.domain)) { dark_message(GetModuleHandleW(nullptr),data->font,data->icons,h,tr(L"Escriba una IP y un dominio válidos."),L"Josts",MB_OK|MB_ICONWARNING); return TRUE; }
            EndDialog(h,IDOK); return TRUE;
        }
    }
    return FALSE;
}
struct AboutData { HFONT font,icons; HINSTANCE instance; };
static std::wstring resource_text(HINSTANCE instance,int id) {
    HRSRC res=FindResourceW(instance,MAKEINTRESOURCEW(id),RT_RCDATA);
    if(!res) return tr(L"Licencia no disponible.");
    HGLOBAL block=LoadResource(instance,res);
    const char* bytes=block?reinterpret_cast<const char*>(LockResource(block)):nullptr;
    DWORD n=SizeofResource(instance,res);
    if(!bytes||!n) return tr(L"Licencia no disponible.");
    int count=MultiByteToWideChar(CP_UTF8,0,bytes,n,nullptr,0);
    if(!count) return tr(L"Licencia no disponible.");
    std::wstring result(count,L'\0');
    MultiByteToWideChar(CP_UTF8,0,bytes,n,&result[0],count);
    std::wstring normalized;
    normalized.reserve(result.size()+result.size()/30);
    for(size_t i=0;i<result.size();++i) {
        if(result[i]==L'\n'&&(i==0||result[i-1]!=L'\r')) normalized+=L'\r';
        normalized+=result[i];
    }
    return normalized;
}
static INT_PTR CALLBACK about_dialog(HWND h,UINT msg,WPARAM w,LPARAM l) {
    AboutData* data=reinterpret_cast<AboutData*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(msg==WM_INITDIALOG) {
        data=reinterpret_cast<AboutData*>(l); SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)data); font_children(h,data->font);
        dark_caption_when_supported(h);
        SetWindowTextW(h,tr(L"Acerca de Josts"));
        SetDlgItemTextW(h,IDC_ABOUT_TAGLINE,tr(L"No entenderás su utilidad hasta que lo uses"));
        SetDlgItemTextW(h,IDOK,tr(L"Cerrar"));
        SetDlgItemTextW(h,IDC_ABOUT_TITLE,tr(L"Josts v1.1.1 — Gestor de hosts para entornos educativos"));
        SetDlgItemTextW(h,IDC_ABOUT_AUTHOR,tr(L"Desarrollado por xdCL"));
        SetDlgItemTextW(h,IDC_ABOUT_FONT,tr(L"Tipografía: DM Sans (Google Fonts, SIL Open Font License 1.1)"));
        SetDlgItemTextW(h,IDC_ABOUT_COPYRIGHT,tr(L"© xdCL — Uso gratuito para fines o entornos educativos"));
        SetDlgItemTextW(h,IDC_ABOUT_ICONS,tr(L"Iconos: Material Icons (Google, Apache License 2.0)"));
        std::wstring licenses=resource_text(data->instance,language()==Language::English?IDR_LICENSE_JOSTS_EN:IDR_LICENSE_JOSTS_ES)+L"\r\n\r\nDM Sans — SIL Open Font License 1.1\r\n\r\n"+
            resource_text(data->instance,IDR_LICENSE)+L"\r\n\r\nMaterial Icons — Apache License 2.0\r\n\r\n"+
            resource_text(data->instance,IDR_LICENSE_MATERIAL_ICONS)+tr(L"\r\n\r\nLLVM-MinGW — licencias de componentes\r\n\r\n")+
            resource_text(data->instance,IDR_LICENSE_LLVM_MINGW);
        SetDlgItemTextW(h,IDC_LICENSE,licenses.c_str());
        return TRUE;
    }
    if(msg==WM_ERASEBKGND) { RECT r{}; GetClientRect(h,&r); FillRect((HDC)w,&r,theme::background_brush()); return TRUE; }
    if(msg==WM_CTLCOLORDLG) return reinterpret_cast<INT_PTR>(theme::background_brush());
    if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLOREDIT) return theme::color_control(msg,(HDC)w,(HWND)l);
    if(msg==WM_DRAWITEM&&data) { theme::paint_button(reinterpret_cast<DRAWITEMSTRUCT*>(l),data->font,data->icons,0xe5cd,false); return TRUE; }
    if(msg==WM_COMMAND&&(LOWORD(w)==IDOK||LOWORD(w)==IDCANCEL)) { EndDialog(h,IDOK); return TRUE; }
    return FALSE;
}
#ifdef JOSTS_UI_TESTS
// Exercise the actual dialog initialization without showing windows or touching hosts.
bool test_localized_dialogs(HINSTANCE instance,HFONT font,HFONT icons) {
    auto matches=[](HWND h,int id,const wchar_t* expected) {
        wchar_t value[512]={};
        if(id) GetDlgItemTextW(h,id,value,512); else GetWindowTextW(h,value,512);
        return std::wstring(value)==expected;
    };
    NoticeData notice{instance,font,icons,L"Test",L"Josts",MB_YESNOCANCEL};
    HWND h=CreateDialogParamW(instance,MAKEINTRESOURCEW(IDD_NOTICE),nullptr,notice_dialog,(LPARAM)&notice);
    bool ok=h&&matches(h,IDYES,tr(L"Sí"))&&matches(h,IDNO,tr(L"No"))&&matches(h,IDCANCEL,tr(L"Cancelar"))&&matches(h,IDOK,tr(L"Aceptar"));
    if(h) DestroyWindow(h);
    EntryDialog entry; entry.font=font; entry.icons=icons;
    h=CreateDialogParamW(instance,MAKEINTRESOURCEW(IDD_ENTRY),nullptr,entry_dialog,(LPARAM)&entry);
    ok=ok&&h&&matches(h,0,tr(L"Entrada de hosts"))&&matches(h,IDC_IP_LABEL,tr(L"Dirección IP:"))&&matches(h,IDC_DOMAIN_LABEL,tr(L"Dominio:"))&&matches(h,IDOK,tr(L"Guardar"))&&matches(h,IDCANCEL,tr(L"Cancelar"));
    if(h) DestroyWindow(h);
    AboutData about{font,icons,instance};
    h=CreateDialogParamW(instance,MAKEINTRESOURCEW(IDD_ABOUT),nullptr,about_dialog,(LPARAM)&about);
    ok=ok&&h&&matches(h,0,tr(L"Acerca de Josts"))&&matches(h,IDC_ABOUT_AUTHOR,tr(L"Desarrollado por xdCL"))&&matches(h,IDOK,tr(L"Cerrar"))&&matches(h,IDC_ABOUT_TAGLINE,tr(L"No entenderás su utilidad hasta que lo uses"))&&matches(h,IDC_ABOUT_COPYRIGHT,tr(L"© xdCL — Uso gratuito para fines o entornos educativos"));
    wchar_t license_start[128]={};
    if(h) GetDlgItemTextW(h,IDC_LICENSE,license_start,128);
    ok=ok&&std::wstring(license_start).find(language()==Language::English?L"Josts — Educational use":L"Josts — Licencia de uso educativo")==0;
    if(h) DestroyWindow(h);
    return ok;
}
#endif
Ui::Ui(HINSTANCE instance,Fonts& fonts):instance_(instance),fonts_(fonts),data_dir_(executable_dir()) { set_language(Language::SpanishChile); }
Ui::~Ui() { if(worker_.joinable()) worker_.join(); }
static void dark_caption_when_supported(HWND window) {
    HMODULE dwm=LoadLibraryW(L"dwmapi.dll");
    if(!dwm) return;
    typedef HRESULT (WINAPI *SetAttribute)(HWND,DWORD,LPCVOID,DWORD);
    FARPROC address=GetProcAddress(dwm,"DwmSetWindowAttribute");
    SetAttribute set=nullptr;
    if(address) { static_assert(sizeof(set)==sizeof(address),"Incompatible function pointer size"); std::memcpy(&set,&address,sizeof(set)); }
    if(set) {
        BOOL dark=TRUE;
        if(FAILED(set(window,20,&dark,sizeof(dark)))) set(window,19,&dark,sizeof(dark));
    }
    FreeLibrary(dwm);
}
int Ui::run(int show) {
    INITCOMMONCONTROLSEX init{sizeof(init),ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&init);
    WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.hInstance=instance_; wc.lpfnWndProc=window_proc;
    wc.lpszClassName=L"JostsMain"; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hIcon=LoadIconW(instance_,MAKEINTRESOURCEW(IDI_JOSTS)); wc.hIconSm=wc.hIcon;
    wc.hbrBackground=theme::background_brush();
    RegisterClassExW(&wc);
    window_=CreateWindowExW(0,wc.lpszClassName,tr(L"Josts — Gestor de hosts"),WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1100,760,nullptr,nullptr,instance_,this);
    if(!window_) return 1;
    dark_caption_when_supported(window_);
    ShowWindow(window_,show); UpdateWindow(window_);
    MSG message{};
    while(GetMessageW(&message,nullptr,0,0)>0) { TranslateMessage(&message); DispatchMessageW(&message); }
    return static_cast<int>(message.wParam);
}
LRESULT CALLBACK Ui::window_proc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    Ui* self=reinterpret_cast<Ui*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(msg==WM_NCCREATE) { self=reinterpret_cast<Ui*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams); self->window_=h; SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)self); }
    return self?self->handle(msg,w,l):DefWindowProcW(h,msg,w,l);
}
static HWND control(HINSTANCE instance,HWND parent,const wchar_t* cls,const wchar_t* label,DWORD style,DWORD ex,int id) {
    return CreateWindowExW(ex,cls,label,style,0,0,100,24,parent,(HMENU)(INT_PTR)id,instance,nullptr);
}
void Ui::create_controls() {
    regular_=fonts_.create(10); bold_=fonts_.create(10,FW_BOLD); title_=fonts_.create(16,FW_BOLD); icons_=fonts_.create_icons(18);
    brand_=control(instance_,window_,L"STATIC",L"Josts",WS_CHILD|WS_VISIBLE,0,0);
    status_=control(instance_,window_,L"STATIC",tr(L"Estado de hosts: verificando…"),WS_CHILD|WS_VISIBLE,0,0);
    admin_=control(instance_,window_,L"STATIC",tr(L"Privilegios: verificando…"),WS_CHILD|WS_VISIBLE,0,0);
    mode_button_=control(instance_,window_,L"BUTTON",tr(L"Modo avanzado"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,ID_MODE);
    language_button_=control(instance_,window_,L"BUTTON",L"English",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,ID_LANGUAGE);
    about_button_=control(instance_,window_,L"BUTTON",tr(L"Acerca de"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,ID_ABOUT);
    edge_button_=control(instance_,window_,L"BUTTON",tr(L"Quitar noticias de Edge"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,ID_EDGE_NEWS);
    tagline_=control(instance_,window_,L"STATIC",tr(L"No entenderás su utilidad hasta que lo uses"),WS_CHILD|WS_VISIBLE,0,0);
    simple_heading_=control(instance_,window_,L"STATIC",tr(L"Aplicar la lista precargada"),WS_CHILD|WS_VISIBLE,0,0);
    simple_description_=control(instance_,window_,L"STATIC",tr(L"Bloquea en este equipo los dominios incluidos en Josts. Se guardará un respaldo antes de hacer cambios."),WS_CHILD|WS_VISIBLE,0,0);
    simple_count_=control(instance_,window_,L"STATIC",tr(L"Cargando precarga…"),WS_CHILD|WS_VISIBLE,0,0);
    simple_apply_=control(instance_,window_,L"BUTTON",tr(L"Aplicar lista precargada"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,ID_SIMPLE_APPLY);
    simple_unpatch_=control(instance_,window_,L"BUTTON",tr(L"Quitar bloqueos de Josts"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,ID_SIMPLE_UNPATCH);
    quick_apply_=control(instance_,window_,L"BUTTON",tr(L"Aplicar y cerrar automáticamente"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,ID_QUICK_APPLY);
    keep_open_=control(instance_,window_,L"BUTTON",tr(L"Mantener abierto"),WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,0,ID_KEEP_OPEN);
    simple_hint_=control(instance_,window_,L"STATIC",tr(L"Verificando el estado del equipo…"),WS_CHILD|WS_VISIBLE,0,0);
    simple_footer_=control(instance_,window_,L"STATIC",tr(L"Un clic: Windows solicitará permisos si hacen falta. Tras aplicar y verificar la precarga, Josts se cierra en 3 segundos."),WS_CHILD|WS_VISIBLE,0,0);
    progress_text_=control(instance_,window_,L"STATIC",tr(L"Listo"),WS_CHILD|WS_VISIBLE|SS_ENDELLIPSIS,0,0);
    progress_=control(instance_,window_,L"STATIC",tr(L"Progreso de la operación"),WS_CHILD|WS_VISIBLE|SS_OWNERDRAW,0,ID_PROGRESS);
    search_=control(instance_,window_,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,0,ID_SEARCH);
    SendMessageW(search_,EM_SETCUEBANNER,TRUE,(LPARAM)tr(L"Buscar por dominio o IP"));
    header_=control(instance_,window_,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_OWNERDRAW,0,ID_HEADER);
    list_=control(instance_,window_,WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_NOCOLUMNHEADER|LVS_OWNERDATA|LVS_SHOWSELALWAYS,0,ID_LIST);
    ListView_SetExtendedListViewStyle(list_,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(list_,theme::panel());
    ListView_SetTextBkColor(list_,theme::panel());
    ListView_SetTextColor(list_,theme::text());
    const wchar_t* names[]={L"IP",tr(L"Dominio"),tr(L"Estado")};
    const int widths[]={135,380,165};
    for(int i=0;i<3;i++) { LVCOLUMNW c{}; c.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM; c.pszText=const_cast<LPWSTR>(names[i]); c.cx=widths[i]; c.iSubItem=i; ListView_InsertColumn(list_,i,&c); }
    for(const auto& item:button_labels) buttons_.push_back(control(instance_,window_,L"BUTTON",tr(item.second),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,item.first));
    console_=control(instance_,window_,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,0,0);
    for(HWND c:{status_,admin_,search_,list_,console_,about_button_,mode_button_,language_button_,edge_button_,tagline_,simple_description_,simple_hint_,simple_footer_,simple_apply_,simple_unpatch_,quick_apply_,keep_open_,progress_text_}) SendMessageW(c,WM_SETFONT,(WPARAM)regular_,TRUE);
    for(HWND c:{brand_,simple_heading_}) SendMessageW(c,WM_SETFONT,(WPARAM)title_,TRUE);
    SendMessageW(simple_count_,WM_SETFONT,(WPARAM)bold_,TRUE);
    SendMessageW(header_,WM_SETFONT,(WPARAM)bold_,TRUE);
    for(HWND c:buttons_) SendMessageW(c,WM_SETFONT,(WPARAM)regular_,TRUE);
    set_mode(false);
    elevated_=token_elevated();
    SetWindowTextW(admin_,elevated_?tr(L"Admin: activo"):tr(L"Admin: al aplicar"));
    if(!elevated_) report(tr(L"Puedes revisar el estado sin privilegios. Al aplicar un cambio, Windows solicitará permisos de administrador."));
    report(tr(L"Archivo hosts del sistema: ")+hosts_.path());
    SetTimer(window_,TIMER_HOSTS,2000,nullptr);
    load_preload();
}
void Ui::layout() {
    if(!list_) return;
    RECT r{}; GetClientRect(window_,&r); int w=r.right,h=r.bottom;
    if(w<=0||h<=0) return;
    MoveWindow(brand_,16,12,90,32,TRUE);
    MoveWindow(status_,112,12,w-628,23,TRUE);
    MoveWindow(admin_,112,36,w-628,20,TRUE);
    MoveWindow(language_button_,w-504,13,168,30,TRUE);
    MoveWindow(mode_button_,w-320,13,176,30,TRUE);
    MoveWindow(about_button_,w-130,13,114,30,TRUE);
    MoveWindow(edge_button_,16,64,260,32,TRUE);
    MoveWindow(tagline_,296,72,w-312,24,TRUE);
    MoveWindow(progress_text_,16,h-56,w-32,28,TRUE);
    MoveWindow(progress_,16,h-22,w-32,8,TRUE);
    if(!advanced_) {
        const int card_w=std::min(720,w-48),card_h=450;
        const int x=(w-card_w)/2,y=std::max(112,(h-64-card_h)/2);
        MoveWindow(simple_heading_,x+30,y+24,card_w-60,36,TRUE);
        MoveWindow(simple_description_,x+30,y+64,card_w-60,40,TRUE);
        MoveWindow(simple_count_,x+30,y+112,card_w-60,28,TRUE);
        MoveWindow(quick_apply_,x+30,y+150,card_w-60,50,TRUE);
        MoveWindow(simple_footer_,x+30,y+212,card_w-60,42,TRUE);
        MoveWindow(simple_apply_,x+30,y+272,310,44,TRUE);
        MoveWindow(simple_unpatch_,x+356,y+272,card_w-386,44,TRUE);
        MoveWindow(simple_hint_,x+30,y+328,card_w-60,52,TRUE);
        MoveWindow(keep_open_,x+30,y+392,230,34,TRUE);
        InvalidateRect(window_,nullptr,FALSE);
        return;
    }
    int right=std::max(720,w-270); int log_h=std::max(70,std::min(140,h-570));
    MoveWindow(search_,19,112,right-38,24,TRUE);
    MoveWindow(header_,16,151,right-32,30,TRUE);
    MoveWindow(list_,17,182,right-34,std::max(200,h-log_h-267),TRUE);
    int by=109; for(HWND b:buttons_) { MoveWindow(b,right,by,250,28,TRUE); by+=32; }
    MoveWindow(console_,18,h-log_h-68,w-36,log_h-4,TRUE);
    ListView_SetColumnWidth(list_,0,135); ListView_SetColumnWidth(list_,2,165);
    ListView_SetColumnWidth(list_,1,std::max(300,right-34-135-165-GetSystemMetrics(SM_CXVSCROLL)-4));
    InvalidateRect(header_,nullptr,FALSE);
    InvalidateRect(window_,nullptr,FALSE);
}
void Ui::set_mode(bool advanced) {
    advanced_=advanced;
    for(HWND c:{search_,header_,list_,console_}) ShowWindow(c,advanced?SW_SHOW:SW_HIDE);
    for(HWND c:buttons_) ShowWindow(c,advanced?SW_SHOW:SW_HIDE);
    for(HWND c:{simple_heading_,simple_description_,simple_count_,simple_hint_,simple_footer_,simple_apply_,simple_unpatch_,quick_apply_}) ShowWindow(c,advanced?SW_HIDE:SW_SHOW);
    ShowWindow(keep_open_,!advanced&&close_at_?SW_SHOW:SW_HIDE);
    SetWindowTextW(mode_button_,advanced?tr(L"Modo simple"):tr(L"Modo avanzado"));
    update_simple_view(); layout();
}
void Ui::toggle_language() {
    if(busy_||close_at_) return;
    set_language(language()==Language::SpanishChile?Language::English:Language::SpanishChile);
    SetWindowTextW(window_,tr(L"Josts — Gestor de hosts"));
    SetWindowTextW(language_button_,language()==Language::SpanishChile?L"English":L"Español (Chile)");
    SetWindowTextW(about_button_,tr(L"Acerca de"));
    SetWindowTextW(edge_button_,tr(L"Quitar noticias de Edge"));
    SetWindowTextW(tagline_,tr(L"No entenderás su utilidad hasta que lo uses"));
    SetWindowTextW(admin_,elevated_?tr(L"Admin: activo"):tr(L"Admin: al aplicar"));
    SetWindowTextW(simple_description_,tr(L"Bloquea en este equipo los dominios incluidos en Josts. Se guardará un respaldo antes de hacer cambios."));
    SetWindowTextW(simple_footer_,tr(L"Un clic: Windows solicitará permisos si hacen falta. Tras aplicar y verificar la precarga, Josts se cierra en 3 segundos."));
    SetWindowTextW(simple_unpatch_,tr(L"Quitar bloqueos de Josts"));
    SetWindowTextW(quick_apply_,tr(L"Aplicar y cerrar automáticamente"));
    SetWindowTextW(keep_open_,tr(L"Mantener abierto"));
    SetWindowTextW(progress_,tr(L"Progreso de la operación"));
    SendMessageW(search_,EM_SETCUEBANNER,TRUE,(LPARAM)tr(L"Buscar por dominio o IP"));
    for(const auto& item:button_labels) SetDlgItemTextW(window_,item.first,tr(item.second));
    const wchar_t* names[]={L"IP",tr(L"Dominio"),tr(L"Estado")};
    for(int i=0;i<3;++i) { LVCOLUMNW c{}; c.mask=LVCF_TEXT; c.pszText=const_cast<LPWSTR>(names[i]); ListView_SetColumn(list_,i,&c); }
    feedback_=Feedback::Idle; quick_result_=false; activity_text_=tr(L"Listo");
    SetWindowTextW(progress_text_,activity_text_.c_str());
    set_mode(advanced_); show_hosts_snapshot(); update_simple_view();
    InvalidateRect(progress_,nullptr,FALSE); InvalidateRect(list_,nullptr,FALSE);
    report(tr(L"Idioma: Español (Chile)"));
}
void Ui::update_simple_view() {
    if(!simple_count_) return;
    std::wstring count=tr(L"Precarga: ")+std::to_wstring(preload_entries_.size())+(preload_entries_.size()==1?tr(L" dominio"):tr(L" dominios"));
    if(preload_errors_) count+=L" · "+std::to_wstring(preload_errors_)+tr(L" errores de formato");
    SetWindowTextW(simple_count_,count.c_str());
    const wchar_t* hint=busy_||feedback_==Feedback::Failure||(quick_result_&&feedback_==Feedback::Success)?activity_text_.c_str():
        !hosts_available_?tr(L"No se pudo leer hosts. Abre el modo avanzado para ver el error."):
        preload_entries_.empty()?tr(L"No hay dominios precargados. Revisa hosts.txt en el modo avanzado."):
        preload_errors_?tr(L"Revisa los errores de la precarga en el modo avanzado antes de aplicarla."):
        snapshot_.state==HostsState::Patched?tr(L"El bloqueo de Josts está activo. Puedes actualizarlo o retirarlo."):
        tr(L"El bloqueo de Josts aún no está aplicado en este equipo.");
    SetWindowTextW(simple_hint_,hint);
    SendMessageW(simple_hint_,WM_SETFONT,(WPARAM)(close_at_?title_:regular_),TRUE);
    SetWindowTextW(simple_heading_,quick_result_&&feedback_==Feedback::Success?tr(L"¡Precarga aplicada correctamente!"):
        quick_result_&&feedback_==Feedback::Failure?tr(L"No se pudo completar la aplicación"):tr(L"Aplicar la lista precargada"));
    SetWindowTextW(simple_apply_,snapshot_.state==HostsState::Patched?tr(L"Actualizar lista precargada"):tr(L"Aplicar lista precargada"));
    const bool available=!busy_&&!close_at_&&hosts_available_&&!preload_entries_.empty()&&!preload_errors_;
    EnableWindow(simple_apply_,available); EnableWindow(quick_apply_,available);
    EnableWindow(simple_unpatch_,!busy_&&!close_at_&&hosts_available_&&snapshot_.state==HostsState::Patched);
    ShowWindow(keep_open_,!advanced_&&close_at_?SW_SHOW:SW_HIDE);
    SetWindowTextW(simple_description_,tr(L"Bloquea en este equipo los dominios incluidos en Josts. Se guardará un respaldo antes de hacer cambios."));
    SetWindowTextW(simple_footer_,tr(L"Un clic: Windows solicitará permisos si hacen falta. Tras aplicar y verificar la precarga, Josts se cierra en 3 segundos."));
    const bool show_actions=!advanced_&&(!welcome_||(welcome_state_!=WelcomeState::Checking&&welcome_state_!=WelcomeState::Current));
    for(HWND c:{quick_apply_,simple_apply_,simple_unpatch_}) ShowWindow(c,show_actions?SW_SHOW:SW_HIDE);
    if(welcome_) {
        SetWindowTextW(simple_heading_,tr(L"Bienvenido a Josts"));
        const wchar_t* state=welcome_state_==WelcomeState::Checking?tr(L"Estado: comprobando el equipo…"):
            welcome_state_==WelcomeState::PresetUnavailable?(snapshot_.state==HostsState::Patched?tr(L"Estado: Josts activo; precarga no verificable"):tr(L"Estado: sin parche de Josts")):
            welcome_state_==WelcomeState::Current?tr(L"Estado: parche al día"):
            welcome_state_==WelcomeState::NeedsUpdate?tr(L"Estado: Josts activo; lista diferente"):
            welcome_state_==WelcomeState::NeedsPatch?tr(L"Estado: sin parche de Josts"):tr(L"Estado: no se pudo verificar");
        SetWindowTextW(simple_count_,state);
        const wchar_t* description=welcome_state_==WelcomeState::Checking?tr(L"Estamos leyendo hosts y comparándolo con la precarga actual."):
            welcome_state_==WelcomeState::PresetUnavailable?tr(L"Se leyó el hosts global, pero la precarga está vacía, no se pudo leer o contiene errores. Revisa las reglas activas en modo avanzado."):
            welcome_state_==WelcomeState::Current?tr(L"La precarga actual ya está aplicada. No es necesario hacer ningún cambio en hosts."):
            welcome_state_==WelcomeState::NeedsUpdate?tr(L"El hosts del equipo tiene un bloque de Josts. Sus reglas no coinciden con la precarga de esta copia; puedes ver las reglas activas en modo avanzado."):
            welcome_state_==WelcomeState::NeedsPatch?tr(L"Este equipo no tiene el parche de Josts. Puedes aplicar la precarga desde aquí."):
            tr(L"No podemos confirmar el estado del parche. Josts permanecerá abierto para que puedas revisarlo.");
        SetWindowTextW(simple_description_,description);
        if(welcome_state_==WelcomeState::Current) {
            const std::wstring summary=tr(L"Precarga verificada: ")+std::to_wstring(preload_entries_.size())+tr(L" dominios. No se modificó hosts.");
            SetWindowTextW(simple_footer_,summary.c_str());
        }
        if(close_at_) update_countdown();
    }
}
bool Ui::preload_is_current() const {
    if(!hosts_available_||preload_errors_||preload_entries_.empty()||snapshot_.state!=HostsState::Patched) return false;
    std::set<std::wstring> wanted,domains;
    for(const auto& entry:preload_entries_) {
        const std::wstring key=entry.ip+L"\n"+entry.domain;
        if(!snapshot_.mappings.count(key)) return false;
        wanted.insert(key); domains.insert(entry.domain);
    }
    // Conflicting IPs or obsolete Josts entries still require review.
    for(const auto& key:snapshot_.mappings) {
        // The standard IPv6 localhost entry can coexist with the IPv4 entry
        // retained by the preload parser (one entry per domain).
        if(key==L"::1\nlocalhost"&&wanted.count(L"127.0.0.1\nlocalhost")) continue;
        const auto separator=key.find(L'\n');
        if(separator!=std::wstring::npos&&domains.count(key.substr(separator+1))&&!wanted.count(key)) return false;
    }
    for(const auto& domain:snapshot_.own) if(!domains.count(domain)) return false;
    return true;
}
void Ui::show_welcome_result(bool readable) {
    startup_checked_=true;
    if(!hosts_available_) welcome_state_=WelcomeState::Unavailable;
    else if(!readable||preload_errors_||preload_entries_.empty()) welcome_state_=WelcomeState::PresetUnavailable;
    else if(preload_is_current()) welcome_state_=WelcomeState::Current;
    else welcome_state_=snapshot_.state==HostsState::Patched?WelcomeState::NeedsUpdate:WelcomeState::NeedsPatch;
    if(welcome_state_==WelcomeState::Current) {
        no_changes_close_=true;
        activity_text_=tr(L"Parche al día. No es necesario hacer cambios en hosts.");
        SetWindowTextW(progress_text_,activity_text_.c_str()); report(activity_text_);
        begin_auto_close(5000);
    } else update_simple_view();
}
void Ui::report(const std::wstring& message) {
    log(message);
    if(console_) {
        int len=GetWindowTextLengthW(console_);
        if(len>100000) SetWindowTextW(console_,L"");
        SendMessageW(console_,EM_SETSEL,-1,-1);
        std::wstring line=message+L"\r\n";
        SendMessageW(console_,EM_REPLACESEL,FALSE,(LPARAM)line.c_str());
    }
}
void Ui::show_hosts_snapshot() {
    if(!hosts_available_) { SetWindowTextW(status_,tr(L"Estado de hosts: error de lectura")); update_simple_view(); return; }
    const wchar_t* label=snapshot_.state==HostsState::Patched?tr(L"Parcheado por Josts"):snapshot_.state==HostsState::ThirdParty?tr(L"Modificado por terceros"):tr(L"Original");
    std::wstring text=tr(L"Estado de hosts: "); text+=label;
    SetWindowTextW(status_,text.c_str());
    if(!filtered_.empty()) ListView_RedrawItems(list_,0,static_cast<int>(filtered_.size()-1));
    update_simple_view();
}
void Ui::merge_active_entries() {
    std::set<std::wstring> keys;
    for(const auto& e:entries_) keys.insert(e.ip+L"\n"+lower(e.domain));
    for(const auto& key:snapshot_.mappings) if(!keys.count(key)) {
        auto split=key.find(L'\n');
        entries_.push_back({key.substr(0,split),key.substr(split+1),false});
    }
    refresh_filter();
}
void Ui::refresh_system() {
    if(busy_) return;
    WIN32_FILE_ATTRIBUTE_DATA attr{};
    const bool readable=GetFileAttributesExW(hosts_.path().c_str(),GetFileExInfoStandard,&attr)!=FALSE;
    if(readable&&hosts_available_&&CompareFileTime(&attr.ftLastWriteTime,&hosts_write_time_)==0) return;
    if(close_at_) cancel_auto_close();
    HostsManager manager=hosts_;
    start_action(ACTION_REFRESH,tr(L"El archivo hosts cambió o no está disponible. Recargando su estado…"),[manager](ActionResult& result) mutable {
        result.snapshot_valid=manager.snapshot(result.snapshot,result.message);
        result.ok=result.snapshot_valid;
        if(result.ok) result.message=tr(L"Estado del hosts global actualizado. Se conservaron los cambios preparados en la lista.");
    });
}
void Ui::load_preload() {
    if(busy_||close_at_) return;
    const std::wstring path=join(data_dir_,L"hosts.txt");
    HINSTANCE instance=instance_; HostsManager manager=hosts_;
    start_action(ACTION_PRELOAD,tr(L"Leyendo la precarga y el estado de hosts…"),[path,instance,manager](ActionResult& result) mutable {
        std::wstring error;
        result.snapshot_valid=manager.snapshot(result.snapshot,error);
        if(!result.snapshot_valid) result.warning=error;
        std::string bytes; DWORD code=0;
        if(!exists(path)) {
            HRSRC resource=FindResourceW(instance,MAKEINTRESOURCEW(IDR_PRELOAD),RT_RCDATA);
            HGLOBAL block=resource?LoadResource(instance,resource):nullptr;
            const char* data=block?reinterpret_cast<const char*>(LockResource(block)):nullptr;
            DWORD length=resource?SizeofResource(instance,resource):0;
            if(!data||!length) { result.message=tr(L"No se pudo cargar la precarga incorporada."); return; }
            bytes.assign(data,length);
            if(!write_bytes(path,bytes,code)) result.warning+=tr(L"\r\nSe usa la precarga incorporada; no se pudo guardar hosts.txt: ")+error_message(code);
        } else if(!read_bytes(path,bytes,code)) {
            result.message=tr(L"No se pudo leer hosts.txt: ")+error_message(code); return;
        }
        TextFile file;
        if(!decode(bytes,file)) { result.message=tr(L"La codificación de hosts.txt no es válida."); return; }
        ParseResult parsed=parse_list(file.text);
        result.entries=std::move(parsed.entries); result.errors=parsed.errors;
        result.ok=parsed.errors==0&&result.snapshot_valid;
        result.message=tr(L"Precarga: ")+std::to_wstring(result.entries.size())+tr(L" dominios; ")+std::to_wstring(parsed.errors)+tr(L" errores de formato.");
        if(!result.snapshot_valid) result.message=error;
    });
}
void Ui::refresh_filter() {
    wchar_t query[512]={}; if(search_) GetWindowTextW(search_,query,512);
    std::wstring needle=lower(trim(query)); filtered_.clear(); filtered_.reserve(entries_.size());
    for(size_t i=0;i<entries_.size();++i) if(needle.empty()||lower(entries_[i].domain).find(needle)!=std::wstring::npos||lower(entries_[i].ip).find(needle)!=std::wstring::npos) filtered_.push_back(i);
    if(list_) { ListView_SetItemCountEx(list_,static_cast<int>(filtered_.size()),LVSICF_NOINVALIDATEALL); InvalidateRect(list_,nullptr,FALSE); }
}
std::wstring Ui::entry_status(const Entry& e) const {
    if(!hosts_available_) return tr(L"Estado no disponible");
    const auto key=e.ip+L"\n"+lower(e.domain);
    if(snapshot_.own_mappings.count(key)) return tr(L"Aplicada por Josts");
    if(snapshot_.foreign_mappings.count(key)) return tr(L"Activa fuera de Josts");
    if(e.modified) return tr(L"Modificada");
    return tr(L"No aplicada");
}
std::vector<size_t> Ui::selection() const {
    std::vector<size_t> result;
    for(int row=ListView_GetNextItem(list_,-1,LVNI_SELECTED);row>=0;row=ListView_GetNextItem(list_,row,LVNI_SELECTED)) if(static_cast<size_t>(row)<filtered_.size()) result.push_back(filtered_[row]);
    return result;
}
void Ui::add_or_edit(bool edit) {
    std::vector<size_t> picked=selection();
    if(edit&&picked.size()!=1) { notice(window_,tr(L"Seleccione una entrada para editar."),L"Josts",MB_OK|MB_ICONINFORMATION); return; }
    EntryDialog data; data.font=regular_; data.icons=icons_; data.value=edit?entries_[picked[0]]:Entry{L"0.0.0.0",L"",false};
    if(DialogBoxParamW(instance_,MAKEINTRESOURCEW(IDD_ENTRY),window_,entry_dialog,(LPARAM)&data)!=IDOK) return;
    for(size_t i=0;i<entries_.size();++i) if((!edit||i!=picked[0])&&entries_[i].domain==data.value.domain) { notice(window_,tr(L"El dominio ya está en la lista."),L"Josts",MB_OK|MB_ICONWARNING); return; }
    quick_result_=false;
    if(edit) { data.value.modified=true; entries_[picked[0]]=data.value; finish_action(true,tr(L"Entrada editada: ")+data.value.domain); }
    else { entries_.push_back(data.value); finish_action(true,tr(L"Entrada agregada: ")+data.value.domain); }
    refresh_filter();
}
void Ui::remove_selected() {
    std::vector<size_t> picked=selection(); if(picked.empty()) return;
    std::wstring question=tr(L"¿Eliminar ")+std::to_wstring(picked.size())+tr(L" entradas de la lista?");
    if(notice(window_,question.c_str(),L"Josts",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES) return;
    std::sort(picked.rbegin(),picked.rend()); for(size_t i:picked) entries_.erase(entries_.begin()+i);
    refresh_filter(); quick_result_=false; finish_action(true,tr(L"Entradas eliminadas de la lista."));
}
void Ui::move_selected(int direction) {
    std::vector<size_t> picked=selection(); if(picked.empty()) return;
    std::vector<bool> flags(entries_.size(),false); for(size_t i:picked) flags[i]=true;
    if(direction<0) for(size_t i=1;i<entries_.size();++i) if(flags[i]&&!flags[i-1]) { std::swap(entries_[i],entries_[i-1]); flags[i-1]=true; flags[i]=false; }
    if(direction>0) for(size_t i=entries_.size();i>1;--i) if(flags[i-2]&&!flags[i-1]) { std::swap(entries_[i-2],entries_[i-1]); flags[i-1]=true; flags[i-2]=false; }
    refresh_filter();
    for(size_t row=0;row<filtered_.size();++row) if(flags[filtered_[row]]) ListView_SetItemState(list_,static_cast<int>(row),LVIS_SELECTED,LVIS_SELECTED);
    quick_result_=false; finish_action(true,tr(L"Orden de la lista actualizado."));
}
static std::vector<std::wstring> open_paths(HWND parent,const std::wstring& initial) {
    std::vector<wchar_t> buffer(65536,0); OPENFILENAMEW ofn{};
    ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=parent; ofn.lpstrFile=buffer.data(); ofn.nMaxFile=static_cast<DWORD>(buffer.size());
    ofn.lpstrInitialDir=initial.c_str(); ofn.lpstrFilter=tr(L"Listas (*.txt;*.md)\0*.txt;*.md\0Todos los archivos\0*.*\0\0");
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_ALLOWMULTISELECT|OFN_EXPLORER;
    if(!GetOpenFileNameW(&ofn)) return {};
    std::vector<std::wstring> paths; std::wstring first=buffer.data(); const wchar_t* p=buffer.data()+first.size()+1;
    if(!*p) paths.push_back(first); else while(*p) { paths.push_back(join(first,p)); p+=wcslen(p)+1; }
    return paths;
}
void Ui::import_files() {
    if(busy_||close_at_) return;
    std::vector<std::wstring> paths=open_paths(window_,join(data_dir_,L"listas")); if(paths.empty()) return;
    start_action(ACTION_IMPORT,tr(L"Leyendo y procesando las listas…"),[paths](ActionResult& data) {
        std::set<std::wstring> seen;
        size_t readable=0;
        for(const std::wstring& path:paths) {
            std::string bytes; DWORD code=0; TextFile file;
            if(!read_bytes(path,bytes,code)||!decode(bytes,file)) { data.warning+=tr(L"No se pudo leer: ")+path+L"\r\n"; continue; }
            ++readable;
            ParseResult result=parse_list(file.text,seen);
            data.duplicates+=result.duplicates; data.errors+=result.errors;
            for(Entry& e:result.entries) { seen.insert(e.domain); data.entries.push_back(std::move(e)); }
        }
        data.ok=readable>0;
        data.message=data.ok?tr(L"Lectura de listas completada."):tr(L"No se pudo leer ninguna de las listas seleccionadas.");
    });
}
void Ui::export_file() {
    if(busy_||close_at_) return;
    wchar_t path[32768]=L"lista_josts.txt"; OPENFILENAMEW ofn{}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=window_;
    std::wstring initial=data_dir_;
    ofn.lpstrFile=path; ofn.nMaxFile=32768; ofn.lpstrInitialDir=initial.c_str();
    ofn.lpstrFilter=tr(L"Texto (*.txt)\0*.txt\0Markdown (*.md)\0*.md\0\0"); ofn.nFilterIndex=1;
    ofn.lpstrDefExt=L"txt"; ofn.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
    if(!GetSaveFileNameW(&ofn)) return;
    const std::wstring destination=path; const std::vector<Entry> entries=entries_;
    start_action(ACTION_EXPORT,tr(L"Guardando la lista exportada…"),[destination,entries](ActionResult& result) {
        std::wstring output=tr(L"# Lista exportada por Josts — xdCL\r\n");
        for(const Entry& e:entries) output+=e.ip+L" "+e.domain+L"\r\n";
        TextFile format; std::string bytes; DWORD code=0;
        result.ok=encode(format,output,bytes)&&write_bytes(destination,bytes,code);
        result.message=result.ok?tr(L"Lista exportada: ")+std::to_wstring(entries.size())+tr(L" entradas."):tr(L"No se pudo exportar: ")+error_message(code);
    });
}
void Ui::apply(bool selected_only,bool preload_only,bool close_after) {
    if(busy_||close_at_) return;
    if(preload_only&&(preload_entries_.empty()||preload_errors_)) {
        const wchar_t* error=tr(L"La precarga está vacía o contiene errores de formato. Revísala en el modo avanzado.");
        if(close_after) { quick_result_=true; finish_action(false,error); }
        else notice(window_,error,L"Josts",MB_OK|MB_ICONWARNING);
        return;
    }
    std::vector<size_t> picked=selected_only?selection():std::vector<size_t>{};
    if(selected_only&&picked.empty()) { notice(window_,tr(L"Seleccione entradas antes de aplicar la selección."),L"Josts",MB_OK|MB_ICONINFORMATION); return; }
    std::vector<Entry> chosen;
    if(preload_only) chosen=preload_entries_;
    else if(selected_only) for(size_t i:picked) chosen.push_back(entries_[i]); else chosen=entries_;
    std::wstring question=preload_only?
        tr(L"¿Aplicar la lista precargada de ")+std::to_wstring(chosen.size())+tr(L" dominios a este equipo?\r\nSe sustituirán las entradas anteriores de Josts; las demás se conservarán."):
        tr(L"¿Reemplazar el bloque de Josts en hosts con ")+std::to_wstring(chosen.size())+tr(L" entradas?");
    if(selected_only) question+=tr(L"\r\nSe quitarán del bloque las demás entradas de Josts.");
    if(!close_after&&notice(window_,question.c_str(),tr(L"Confirmar parche"),MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES) return;
    HWND target=window_; HostsManager manager=hosts_;
    const auto action=privileged_action_; const auto revision=snapshot_.revision;
    start_action(ACTION_APPLY,tr(L"Se requieren permisos de administrador para aplicar este cambio…"),[target,chosen,manager,action,revision](ActionResult& result) mutable {
        PrivilegedRequest request; request.operation=PrivilegedOperation::Apply; request.revision=revision; request.entries=chosen;
        for(const auto& entry:chosen) result.applied.insert(entry.ip+L"\n"+lower(entry.domain));
        auto outcome=action(target,request);
        result.ok=outcome.ok; result.message=outcome.message; result.warning=outcome.warning;
        PostMessageW(target,WM_ACTION_STAGE,STAGE_VERIFYING,0);
        std::wstring error;
        result.snapshot_valid=manager.snapshot(result.snapshot,error);
        if(!result.snapshot_valid) { result.ok=false; result.message+=L"\r\n"+error; }
        if(result.ok) for(const auto& key:result.applied) if(!result.snapshot.mappings.count(key)) {
            result.ok=false; result.message=tr(L"Se escribió el parche, pero la lectura posterior no coincide con la lista. Josts seguirá abierto para revisarlo."); break;
        }
        if(result.ok) result.message=tr(L"Lista aplicada y verificada en hosts: ")+std::to_wstring(result.applied.size())+(result.applied.size()==1?tr(L" dominio."):tr(L" dominios."));
    },close_after);
}
void Ui::remove_own() {
    if(busy_||close_at_) return;
    if(notice(window_,tr(L"¿Quitar únicamente el bloque de Josts del archivo hosts?"),tr(L"Confirmar desparche"),MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES) return;
    HWND target=window_; HostsManager manager=hosts_;
    const auto action=privileged_action_; const auto revision=snapshot_.revision;
    start_action(ACTION_UNPATCH,tr(L"Solicitando permisos para quitar el bloque de Josts…"),[target,manager,action,revision](ActionResult& result) mutable {
        PrivilegedRequest request; request.operation=PrivilegedOperation::Remove; request.revision=revision;
        auto outcome=action(target,request);
        result.ok=outcome.ok; result.message=outcome.message; result.warning=outcome.warning;
        std::wstring error; result.snapshot_valid=manager.snapshot(result.snapshot,error);
        if(!result.snapshot_valid) { result.ok=false; result.message+=L"\r\n"+error; }
        if(result.ok&&result.snapshot.state==HostsState::Patched) { result.ok=false; result.message=tr(L"No se pudo verificar la retirada del bloque: "); }
    });
}
void Ui::restore() {
    if(busy_||close_at_) return;
    if(notice(window_,tr(L"¿Restablecer hosts desde el respaldo original? Se perderán también los cambios realizados por terceros."),tr(L"Confirmar restablecimiento"),MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)!=IDYES) return;
    HWND target=window_; HostsManager manager=hosts_;
    const auto action=privileged_action_; const auto revision=snapshot_.revision;
    start_action(ACTION_RESTORE,tr(L"Solicitando permisos para restaurar el respaldo del equipo…"),[target,manager,action,revision](ActionResult& result) mutable {
        PrivilegedRequest request; request.operation=PrivilegedOperation::Restore; request.revision=revision;
        auto outcome=action(target,request);
        result.ok=outcome.ok; result.message=outcome.message; result.warning=outcome.warning;
        std::wstring error; result.snapshot_valid=manager.snapshot(result.snapshot,error);
        if(!result.snapshot_valid) { result.ok=false; result.message+=L"\r\n"+error; }
    });
}
void Ui::block_edge_news() {
    if(busy_||close_at_) return;
    auto action=privileged_action_; HWND target=window_;
    start_action(ACTION_EDGE_NEWS,tr(L"Desactivando las noticias de Edge…"),[action,target](ActionResult& result) {
        PrivilegedRequest request; request.operation=PrivilegedOperation::Edge;
        const auto outcome=action(target,request);
        result.ok=outcome.ok; result.message=outcome.message; result.warning=outcome.warning;
    });
}
void Ui::about() { AboutData data{regular_,icons_,instance_}; DialogBoxParamW(instance_,MAKEINTRESOURCEW(IDD_ABOUT),window_,about_dialog,(LPARAM)&data); }
int Ui::notice(HWND owner,const wchar_t* message,const wchar_t* title,UINT flags) { return dark_message(instance_,regular_,icons_,owner,message,title,flags); }
void Ui::set_busy(bool busy) {
    busy_=busy;
    const bool enabled=!busy&&!close_at_;
    for(HWND button:buttons_) EnableWindow(button,enabled);
    EnableWindow(edge_button_,enabled); EnableWindow(language_button_,enabled); EnableWindow(mode_button_,enabled); EnableWindow(about_button_,enabled);
    EnableWindow(list_,enabled); EnableWindow(search_,enabled);
    update_simple_view();
}
void Ui::start_action(int operation,const std::wstring& label,std::function<void(ActionResult&)> work,bool quick) {
    if(busy_||close_at_) return;
    if(worker_.joinable()) worker_.join();
    if(startup_checked_) welcome_=false;
    quick_result_=quick;
    activity_text_=label; feedback_=Feedback::Working; animation_frame_=0;
    SetWindowTextW(progress_text_,activity_text_.c_str());
    set_busy(true); report(label);
    SetTimer(window_,TIMER_PROGRESS,40,nullptr);
    InvalidateRect(progress_,nullptr,FALSE); UpdateWindow(progress_); UpdateWindow(progress_text_);
    HWND target=window_;
    try {
        std::unique_ptr<ActionResult> result(new ActionResult);
        result->operation=operation; result->quick=quick;
        worker_=std::thread([target,work,result=std::move(result)]() mutable {
            try { work(*result); }
            catch(...) { result->ok=false; result->message=tr(L"No se pudo completar la operación. Josts seguirá abierto para revisarlo."); }
            if(PostMessageW(target,WM_ACTION_DONE,0,(LPARAM)result.get())) result.release();
        });
    } catch(...) { finish_action(false,tr(L"No se pudo iniciar la operación. Vuelve a intentarlo.")); }
}
void Ui::finish_action(bool ok,const std::wstring& message) {
    KillTimer(window_,TIMER_PROGRESS);
    feedback_=ok?Feedback::Success:Feedback::Failure;
    if(!ok&&!startup_checked_) welcome_state_=WelcomeState::Unavailable;
    activity_text_=message;
    SetWindowTextW(progress_text_,message.c_str());
    set_busy(false); report(message);
    InvalidateRect(progress_,nullptr,FALSE);
}
void Ui::update_countdown() {
    if(!close_at_) return;
    const ULONGLONG now=GetTickCount64();
    if(now>=close_at_) {
        KillTimer(window_,TIMER_CLOSE); close_at_=0;
        PostMessageW(window_,WM_CLOSE,0,0); return;
    }
    const unsigned seconds=static_cast<unsigned>((close_at_-now+999)/1000);
    const std::wstring text=tr(L"Josts se cerrará en ")+std::to_wstring(seconds)+(seconds==1?tr(L" segundo."):tr(L" segundos."));
    SetWindowTextW(simple_hint_,text.c_str());
    SetWindowTextW(progress_text_,(activity_text_+L"  "+text).c_str());
}
void Ui::begin_auto_close(unsigned milliseconds) {
    close_at_=GetTickCount64()+milliseconds;
    set_busy(false); SetFocus(keep_open_);
    // Give the user the entire interval after the controls and focus are ready.
    close_at_=GetTickCount64()+milliseconds;
    if(SetTimer(window_,TIMER_CLOSE,100,nullptr)) update_countdown();
    else cancel_auto_close();
}
void Ui::cancel_auto_close() {
    KillTimer(window_,TIMER_CLOSE); close_at_=0;
    activity_text_=no_changes_close_?tr(L"No se realizaron cambios en hosts. Josts permanecerá abierto."):tr(L"Precarga aplicada y verificada. Josts permanecerá abierto.");
    no_changes_close_=false; welcome_=false;
    SetWindowTextW(progress_text_,activity_text_.c_str());
    set_busy(false); SetFocus(mode_button_);
}
void Ui::paint_background(HDC dc) {
    RECT r{}; GetClientRect(window_,&r); FillRect(dc,&r,theme::background_brush());
    const int width=static_cast<int>(r.right),height=static_cast<int>(r.bottom);
    if(width<=0||height<=0) return;
    if(!advanced_) {
        const int card_w=std::min(720,width-48),card_h=450;
        const int x=(width-card_w)/2,y=std::max(112,(height-64-card_h)/2);
        RECT card{x,y,x+card_w,y+card_h}; FillRect(dc,&card,theme::panel_brush());
        HBRUSH edge=CreateSolidBrush(theme::border()); FrameRect(dc,&card,edge); DeleteObject(edge);
        HBRUSH accent=CreateSolidBrush(theme::accent());
        RECT stripe{x,y,x+card_w,y+3}; FillRect(dc,&stripe,accent); DeleteObject(accent);
        return;
    }
    const int right=std::max(720,width-270),log_h=std::max(70,std::min(140,height-570));
    HBRUSH edge=CreateSolidBrush(theme::border());
    RECT search_frame{16,109,right-16,139}; FrameRect(dc,&search_frame,edge);
    RECT list_frame{16,151,right-16,r.bottom-log_h-84}; FrameRect(dc,&list_frame,edge);
    RECT log_frame{16,r.bottom-log_h-70,r.right-16,r.bottom-70}; FrameRect(dc,&log_frame,edge);
    DeleteObject(edge);
}
LRESULT Ui::handle(UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_CREATE) { create_controls(); return 0; }
    if(msg==WM_SIZE) { layout(); return 0; }
    if(msg==WM_GETMINMAXINFO) { auto* info=reinterpret_cast<MINMAXINFO*>(l); info->ptMinTrackSize.x=1020; info->ptMinTrackSize.y=680; return 0; }
    if(msg==WM_ERASEBKGND) { RECT r{}; GetClientRect(window_,&r); FillRect((HDC)w,&r,theme::background_brush()); return 1; }
    if(msg==WM_PRINTCLIENT) { paint_background((HDC)w); return 0; }
    if(msg==WM_PAINT) {
        PAINTSTRUCT ps{}; HDC dc=BeginPaint(window_,&ps);
        paint_background(dc); EndPaint(window_,&ps); return 0;
    }
    if(msg==WM_CTLCOLOREDIT||msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLORDLG||msg==WM_CTLCOLORBTN) {
        HDC dc=(HDC)w; HWND child=(HWND)l;
        if(msg==WM_CTLCOLORSTATIC&&(child==simple_heading_||child==simple_description_||child==simple_count_||child==simple_hint_||child==simple_footer_)) {
            SetTextColor(dc,child==simple_hint_&&close_at_?RGB(121,216,163):child==simple_count_?theme::accent():child==simple_heading_?theme::text():theme::muted());
            SetBkColor(dc,theme::panel());
            return reinterpret_cast<LRESULT>(theme::panel_brush());
        }
        if(msg==WM_CTLCOLORSTATIC&&(child==brand_||child==status_||child==admin_||child==tagline_)) {
            COLORREF color=child==brand_?theme::accent():child==status_?theme::text():theme::muted();
            SetTextColor(dc,color); SetBkColor(dc,theme::background());
            return reinterpret_cast<LRESULT>(theme::background_brush());
        }
        return theme::color_control(msg,dc,child);
    }
    if(msg==WM_DRAWITEM) {
        DRAWITEMSTRUCT* item=reinterpret_cast<DRAWITEMSTRUCT*>(l);
        if(item->CtlType==ODT_STATIC&&item->CtlID==ID_PROGRESS) {
            FillRect(item->hDC,&item->rcItem,theme::surface_brush());
            if(feedback_!=Feedback::Idle) {
                RECT fill=item->rcItem;
                const int width=fill.right-fill.left;
                if(feedback_==Feedback::Working) {
                    const int segment=std::max(24,width/4);
                    const int position=static_cast<int>((animation_frame_*9)%(width+segment))-segment/2;
                    fill.left=std::max(0,position); fill.right=std::min(width,position+segment);
                }
                HBRUSH brush=CreateSolidBrush(feedback_==Feedback::Success?RGB(121,216,163):feedback_==Feedback::Failure?RGB(244,137,137):theme::accent());
                if(fill.right>fill.left) FillRect(item->hDC,&fill,brush);
                DeleteObject(brush);
            }
            return TRUE;
        }
        if(item->CtlType==ODT_BUTTON) {
            int id=GetDlgCtrlID(item->hwndItem);
            theme::paint_button(item,regular_,icons_,material_glyph(id),id==ID_APPLY||id==ID_QUICK_APPLY); return TRUE;
        }
        if(item->CtlType==ODT_STATIC&&item->CtlID==ID_HEADER) {
            FillRect(item->hDC,&item->rcItem,theme::surface_brush());
            SetBkMode(item->hDC,TRANSPARENT); SetTextColor(item->hDC,theme::muted());
            HGDIOBJ previous=SelectObject(item->hDC,bold_);
            const wchar_t* names[]={L"IP",tr(L"DOMINIO"),tr(L"ESTADO")};
            int x=0;
            for(int col=0;col<3;++col) {
                RECT cell=item->rcItem; cell.left=x+10; x+=ListView_GetColumnWidth(list_,col); cell.right=x-7;
                DrawTextW(item->hDC,names[col],-1,&cell,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            }
            SelectObject(item->hDC,previous);
            HBRUSH edge=CreateSolidBrush(theme::border());
            RECT line=item->rcItem; line.top=line.bottom-1; FillRect(item->hDC,&line,edge); DeleteObject(edge);
            return TRUE;
        }
        return FALSE;
    }
    if(msg==WM_ACTIVATEAPP&&w&&startup_checked_) { refresh_system(); return 0; }
    if(msg==WM_ACTION_STAGE) {
        if(busy_) {
            activity_text_=w==STAGE_WRITING?tr(L"Guardando el respaldo y aplicando la lista…"):
                w==STAGE_VERIFYING?tr(L"Verificando el resultado guardado en hosts…"):tr(L"Leyendo hosts…");
            SetWindowTextW(progress_text_,activity_text_.c_str()); update_simple_view();
        }
        return 0;
    }
    if(msg==WM_TIMER&&w==TIMER_HOSTS) { if(startup_checked_) refresh_system(); return 0; }
    if(msg==WM_TIMER) {
        if(w==TIMER_PROGRESS&&busy_) { ++animation_frame_; InvalidateRect(progress_,nullptr,FALSE); return 0; }
        if(w==TIMER_CLOSE) { update_countdown(); return 0; }
    }
    if(msg==WM_ACTION_DONE) {
        std::unique_ptr<ActionResult> result(reinterpret_cast<ActionResult*>(l));
        if(worker_.joinable()) worker_.join();
        if(result->operation==ACTION_PRELOAD) {
            preload_entries_=result->entries; preload_errors_=result->errors;
            entries_=std::move(result->entries); refresh_filter();
        }
        if(result->operation<=ACTION_RESTORE||result->operation==ACTION_PRELOAD||result->operation==ACTION_REFRESH) {
            if(result->snapshot_valid) { snapshot_=std::move(result->snapshot); hosts_available_=true; }
            else hosts_available_=false;
            if(result->snapshot_valid) {
                hosts_write_time_=snapshot_.write_time;
                merge_active_entries();
            }
            if(result->ok&&result->operation==ACTION_APPLY) for(Entry& e:entries_) if(result->applied.count(e.ip+L"\n"+e.domain)) e.modified=false;
            show_hosts_snapshot();
        }
        finish_action(result->ok,result->message);
        if(!result->warning.empty()) report(result->warning);
        if(result->operation==ACTION_PRELOAD&&!startup_checked_) show_welcome_result(result->ok);
        if(result->ok&&result->quick&&result->operation==ACTION_APPLY) {
            begin_auto_close(3000);
        } else if(result->ok&&result->operation==ACTION_IMPORT) {
            std::set<std::wstring> old; for(const Entry& e:entries_) old.insert(e.domain);
            size_t existing=0; for(const Entry& e:result->entries) if(old.count(e.domain)) ++existing;
            std::wstring summary=tr(L"Nuevas: ")+std::to_wstring(result->entries.size()-existing)+tr(L"\r\nDuplicadas ignoradas: ")+std::to_wstring(result->duplicates+existing)+tr(L"\r\nErrores: ")+std::to_wstring(result->errors);
            if(!result->warning.empty()) summary+=L"\r\n"+result->warning;
            summary+=tr(L"\r\nSí = añadir nuevas; No = reemplazar lista; Cancelar = no cambiar.");
            const int answer=notice(window_,summary.c_str(),tr(L"Resultado de la carga"),MB_YESNOCANCEL|MB_ICONQUESTION);
            if(answer==IDYES) {
                for(Entry& e:result->entries) if(!old.count(e.domain)) entries_.push_back(std::move(e));
                refresh_filter(); finish_action(true,tr(L"Se añadieron ")+std::to_wstring(result->entries.size()-existing)+tr(L" entradas."));
            } else if(answer==IDNO) {
                entries_=std::move(result->entries); refresh_filter();
                finish_action(true,tr(L"Lista reemplazada con ")+std::to_wstring(entries_.size())+tr(L" entradas."));
            } else finish_action(true,tr(L"Carga cancelada. La lista se conservó."));
        } else if(result->operation==ACTION_EDGE_NEWS) {
            SetWindowTextW(simple_hint_,result->message.c_str());
        } else if(!result->ok&&!result->quick&&result->operation!=ACTION_PRELOAD&&result->operation!=ACTION_REFRESH) {
            notice(window_,result->message.c_str(),L"Josts",MB_OK|MB_ICONERROR);
        }
        return 0;
    }
    if(msg==WM_NOTIFY) {
        NMHDR* n=reinterpret_cast<NMHDR*>(l);
        if(n->hwndFrom==list_&&n->code==LVN_GETDISPINFOW) {
            NMLVDISPINFOW* d=reinterpret_cast<NMLVDISPINFOW*>(l);
            if(d->item.iItem>=0&&static_cast<size_t>(d->item.iItem)<filtered_.size()&&(d->item.mask&LVIF_TEXT)) {
                const Entry& e=entries_[filtered_[d->item.iItem]];
                std::wstring s=d->item.iSubItem==0?e.ip:d->item.iSubItem==1?e.domain:entry_status(e);
                if(d->item.pszText&&d->item.cchTextMax) lstrcpynW(d->item.pszText,s.c_str(),d->item.cchTextMax);
            } return 0;
        }
        if(n->hwndFrom==list_&&n->code==NM_DBLCLK) { if(!busy_&&!close_at_) add_or_edit(true); return 0; }
        if(n->hwndFrom==list_&&n->code==LVN_KEYDOWN) { if(busy_||close_at_) return 0; NMLVKEYDOWN* k=reinterpret_cast<NMLVKEYDOWN*>(l); if(k->wVKey==VK_DELETE) remove_selected(); else if(k->wVKey==VK_F2) add_or_edit(true); return 0; }
        if(n->hwndFrom==list_&&n->code==NM_CUSTOMDRAW) {
            NMLVCUSTOMDRAW* d=reinterpret_cast<NMLVCUSTOMDRAW*>(l);
            if(d->nmcd.dwDrawStage==CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if(d->nmcd.dwDrawStage==CDDS_ITEMPREPAINT&&d->nmcd.dwItemSpec<filtered_.size()) {
                int row=static_cast<int>(d->nmcd.dwItemSpec);
                const Entry& e=entries_[filtered_[row]];
                const bool selected=(ListView_GetItemState(list_,row,LVIS_SELECTED)&LVIS_SELECTED)!=0;
                RECT row_rect{}; ListView_GetItemRect(list_,row,&row_rect,LVIR_BOUNDS);
                RECT client{}; GetClientRect(list_,&client); row_rect.right=client.right;
                HBRUSH fill=CreateSolidBrush(selected?theme::selection():row%2?theme::surface():theme::panel());
                FillRect(d->nmcd.hdc,&row_rect,fill); DeleteObject(fill);
                SetBkMode(d->nmcd.hdc,TRANSPARENT);
                HGDIOBJ previous=SelectObject(d->nmcd.hdc,regular_);
                const std::wstring values[]={e.ip,e.domain,entry_status(e)};
                int x=0;
                for(int col=0;col<3;++col) {
                    RECT cell=row_rect; cell.left=x+10; x+=ListView_GetColumnWidth(list_,col); cell.right=x-7;
                    COLORREF color=theme::text();
                    if(col==2) color=!hosts_available_?theme::muted():snapshot_.own_mappings.count(e.ip+L"\n"+lower(e.domain))?RGB(113,191,242):
                        snapshot_.foreign_mappings.count(e.ip+L"\n"+lower(e.domain))?RGB(244,192,107):e.modified?theme::muted():RGB(121,216,163);
                    SetTextColor(d->nmcd.hdc,color);
                    DrawTextW(d->nmcd.hdc,values[col].c_str(),-1,&cell,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                }
                SelectObject(d->nmcd.hdc,previous);
                if(ListView_GetItemState(list_,row,LVIS_FOCUSED)&LVIS_FOCUSED) {
                    RECT focus=row_rect; InflateRect(&focus,-2,-1); DrawFocusRect(d->nmcd.hdc,&focus);
                }
                return CDRF_SKIPDEFAULT;
            } return CDRF_DODEFAULT;
        }
    }
    if(msg==WM_COMMAND) {
        int id=LOWORD(w);
        if(id==ID_KEEP_OPEN&&close_at_) { cancel_auto_close(); return 0; }
        if(busy_||close_at_) return 0;
        if(id==ID_SEARCH&&HIWORD(w)==EN_CHANGE) { refresh_filter(); return 0; }
        switch(id) {
            case ID_APPLY: apply(false); return 0; case ID_APPLY_SELECTED: apply(true); return 0;
            case ID_SIMPLE_APPLY: apply(false,true); return 0; case ID_SIMPLE_UNPATCH: remove_own(); return 0;
            case ID_QUICK_APPLY: apply(false,true,true); return 0;
            case ID_RESTORE: restore(); return 0; case ID_UNPATCH: remove_own(); return 0;
            case ID_IMPORT: import_files(); return 0; case ID_ADD: add_or_edit(false); return 0;
            case ID_RELOAD: load_preload(); return 0; case ID_EXPORT: export_file(); return 0;
            case ID_EDIT: add_or_edit(true); return 0; case ID_DELETE: remove_selected(); return 0;
            case ID_UP: move_selected(-1); return 0; case ID_DOWN: move_selected(1); return 0;
            case ID_EDGE_NEWS: block_edge_news(); return 0;
            case ID_LANGUAGE: toggle_language(); return 0;
            case ID_ABOUT: about(); return 0; case ID_MODE: if(!busy_) { welcome_=false; set_mode(!advanced_); } return 0;
        }
    }
    if(msg==WM_CLOSE&&busy_) { report(tr(L"Espera a que termine la operación antes de cerrar Josts.")); return 0; }
    if(msg==WM_DESTROY) { KillTimer(window_,TIMER_PROGRESS); KillTimer(window_,TIMER_CLOSE); DeleteObject(regular_); DeleteObject(bold_); DeleteObject(title_); if(icons_) DeleteObject(icons_); PostQuitMessage(0); return 0; }
    return DefWindowProcW(window_,msg,w,l);
}
}
