#pragma once

#include <string>

namespace cm {
namespace Locale {

// Translate a source English string into Simplified Chinese.
// Returns `text` unchanged when no translation is registered, so an untranslated
// string degrades to English rather than breaking.
const char* T(const char* text);

std::string Ts(const std::string& text);

// Publish the bundled strings through the Nexus localization API under "cn".
// Purely informational: Nexus may display them for its own UI, but the addon
// does not read them back. Safe to call when api (or the API entry) is null.
void RegisterNexusStrings(void* api);

}  // namespace Locale
}  // namespace cm
