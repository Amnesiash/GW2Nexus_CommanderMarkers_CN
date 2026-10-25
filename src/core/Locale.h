#pragma once

#include <string>

namespace cm {
namespace Locale {

// UI language selection stored in the settings file.
//
// FollowNexus (default) mirrors whatever language Nexus itself is running: the
// addon registers en + cn copies with Nexus and hands the identifier to
// Localization_Translate, which answers in the active language.
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

// Translate a source English string into the active language.
// Returns `text` unchanged when no translation is registered.
const char* T(const char* text);

std::string Ts(const std::string& text);

// Hands the Nexus API pointer to the locale module. Must be called before the
// first T() so FollowNexus can query Nexus for translations.
void SetApi(void* api);

// Publish the bundled strings through the Nexus localization API so the loader
// may serve them. Safe to call when api (or the API entry) is null.
void RegisterNexusStrings(void* api);

}  // namespace Locale
}  // namespace cm
