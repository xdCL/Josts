#pragma once
#include "common.h"
#include <set>

namespace josts {
struct ParseResult {
    std::vector<Entry> entries;
    size_t duplicates = 0;
    size_t errors = 0;
};
bool valid_ip(const std::wstring& ip);
bool valid_domain(const std::wstring& domain);
ParseResult parse_list(const std::wstring& text, const std::set<std::wstring>& existing = {});
}
