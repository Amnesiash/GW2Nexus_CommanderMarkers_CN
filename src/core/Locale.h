#pragma once

#include <string>

namespace cm {
namespace Locale {

// UI language selection stored in the settings file.
// Chinese (Simplified) is the default; English stays selectable at runtime.
enum class Language {
    ChineseSimplified = 0,
    English = 1,
};

inline constexpr Language kDefaultLanguage = Language::ChineseSimplified;

Language FromSettingsValue(int value);
int ToSettingsValue(Language language);

void SetLanguage(Language language);
Language Current();

// Translate a source English string into the active language.
// Returns `text` unchanged when no translation is registered.
const char* T(const char* text);

std::string Ts(const std::string& text);

// Publish the bundled strings through the Nexus localization API so the loader
// may override them later. Safe to call when api (or the API entry) is null.
void RegisterNexusStrings(void* api);

// Chinese glyphs are not guaranteed in the font Nexus hands to addons, so the
// active UI font gets a fallback font with a CJK glyph range merged in.
void RegisterFallbackFont(void* font);

}  // namespace Locale
}  // namespace cm
