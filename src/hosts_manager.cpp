#include "hosts_manager.h"
#include "parser.h"
#include <algorithm>
#include <sstream>
#include <map>

namespace josts {
static const wchar_t* START=L"# ===== INICIO Josts =====";
static const wchar_t* END=L"# ===== FIN Josts =====";
static const wchar_t* CREDIT=L"# Generado por Josts — crédito: xdCL";
namespace {
struct FileLock {
    HANDLE handle=INVALID_HANDLE_VALUE;
    explicit FileLock(const std::wstring& path) {
        if(path.empty()) { SetLastError(ERROR_PATH_NOT_FOUND); return; }
        handle=CreateFileW((path+L".josts.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,
                           FILE_ATTRIBUTE_HIDDEN|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(handle!=INVALID_HANDLE_VALUE) {
            BY_HANDLE_FILE_INFORMATION info{};
            if(!GetFileInformationByHandle(handle,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) {
                CloseHandle(handle); handle=INVALID_HANDLE_VALUE;
            }
        }
    }
    ~FileLock() { if(handle!=INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
bool unchanged(const std::string& bytes,const std::string& revision,std::wstring& error) {
    if(revision.empty()||sha256(bytes)==revision) return true;
    error=tr(L"El archivo hosts fue modificado externamente. Se recargará su estado; revisa los cambios antes de volver a aplicar."); return false;
}
bool regular_file(const std::wstring& path) {
    DWORD attr=GetFileAttributesW(path.c_str());
    return attr!=INVALID_FILE_ATTRIBUTES&&!(attr&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
}
bool blocking_ip(const std::wstring& ip) {
    return ip==L"0.0.0.0"||ip==L"127.0.0.1"||ip==L"::1";
}
// Never truncate a predictable temporary file or follow a pre-existing link.
bool stage_file(const std::wstring& destination,const std::string& bytes,std::wstring& temp,DWORD& code) {
    temp=destination+L"."+random_id()+L".tmp";
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) { code=GetLastError(); return false; }
    DWORD written=0;
    bool ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file);
    code=ok?ERROR_SUCCESS:GetLastError(); CloseHandle(file);
    if(!ok) { if(!code) code=ERROR_WRITE_FAULT; DeleteFileW(temp.c_str()); }
    return ok;
}
}
HostsManager::HostsManager(const std::wstring& custom_path,const std::wstring& custom_portable_backup) {
    if(!custom_path.empty()) path_=custom_path;
    else {
        wchar_t path[MAX_PATH]={};
        UINT n=GetSystemDirectoryW(path,MAX_PATH);
        if(n&&n<MAX_PATH) {
            // System32\drivers\etc is exempt from WOW64 file-system redirection.
            // Keep the canonical path visible to users instead of the virtual Sysnative alias.
            path_=join(std::wstring(path,n),L"drivers\\etc\\hosts");
        }
    }
    system_backup_=path_+L".bak_original";
    // Kept as a source-compatible argument for old callers; never trust portable backups for restoration.
    (void)custom_portable_backup;
}
static bool block_span(const std::wstring& s,size_t& begin,size_t& end,std::wstring& error) {
    bool inside=false,found=false;
    size_t pos=0;
    while(pos<s.size()) {
        size_t line_end=s.find(L'\n',pos);
        size_t next=line_end==std::wstring::npos?s.size():line_end+1;
        std::wstring line=trim(s.substr(pos,(line_end==std::wstring::npos?s.size():line_end)-pos));
        if(line==START) {
            if(inside||found) { error=tr(L"El hosts contiene más de un inicio de bloque Josts."); return false; }
            inside=true; found=true; begin=pos;
        } else if(line==END) {
            if(!inside) { error=tr(L"El hosts tiene un fin de bloque Josts sin inicio."); return false; }
            inside=false; end=next;
        }
        pos=next;
    }
    if(inside) { error=tr(L"El bloque Josts está incompleto; no se modificó hosts."); return false; }
    if(!found) begin=end=std::wstring::npos;
    return true;
}
bool HostsManager::read_current(std::string& bytes,TextFile& text,std::wstring& error) {
    if(path_.empty()) { error=tr(L"No se pudo resolver la carpeta de sistema."); return false; }
    DWORD code=0;
    if(!regular_file(path_)) { error=tr(L"El hosts no existe, no es un archivo normal o es un enlace. No se modificó el sistema."); return false; }
    if(!read_bytes(path_,bytes,code)) { error=tr(L"No se pudo leer hosts: ")+error_message(code); return false; }
    if(!decode(bytes,text)) { error=tr(L"Codificación del hosts no válida; no se modificó."); return false; }
    return true;
}
bool HostsManager::ensure_backups(const std::string& original,std::wstring& error) {
    DWORD code=0;
    // The original belongs to this computer. Never restore an untrusted portable copy.
    if(exists(system_backup_)) {
        std::string bytes; TextFile decoded;
        if(!regular_file(system_backup_)||!read_bytes(system_backup_,bytes,code)||!decode(bytes,decoded)) {
            error=tr(L"El respaldo original del sistema no es válido. No se modificó hosts."); return false;
        }
    } else {
        std::wstring temp;
        if(!stage_file(system_backup_,original,temp,code)) {
            error=tr(L"No se pudo crear el respaldo del sistema: ")+error_message(code); return false;
        }
        if(!MoveFileExW(temp.c_str(),system_backup_.c_str(),MOVEFILE_WRITE_THROUGH)) {
            code=GetLastError(); DeleteFileW(temp.c_str());
            error=tr(L"No se pudo guardar el respaldo del sistema: ")+error_message(code); return false;
        }
    }
    const std::wstring previous=path_+L".bak_previous";
    if(exists(previous)&&!regular_file(previous)) { error=tr(L"El respaldo anterior no es un archivo normal."); return false; }
    std::wstring temp;
    if(!stage_file(previous,original,temp,code)) { error=tr(L"No se pudo respaldar el estado anterior: ")+error_message(code); return false; }
    if(!MoveFileExW(temp.c_str(),previous.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        code=GetLastError(); DeleteFileW(temp.c_str()); error=tr(L"No se pudo respaldar el estado anterior: ")+error_message(code); return false;
    }
    std::string verified;
    if(!read_bytes(previous,verified,code)||verified!=original) { error=tr(L"No se pudo verificar el respaldo anterior. No se modificó hosts."); return false; }
    return true;
}
bool HostsManager::atomic_replace(const std::string& expected,const std::string& replacement,std::wstring& error) {
    DWORD code=0;
    if(!regular_file(path_)) { error=tr(L"El hosts no es un archivo normal. No se modificó el sistema."); return false; }
    if(GetFileAttributesW(path_.c_str())&FILE_ATTRIBUTE_READONLY) { error=tr(L"hosts es de solo lectura."); return false; }
    std::wstring temp;
    if(!stage_file(path_,replacement,temp,code)) { error=tr(L"No fue posible escribir el archivo hosts: ")+error_message(code); return false; }
    std::string staged,now;
    if(!read_bytes(temp,staged,code)||staged!=replacement) {
        DeleteFileW(temp.c_str()); error=tr(L"No se pudo verificar el archivo temporal. No se modificó hosts."); return false;
    }
    if(!read_bytes(path_,now,code)||now!=expected) {
        DeleteFileW(temp.c_str()); error=tr(L"El archivo hosts fue modificado externamente. Se recargará su estado; revisa los cambios antes de volver a aplicar."); return false;
    }
    // ReplaceFile preserves the original ACL; no insecure fallback that changes permissions.
    if(!ReplaceFileW(path_.c_str(),temp.c_str(),nullptr,0,nullptr,nullptr)) {
        code=GetLastError(); DeleteFileW(temp.c_str());
        error=tr(L"No fue posible reemplazar hosts. El respaldo está en hosts.bak_previous: ")+error_message(code); return false;
    }
    if(!read_bytes(path_,now,code)||now!=replacement) {
        // Do not overwrite a possible external edit with an automatic rollback.
        error=tr(L"La lectura posterior no coincide con lo escrito. No se confirmó el éxito; conserva hosts.bak_previous para recuperar el estado anterior."); return false;
    }
    return true;
}
bool HostsManager::snapshot(Snapshot& out,std::wstring& error) {
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    GetFileAttributesExW(path_.c_str(),GetFileExInfoStandard,&attributes);
    std::string bytes; TextFile text;
    if(!read_current(bytes,text,error)) return false;
    size_t begin=0,end=0;
    if(!block_span(text.text,begin,end,error)) return false;
    out=Snapshot{}; out.revision=sha256(bytes); out.write_time=attributes.ftLastWriteTime;
    if(begin!=std::wstring::npos) out.state=HostsState::Patched;

    size_t pos=0;
    while(pos<text.text.size()) {
        size_t nl=text.text.find(L'\n',pos);
        size_t next=nl==std::wstring::npos?text.text.size():nl+1;
        std::wstring line=trim(text.text.substr(pos,(nl==std::wstring::npos?text.text.size():nl)-pos));
        const bool own=begin!=std::wstring::npos&&pos>=begin&&pos<end;
        // System hosts syntax is stricter than the Markdown-friendly preset importer.
        line=line.substr(0,line.find(L'#'));
        std::wistringstream words(line); std::wstring ip,domain;
        words>>ip;
        if(valid_ip(ip)) while(words>>domain) {
            domain=lower(domain); if(!valid_domain(domain)) continue;
            (own?out.own:out.foreign).insert(domain);
            const auto key=ip+L"\n"+domain;
            out.mappings.insert(key); (own?out.own_mappings:out.foreign_mappings).insert(key);
        }
        pos=next;
    }
    if(begin==std::wstring::npos) {
        for(const std::wstring& domain:out.foreign) if(domain!=L"localhost") { out.state=HostsState::ThirdParty; break; }
    }
    return true;
}
bool HostsManager::apply(const std::vector<Entry>& entries,std::wstring& error,const std::string& expected_revision) {
    error.clear();
    FileLock lock(path_);
    if(lock.handle==INVALID_HANDLE_VALUE) { error=tr(L"No se pudo obtener acceso exclusivo para modificar hosts: ")+error_message(GetLastError()); return false; }
    std::string bytes; TextFile text;
    if(!read_current(bytes,text,error)||!unchanged(bytes,expected_revision,error)) return false;
    size_t begin=0,end=0;
    if(!block_span(text.text,begin,end,error)) return false;
    std::wstring updated=text.text;
    if(begin!=std::wstring::npos) updated.erase(begin,end-begin);
    std::wstring block=START; block+=text.newline;
    block+=CREDIT; block+=text.newline;
    Snapshot current;
    if(!snapshot(current,error)) return false;
    if(current.revision!=sha256(bytes)) { error=tr(L"El archivo hosts fue modificado externamente. Se recargará su estado; revisa los cambios antes de volver a aplicar."); return false; }
    std::map<std::wstring,std::wstring> seen;
    std::vector<std::wstring> conflicts;
    for(const Entry& item:entries) {
        Entry e=item; e.domain=lower(e.domain);
        if(!valid_ip(e.ip)||!valid_domain(e.domain)) { error=tr(L"La lista contiene una IP o un dominio inválido."); return false; }
        if(seen.count(e.domain)) {
            if(seen[e.domain]!=e.ip) { error=tr(L"La lista contiene el mismo dominio con distintas IP."); return false; }
            continue;
        }
        seen[e.domain]=e.ip;
        if(current.foreign.count(e.domain)) {
            bool satisfied=false, conflict=false;
            for(const auto& key:current.foreign_mappings) {
                const auto split=key.find(L'\n');
                if(split==std::wstring::npos||key.substr(split+1)!=e.domain) continue;
                const std::wstring foreign_ip=key.substr(0,split);
                if(foreign_ip==e.ip||(blocking_ip(foreign_ip)&&blocking_ip(e.ip))) satisfied=true;
                else conflict=true;
            }
            // Never overwrite third-party mappings. Equivalent sinkhole/loopback mappings already
            // satisfy a blocking rule; genuinely different mappings are skipped without aborting
            // the rest of the Josts list.
            if(conflict) { conflicts.push_back(e.domain); continue; }
            if(satisfied) continue;
        }
        if(!valid_ip(e.ip)||!valid_domain(e.domain)) { error=tr(L"La lista contiene una IP o un dominio inválido."); return false; }
        block+=e.ip+L" "+e.domain+text.newline;
    }
    if(!conflicts.empty()) {
        error=tr(L"Lista aplicada; se omitieron conflictos externos: ")+std::to_wstring(conflicts.size())+L" — ";
        const size_t shown=std::min<size_t>(conflicts.size(),5);
        for(size_t i=0;i<shown;++i) {
            if(i) error+=L", ";
            error+=conflicts[i];
        }
        if(conflicts.size()>shown) error+=L" (+"+std::to_wstring(conflicts.size()-shown)+L")";
    }
    block+=END; block+=text.newline;
    if(!updated.empty()&&updated.back()!=L'\n') updated=block+updated;
    else updated+=block;
    std::string replacement;
    if(!encode(text,updated,replacement)) { error=tr(L"No se pueden representar las entradas con la codificación original de hosts."); return false; }
    if(bytes==replacement) return true;
    if(!ensure_backups(bytes,error)) return false;
    return atomic_replace(bytes,replacement,error);
}
bool HostsManager::remove_own(std::wstring& error,const std::string& expected_revision) {
    FileLock lock(path_);
    if(lock.handle==INVALID_HANDLE_VALUE) { error=tr(L"No se pudo obtener acceso exclusivo para modificar hosts: ")+error_message(GetLastError()); return false; }
    std::string bytes; TextFile text;
    if(!read_current(bytes,text,error)||!unchanged(bytes,expected_revision,error)) return false;
    size_t begin=0,end=0;
    if(!block_span(text.text,begin,end,error)) return false;
    if(begin==std::wstring::npos) { error=tr(L"No hay entradas de Josts para quitar."); return false; }
    std::wstring updated=text.text;
    updated.erase(begin,end-begin);
    std::string replacement;
    if(!encode(text,updated,replacement)) { error=tr(L"No se pudo conservar la codificación de hosts."); return false; }
    if(bytes==replacement) return true;
    if(!ensure_backups(bytes,error)) return false;
    return atomic_replace(bytes,replacement,error);
}
bool HostsManager::restore(std::wstring& error,const std::string& expected_revision) {
    FileLock lock(path_);
    if(lock.handle==INVALID_HANDLE_VALUE) { error=tr(L"No se pudo obtener acceso exclusivo para modificar hosts: ")+error_message(GetLastError()); return false; }
    std::string bytes; TextFile text;
    if(!read_current(bytes,text,error)||!unchanged(bytes,expected_revision,error)) return false;
    std::wstring source=system_backup_;
    if(!regular_file(source)) { error=tr(L"No se encontró un respaldo original válido de este computador."); return false; }
    std::string backup; DWORD code=0;
    if(!read_bytes(source,backup,code)) { error=tr(L"No se encontró un respaldo válido: ")+error_message(code); return false; }
    TextFile decoded;
    if(!decode(backup,decoded)) { error=tr(L"El respaldo original del sistema no es válido. No se modificó hosts."); return false; }
    if(backup==bytes) return true;
    if(!ensure_backups(bytes,error)) return false;
    return atomic_replace(bytes,backup,error);
}
}
