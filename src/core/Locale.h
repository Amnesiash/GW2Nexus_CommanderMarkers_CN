#pragma once

#include <string>

namespace cm {
namespace Locale {

// UI language selection stored in the settings file.
//
// FollowNexus (default) mirrors whatever language Nexus itself is running: the
// addon asks Nexus to translate a probe string and picks the matching language.
// ChineseSimplified / English force one language regardless of Nexus.
enum class Language {
    FollowNexus = 0,
    ChineseSimplified = 1,
    English = 2,
};

inline constexpr Language kDefaultLanguage = Language::FollowNexus;

Language FromSettingsValue(int value);
int ToSettingsValue(Language language);

void SetLanguage(Language language);
Language Current();

// Resolves FollowNexus down to an actual language. ChineseSimplified and
// English pass through unchanged.
Language Resolved();

// Ask Nexus which language it is running and cache the answer. Safe to call
// repeatedly and with a null api (falls back to the bundled default).
void DetectNexusLanguage(void* api);

// True when the resolved language needs CJK glyphs.
bool NeedsCjkFont();

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
