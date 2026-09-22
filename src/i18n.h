#pragma once
#include <windows.h>

// Translations are compiled into the portable executable — xdCL.
namespace josts {
enum class Language { SpanishChile, English };
Language language();
void set_language(Language value);
LANGID language_id();
const wchar_t* tr(const wchar_t* spanish);
}
