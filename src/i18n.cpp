#include "i18n.h"
#include <atomic>
#include <cwchar>

namespace josts {
static std::atomic<Language> selected{Language::SpanishChile};
Language language() { return selected.load(); }
void set_language(Language value) { selected.store(value); }
LANGID language_id() {
    return language()==Language::SpanishChile?MAKELANGID(LANG_SPANISH,SUBLANG_SPANISH_CHILE):MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US);
}
struct Translation { const wchar_t* spanish; const wchar_t* english; };
static const Translation translations[]={
#include "translations.inc"
};
const wchar_t* tr(const wchar_t* spanish) {
    if(language()==Language::English) for(const auto& entry:translations) if(std::wcscmp(entry.spanish,spanish)==0) return entry.english;
    return spanish;
}
}
