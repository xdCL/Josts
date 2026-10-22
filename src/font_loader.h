#pragma once
#include <windows.h>
#include <vector>
namespace josts {
class Fonts {
public:
    bool load(HINSTANCE instance);
    HFONT create(int points,int weight=FW_NORMAL,bool italic=false) const;
    HFONT create_icons(int points) const;
    bool available() const { return loaded_; }
    bool icons_available() const { return icons_loaded_; }
    ~Fonts();
private:
    std::vector<HANDLE> handles_;
    bool loaded_ = false;
    bool icons_loaded_ = false;
};
}
