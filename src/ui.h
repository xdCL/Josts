#pragma once
#include "common.h"
#include "font_loader.h"
#include "hosts_manager.h"
#include "edge_settings.h"
#include <functional>
#include <thread>
#include "privileged_ops.h"

namespace josts {
struct ActionResult;
class Ui {
public:
    Ui(HINSTANCE instance,Fonts& fonts);
    ~Ui();
    int run(int show);
private:
    static LRESULT CALLBACK window_proc(HWND,UINT,WPARAM,LPARAM);
    LRESULT handle(UINT,WPARAM,LPARAM);
    void create_controls();
    void layout();
    void paint_background(HDC dc);
    void set_mode(bool advanced);
    void toggle_language();
    void update_simple_view();
    void show_welcome_result(bool readable);
    bool preload_is_current() const;
    void load_preload();
    void show_hosts_snapshot();
    void merge_active_entries();
    void refresh_system();
    void refresh_filter();
    void report(const std::wstring& message);
    void add_or_edit(bool edit);
    void remove_selected();
    void move_selected(int direction);
    void import_files();
    void export_file();
    void apply(bool selected_only,bool preload_only=false,bool close_after=false);
    void remove_own();
    void restore();
    void block_edge_news();
    void about();
    int notice(HWND owner,const wchar_t* message,const wchar_t* title,UINT flags);
    void set_busy(bool busy);
    void start_action(int operation,const std::wstring& label,std::function<void(ActionResult&)> work,bool quick=false);
    void finish_action(bool ok,const std::wstring& message);
    void update_countdown();
    void begin_auto_close(unsigned milliseconds);
    void cancel_auto_close();
    std::vector<size_t> selection() const;
    std::wstring entry_status(const Entry& e) const;
    HINSTANCE instance_;
    Fonts& fonts_;
    HostsManager hosts_;
    HWND window_=nullptr,list_=nullptr,search_=nullptr,console_=nullptr,status_=nullptr,admin_=nullptr;
    HWND brand_=nullptr,header_=nullptr,about_button_=nullptr,mode_button_=nullptr;
    HWND language_button_=nullptr;
    HWND edge_button_=nullptr,tagline_=nullptr;
    std::function<bool()> edge_action_=BloquearContenidoNoticiasEdge;
    std::function<PrivilegedResult(HWND,const PrivilegedRequest&)> privileged_action_=request_privileged;
    FILETIME hosts_write_time_{};
    HWND simple_heading_=nullptr,simple_description_=nullptr,simple_count_=nullptr;
    HWND simple_hint_=nullptr,simple_footer_=nullptr,simple_apply_=nullptr,simple_unpatch_=nullptr;
    HWND quick_apply_=nullptr,keep_open_=nullptr,progress_=nullptr,progress_text_=nullptr;
    HFONT regular_=nullptr,bold_=nullptr,title_=nullptr,icons_=nullptr;
    std::vector<HWND> buttons_;
    std::vector<Entry> entries_;
    std::vector<Entry> preload_entries_;
    std::vector<size_t> filtered_;
    Snapshot snapshot_;
    bool busy_=false;
    bool advanced_=false;
    bool hosts_available_=false;
    bool elevated_=false;
    size_t preload_errors_=0;
    enum class Feedback { Idle,Working,Success,Failure };
    Feedback feedback_=Feedback::Idle;
    std::wstring activity_text_=L"Listo",data_dir_;
    unsigned animation_frame_=0;
    ULONGLONG close_at_=0;
    bool quick_result_=false;
    enum class WelcomeState { Checking,NeedsPatch,NeedsUpdate,Current,Unavailable,PresetUnavailable };
    WelcomeState welcome_state_=WelcomeState::Checking;
    bool welcome_=true,startup_checked_=false,no_changes_close_=false;
    std::thread worker_;
#ifdef JOSTS_UI_TESTS
    friend struct UiTestAccess;
#endif
};
}
