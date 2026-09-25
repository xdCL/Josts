#include "common.h"
#include "parser.h"
#include "hosts_manager.h"
#include "font_loader.h"
#include "resource.h"
#include "theme.h"
#include <cassert>

using namespace josts;
int main() {
    // Validate the real system path without reading or modifying the real hosts.
    wchar_t system_dir[MAX_PATH]={};
    const UINT length=GetSystemDirectoryW(system_dir,MAX_PATH);
    assert(length>0&&length<MAX_PATH);
    HostsManager system_manager;
    assert(system_manager.path()==join(std::wstring(system_dir),L"drivers\\etc\\hosts"));
    DWORD binary_type=0;
    assert(GetBinaryTypeW(join(executable_dir(),L"Josts.exe").c_str(),&binary_type));
    assert(binary_type==(sizeof(void*)==4?SCS_32BIT_BINARY:SCS_64BIT_BINARY));
    const std::wstring lines=L"# cabecera\n- [example.com](https://example.org)\n0.0.0.0 www.example.com m.example.com\n```\nbad.example\n```\n// comentario\n0.0.0.0 example.com\n";
    ParseResult parsed=parse_list(lines);
    assert(parsed.entries.size()==3&&parsed.duplicates==1&&parsed.errors==0);
    assert(parsed.entries[0].domain==L"example.com");
    assert(parsed.entries[1].domain==L"www.example.com");
    std::wstring dir=join(executable_dir(),L"testdata"); assert(ensure_dir(dir));
    const std::string original="\xef\xbb\xbf# ajeno\r\n127.0.0.1 tercero.com\r\n";
    std::wstring path=join(dir,L"hosts"); DWORD code=0;
    assert(write_bytes(path,original,code));
    HostsManager manager(path,join(dir,L"hosts_original.bak"));
    Snapshot snapshot; std::wstring error;
    assert(manager.snapshot(snapshot,error));
    assert(snapshot.foreign.count(L"tercero.com"));
    assert(manager.apply({{L"0.0.0.0",L"nuevo.com",false}},error));
    std::string patched; assert(read_bytes(path,patched,code));
    assert(patched.compare(0,original.size(),original)==0);
    assert(patched.find("nuevo.com")!=std::string::npos);
    assert(manager.snapshot(snapshot,error)&&snapshot.own.count(L"nuevo.com"));
    assert(manager.remove_own(error));
    std::string restored; assert(read_bytes(path,restored,code)&&restored==original);
    assert(write_bytes(path,"# cambio ajeno\r\n",code));
    assert(manager.restore(error));
    assert(read_bytes(path,restored,code)&&restored==original);
    {
        const auto clean=join(dir,L"integrity-"+random_id()); assert(ensure_dir(clean));
        const auto file=join(clean,L"hosts");
        const std::string external="127.0.0.1 localhost\r\n::1 localhost\r\n# keep this exactly\r\n";
        assert(write_bytes(file,external,code)); HostsManager checked(file);
        Snapshot first; assert(checked.snapshot(first,error));
        assert(checked.apply({{L"127.0.0.1",L"localhost",false},{L"0.0.0.0",L"Duplicate.EXAMPLE",false},{L"0.0.0.0",L"duplicate.example",false}},error,first.revision));
        assert(read_bytes(file,restored,code));
        assert(restored.find(external)==0);
        auto occurrence=restored.find("duplicate.example"); assert(occurrence!=std::string::npos&&restored.find("duplicate.example",occurrence+1)==std::string::npos);
        assert(!checked.remove_own(error,first.revision)); // A stale UI cannot delete a newer block.
        Snapshot current; assert(checked.snapshot(current,error));
        HANDLE lock=CreateFileW((file+L".josts.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_HIDDEN,nullptr);
        assert(lock!=INVALID_HANDLE_VALUE);
        assert(!checked.remove_own(error,current.revision)); CloseHandle(lock);
        assert(checked.remove_own(error,current.revision));
        assert(read_bytes(file,restored,code)&&restored==external);

        // A pre-existing equivalent blocker satisfies the rule even with another sinkhole IP.
        const auto equivalent_file=join(clean,L"equivalent-hosts");
        const std::string equivalent_external="0.0.0.0 already-blocked.example\r\n";
        assert(write_bytes(equivalent_file,equivalent_external,code));
        HostsManager equivalent(equivalent_file); Snapshot equivalent_snapshot;
        assert(equivalent.snapshot(equivalent_snapshot,error));
        error.clear();
        assert(equivalent.apply({{L"127.0.0.1",L"already-blocked.example",false},{L"127.0.0.1",L"new-rule.example",false}},error,equivalent_snapshot.revision));
        assert(error.empty());
        assert(read_bytes(equivalent_file,restored,code));
        assert(restored.find("0.0.0.0 already-blocked.example")!=std::string::npos);
        assert(restored.find("127.0.0.1 already-blocked.example")==std::string::npos);
        assert(restored.find("127.0.0.1 new-rule.example")!=std::string::npos);

        // A genuinely different external mapping is preserved and skipped, but must not abort
        // the remaining Josts battery.
        const auto conflict_file=join(clean,L"conflict-hosts");
        const std::string conflict_external="120.0.0.1 www.tiktok.com\r\n";
        assert(write_bytes(conflict_file,conflict_external,code));
        HostsManager conflict_manager(conflict_file); Snapshot conflict_snapshot;
        assert(conflict_manager.snapshot(conflict_snapshot,error));
        error.clear();
        assert(conflict_manager.apply({{L"127.0.0.1",L"www.tiktok.com",false},{L"127.0.0.1",L"rest-of-list.example",false}},error,conflict_snapshot.revision));
        assert(!error.empty()&&error.find(L"www.tiktok.com")!=std::wstring::npos);
        assert(read_bytes(conflict_file,restored,code));
        assert(restored.find("120.0.0.1 www.tiktok.com")!=std::string::npos);
        assert(restored.find("127.0.0.1 www.tiktok.com")==std::string::npos);
        assert(restored.find("127.0.0.1 rest-of-list.example")!=std::string::npos);

        const auto bad_backup=join(clean,L"backup-failure");
        assert(write_bytes(bad_backup,external,code)); assert(ensure_dir(bad_backup+L".bak_previous"));
        HostsManager cannot_backup(bad_backup);
        assert(!cannot_backup.apply({{L"0.0.0.0",L"safe.example",false}},error));
        assert(read_bytes(bad_backup,restored,code)&&restored==external);
    }
    {
        // Some managed Windows images may have no hosts file at all. Treat absence as a
        // recoverable state, create the file only after an explicit apply, and remember
        // that restore must return the machine to the original "missing" state.
        const auto missing_dir=join(dir,L"missing-"+random_id()); assert(ensure_dir(missing_dir));
        const auto missing_file=join(missing_dir,L"hosts");
        HostsManager missing_manager(missing_file); Snapshot missing_snapshot;
        error.clear();
        assert(missing_manager.snapshot(missing_snapshot,error));
        assert(missing_snapshot.state==HostsState::Missing&&missing_snapshot.revision=="MISSING");
        assert(!exists(missing_file));
        assert(missing_manager.apply({{L"127.0.0.1",L"missing-file.example",false}},error,missing_snapshot.revision));
        assert(exists(missing_file));
        assert(read_bytes(missing_file,restored,code)&&restored.find("missing-file.example")!=std::string::npos);
        assert(exists(missing_file+L".bak_original_missing"));
        Snapshot created; assert(missing_manager.snapshot(created,error)&&created.state==HostsState::Patched);
        assert(missing_manager.restore(error,created.revision));
        assert(!exists(missing_file));
    }
    const std::wstring malformed=join(dir,L"malformed");
    assert(write_bytes(malformed,"# ===== INICIO Josts =====\r\n0.0.0.0 x.com\r\n",code));
    HostsManager bad(malformed,join(dir,L"other.bak"));
    assert(!bad.apply({{L"0.0.0.0",L"x.com",false}},error));
    std::string unchanged; assert(read_bytes(malformed,unchanged,code));
    assert(unchanged.find("# ===== FIN Josts =====")==std::string::npos);
    const std::wstring no_eol=join(dir,L"no_eol");
    const std::string no_eol_original="# sin salto final";
    assert(write_bytes(no_eol,no_eol_original,code));
    HostsManager no_eol_manager(no_eol,join(dir,L"no_eol.bak"));
    assert(no_eol_manager.apply({{L"0.0.0.0",L"uno.com",false}},error));
    assert(no_eol_manager.remove_own(error));
    assert(read_bytes(no_eol,restored,code)&&restored==no_eol_original);
    TextFile utf16; utf16.encoding=Encoding::Utf16Le; utf16.newline=L"\r\n";
    std::string encoded; assert(encode(utf16,L"# UTF16\r\n",encoded));
    const std::wstring utf16path=join(dir,L"utf16"); assert(write_bytes(utf16path,encoded,code));
    HostsManager manager16(utf16path,join(dir,L"utf16.bak"));
    assert(manager16.apply({{L"0.0.0.0",L"unicode.com",false}},error));
    std::string bytes16; TextFile result16;
    assert(read_bytes(utf16path,bytes16,code)&&decode(bytes16,result16));
    assert(result16.encoding==Encoding::Utf16Le&&result16.text.find(L"unicode.com")!=std::wstring::npos);
    TextFile be; be.encoding=Encoding::Utf16Be;
    std::string big_endian; assert(encode(be,L"0.0.0.0 ejemplo.com\r\n",big_endian));
    TextFile decoded_be; assert(decode(big_endian,decoded_be));
    assert(decoded_be.encoding==Encoding::Utf16Be&&decoded_be.text==L"0.0.0.0 ejemplo.com\r\n");
    std::wstring large;
    for(int i=0;i<50000;++i) large+=L"bloqueo"+std::to_wstring(i)+L".example\n";
    ParseResult bulk=parse_list(large);
    assert(bulk.entries.size()==50000&&bulk.errors==0);
    HMODULE program=LoadLibraryExW(join(executable_dir(),L"Josts.exe").c_str(),nullptr,LOAD_LIBRARY_AS_DATAFILE);
    assert(program);
    assert(FindResourceW(program,MAKEINTRESOURCEW(IDR_LICENSE),RT_RCDATA));
    assert(FindResourceW(program,MAKEINTRESOURCEW(IDR_LICENSE_MATERIAL_ICONS),RT_RCDATA));
    assert(FindResourceW(program,MAKEINTRESOURCEW(IDR_LICENSE_LLVM_MINGW),RT_RCDATA));
    assert(FindResourceW(program,MAKEINTRESOURCEW(IDR_LICENSE_JOSTS_ES),RT_RCDATA));
    assert(FindResourceW(program,MAKEINTRESOURCEW(IDR_LICENSE_JOSTS_EN),RT_RCDATA));
    HRSRC preload=FindResourceW(program,MAKEINTRESOURCEW(IDR_PRELOAD),RT_RCDATA);
    assert(preload);
    HGLOBAL preload_block=LoadResource(program,preload);
    const char* preload_bytes=reinterpret_cast<const char*>(LockResource(preload_block));
    std::string preload_raw(preload_bytes,SizeofResource(program,preload));
    TextFile preload_text; assert(decode(preload_raw,preload_text));
    ParseResult preload_list=parse_list(preload_text.text);
    assert(preload_list.entries.size()>50);
    assert(preload_list.errors==0);
    HRSRC manifest=FindResourceW(program,MAKEINTRESOURCEW(1),MAKEINTRESOURCEW(24));
    assert(manifest);
    HGLOBAL manifest_block=LoadResource(program,manifest);
    std::string manifest_text(reinterpret_cast<const char*>(LockResource(manifest_block)),SizeofResource(program,manifest));
    assert(manifest_text.find("asInvoker")!=std::string::npos);
    assert(manifest_text.find("35138b9a-5d96-4fbd-8e2d-a2440225f93a")!=std::string::npos);
    assert(manifest_text.find("Microsoft.Windows.Common-Controls")!=std::string::npos);
    {
        Fonts fonts; assert(fonts.load(program));
        assert(fonts.icons_available());
        HFONT font=fonts.create(10); assert(font);
        HDC dc=GetDC(nullptr); HGDIOBJ previous=SelectObject(dc,font);
        wchar_t face[LF_FACESIZE]={}; assert(GetTextFaceW(dc,LF_FACESIZE,face)>0);
        assert(lower(face)==L"dm sans");
        SelectObject(dc,previous); ReleaseDC(nullptr,dc);
        HFONT icons=fonts.create_icons(18); assert(icons);
        dc=GetDC(nullptr); previous=SelectObject(dc,icons);
        wchar_t icon_face[LF_FACESIZE]={}; assert(GetTextFaceW(dc,LF_FACESIZE,icon_face)>0);
        assert(lower(icon_face)==L"material icons");
        wchar_t symbols[]={0xe171,0xe065,0xe8b3,0xe16c,0xe2c6,0xe148,0xe3c9,0xe92e,0xe5d8,0xe5db,0xe5d5,0xe2c4,0xe8ef,0xe894,0xe0e5};
        WORD glyphs[sizeof(symbols)/sizeof(symbols[0])]={};
        assert(GetGlyphIndicesW(dc,symbols,static_cast<int>(sizeof(symbols)/sizeof(symbols[0])),glyphs,GGI_MARK_NONEXISTING_GLYPHS)!=GDI_ERROR);
        for(WORD glyph:glyphs) assert(glyph!=0xffff);
        SelectObject(dc,previous); ReleaseDC(nullptr,dc); DeleteObject(icons);
        HWND button=CreateWindowW(L"BUTTON",L"Aplicar",0,0,0,100,32,nullptr,nullptr,nullptr,nullptr);
        assert(button);
        HDC screen=GetDC(nullptr),paint=CreateCompatibleDC(screen);
        HBITMAP bitmap=CreateCompatibleBitmap(screen,100,32);
        HGDIOBJ old_bitmap=SelectObject(paint,bitmap);
        DRAWITEMSTRUCT draw{}; draw.CtlType=ODT_BUTTON; draw.hwndItem=button; draw.hDC=paint; draw.rcItem={0,0,100,32};
        theme::paint_button(&draw,font,nullptr,0,false);
        assert(GetPixel(paint,2,2)==theme::surface());
        theme::paint_button(&draw,font,nullptr,0,true);
        assert(GetPixel(paint,2,2)==RGB(40,142,202));
        SelectObject(paint,old_bitmap); DeleteObject(bitmap); DeleteDC(paint); ReleaseDC(nullptr,screen); DestroyWindow(button); DeleteObject(font);
    }
    FreeLibrary(program);
    return 0;
}
