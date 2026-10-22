#include "font_loader.h"
#include "common.h"
#include "resource.h"
namespace josts {
bool Fonts::load(HINSTANCE instance) {
    const int ids[]={IDR_FONT_DMSANS_REGULAR,IDR_FONT_DMSANS_MEDIUM,IDR_FONT_DMSANS_BOLD,IDR_FONT_DMSANS_ITALIC};
    bool all=true;
    for(int id:ids) {
        HRSRC res=FindResourceW(instance,MAKEINTRESOURCEW(id),RT_RCDATA);
        if(!res) { all=false; log(tr(L"No se encontró un recurso DM Sans: ")+std::to_wstring(id)); continue; }
        HGLOBAL block=LoadResource(instance,res);
        void* bytes=block?LockResource(block):nullptr;
        DWORD size=SizeofResource(instance,res), count=0;
        HANDLE handle=bytes&&size?AddFontMemResourceEx(bytes,size,nullptr,&count):nullptr;
        if(!handle||!count) { all=false; log(tr(L"No se pudo cargar DM Sans desde memoria: ")+std::to_wstring(id)); }
        else handles_.push_back(handle);
    }
    loaded_=all;
    if(!loaded_) log(tr(L"Se usa Segoe UI como tipografía alternativa."));
    HRSRC icon_res=FindResourceW(instance,MAKEINTRESOURCEW(IDR_FONT_MATERIAL_ICONS),RT_RCDATA);
    if(icon_res) {
        HGLOBAL block=LoadResource(instance,icon_res);
        void* bytes=block?LockResource(block):nullptr;
        DWORD size=SizeofResource(instance,icon_res),count=0;
        HANDLE handle=bytes&&size?AddFontMemResourceEx(bytes,size,nullptr,&count):nullptr;
        if(handle&&count) { handles_.push_back(handle); icons_loaded_=true; }
    }
    if(!icons_loaded_) log(tr(L"No se pudo cargar Material Icons; los botones conservan sus textos."));
    return loaded_;
}
HFONT Fonts::create(int points,int weight,bool italic) const {
    HDC dc=GetDC(nullptr);
    int dpi=dc?GetDeviceCaps(dc,LOGPIXELSY):96;
    if(dc) ReleaseDC(nullptr,dc);
    LOGFONTW lf{};
    lf.lfHeight=-MulDiv(points,dpi,72);
    lf.lfWeight=weight;
    lf.lfItalic=italic?TRUE:FALSE;
    lf.lfCharSet=DEFAULT_CHARSET;
    lstrcpynW(lf.lfFaceName,loaded_?L"DM Sans":L"Segoe UI",LF_FACESIZE);
    return CreateFontIndirectW(&lf);
}
HFONT Fonts::create_icons(int points) const {
    if(!icons_loaded_) return nullptr;
    HDC dc=GetDC(nullptr);
    int dpi=dc?GetDeviceCaps(dc,LOGPIXELSY):96;
    if(dc) ReleaseDC(nullptr,dc);
    LOGFONTW lf{};
    lf.lfHeight=-MulDiv(points,dpi,72);
    lf.lfWeight=FW_NORMAL;
    lf.lfCharSet=DEFAULT_CHARSET;
    lstrcpynW(lf.lfFaceName,L"Material Icons",LF_FACESIZE);
    return CreateFontIndirectW(&lf);
}
Fonts::~Fonts() { for(HANDLE h:handles_) RemoveFontMemResourceEx(h); }
}
