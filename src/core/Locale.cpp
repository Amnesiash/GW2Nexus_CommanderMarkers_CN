#include "core/Locale.h"
#include "core/LocaleStrings.h"

#include "nexus/Nexus.h"

#include <unordered_map>

namespace cm {
namespace Locale {
namespace {

// The addon is Simplified Chinese only: there is no runtime language switch and
// nothing depends on the Nexus UI language.
StringMap g_chinese = BuildChineseSimplified();

// Nexus identifies its bundled languages by short codes, not BCP-47 tags: each
// locale file declares an Identifier and cn_Main.json uses "cn" (DisplayName
// "Chinese"), matching Settings.json's "Language": "Chinese".
constexpr const char* kChineseLanguageId = "cn";

}  // namespace

const char* T(const char* text) {
    if (!text || text[0] == '\0') {
        return text;
    }
    const auto it = g_chinese.find(text);
    return it == g_chinese.end() ? text : it->second.data();
}

std::string Ts(const std::string& text) {
    return std::string(T(text.c_str()));
}

void RegisterNexusStrings(void* api) {
    auto* nexusApi = static_cast<AddonAPI_t*>(api);
    if (!nexusApi || !nexusApi->Localization_Set) {
        return;
    }
    for (const auto& entry : g_chinese) {
        nexusApi->Localization_Set(entry.first.c_str(), kChineseLanguageId,
                                   std::string(entry.second).c_str());
    }
}

}  // namespace Locale
}  // namespace cm
