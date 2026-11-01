#include "edge_settings.h"
#include <windows.h>

namespace josts {
bool BloquearContenidoNoticiasEdge() noexcept {
    constexpr wchar_t ruta[]=L"SOFTWARE\\Policies\\Microsoft\\Edge";
    constexpr wchar_t nombre[]=L"NewTabPageContentEnabled";
    HKEY clave=nullptr;
    // Vista nativa de 64 bits desde un EXE x86; Windows x86 ignora esta bandera.
    LSTATUS estado=RegCreateKeyExW(HKEY_LOCAL_MACHINE,ruta,0,nullptr,
        REG_OPTION_NON_VOLATILE,KEY_SET_VALUE|KEY_QUERY_VALUE|KEY_WOW64_64KEY,
        nullptr,&clave,nullptr);
    if(estado!=ERROR_SUCCESS) {
        SetLastError(static_cast<DWORD>(estado));
        return false;
    }

    const DWORD deshabilitado=0;
    estado=RegSetValueExW(clave,nombre,0,REG_DWORD,
        reinterpret_cast<const BYTE*>(&deshabilitado),sizeof(deshabilitado));
    if(estado==ERROR_SUCCESS) {
        DWORD tipo=0,valor=1,tamano=sizeof(valor);
        estado=RegQueryValueExW(clave,nombre,nullptr,&tipo,
            reinterpret_cast<BYTE*>(&valor),&tamano);
        if(estado==ERROR_SUCCESS&&(tipo!=REG_DWORD||tamano!=sizeof(valor)||valor!=0))
            estado=ERROR_INVALID_DATA;
    }
    const LSTATUS cierre=RegCloseKey(clave);
    if(estado==ERROR_SUCCESS) estado=cierre;
    // RegCloseKey no debe ocultar un error anterior de escritura o verificación.
    SetLastError(static_cast<DWORD>(estado));
    return estado==ERROR_SUCCESS;
}
}
