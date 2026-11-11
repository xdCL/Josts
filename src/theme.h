#pragma once
#include <windows.h>

// Paleta oscura única de Josts — desarrollado por xdCL.
namespace josts { namespace theme {
inline COLORREF background() { return RGB(15,20,26); }
inline COLORREF panel() { return RGB(24,31,40); }
inline COLORREF surface() { return RGB(34,43,54); }
inline COLORREF border() { return RGB(55,69,83); }
inline COLORREF text() { return RGB(232,239,246); }
inline COLORREF muted() { return RGB(156,172,187); }
inline COLORREF accent() { return RGB(91,181,235); }
inline COLORREF selection() { return RGB(37,80,109); }
inline HBRUSH background_brush() { static HBRUSH b=CreateSolidBrush(background()); return b; }
inline HBRUSH panel_brush() { static HBRUSH b=CreateSolidBrush(panel()); return b; }
inline HBRUSH surface_brush() { static HBRUSH b=CreateSolidBrush(surface()); return b; }

inline LRESULT color_control(UINT message,HDC dc,HWND child) {
    if(message==WM_CTLCOLOREDIT) {
        SetTextColor(dc,text()); SetBkColor(dc,surface());
        return reinterpret_cast<LRESULT>(surface_brush());
    }
    if(message==WM_CTLCOLORSTATIC) {
        wchar_t cls[32]={}; GetClassNameW(child,cls,32);
        const bool edit=lstrcmpiW(cls,L"Edit")==0;
        SetTextColor(dc,edit?text():muted());
        SetBkColor(dc,edit?panel():background());
        return reinterpret_cast<LRESULT>(edit?panel_brush():background_brush());
    }
    SetTextColor(dc,text()); SetBkColor(dc,background());
    return reinterpret_cast<LRESULT>(background_brush());
}

inline void paint_button(const DRAWITEMSTRUCT* item,HFONT label_font,HFONT icon_font,wchar_t icon,bool primary) {
    const bool pressed=(item->itemState&ODS_SELECTED)!=0;
    const bool disabled=(item->itemState&ODS_DISABLED)!=0;
    COLORREF base=primary?(pressed?RGB(38,126,181):RGB(40,142,202)):
        pressed?RGB(46,61,76):surface();
    COLORREF foreground=disabled?RGB(105,119,133):text();
    HBRUSH fill=CreateSolidBrush(base);
    FillRect(item->hDC,&item->rcItem,fill); DeleteObject(fill);
    HBRUSH edge=CreateSolidBrush(primary?RGB(70,167,222):border());
    FrameRect(item->hDC,&item->rcItem,edge); DeleteObject(edge);
    SetBkMode(item->hDC,TRANSPARENT);
    SetTextColor(item->hDC,foreground);
    RECT label=item->rcItem;
    if(icon_font&&icon) {
        RECT box=item->rcItem; box.left+=10; box.right=box.left+25;
        HGDIOBJ previous=SelectObject(item->hDC,icon_font);
        DrawTextW(item->hDC,&icon,1,&box,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        SelectObject(item->hDC,previous);
        label.left+=43;
    } else label.left+=12;
    label.right-=8;
    wchar_t caption[128]={}; GetWindowTextW(item->hwndItem,caption,128);
    HGDIOBJ previous=SelectObject(item->hDC,label_font);
    DrawTextW(item->hDC,caption,-1,&label,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
    SelectObject(item->hDC,previous);
    if(item->itemState&ODS_FOCUS) {
        RECT focus=item->rcItem; InflateRect(&focus,-3,-3); DrawFocusRect(item->hDC,&focus);
    }
}
}}
