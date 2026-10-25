#include "core/Locale.h"
#include "core/LocaleStrings.h"

#include "nexus/Nexus.h"

#include <unordered_map>

namespace cm {
namespace Locale {
namespace {

Language g_language = kDefaultLanguage;
StringMap g_chinese = BuildChineseSimplified();
std::unordered_map<std::string, std::string> g_overrides;
// Nexus localization API, mirrored here so T() can ask Nexus first.
AddonAPI_t* g_api = nullptr;

// Nexus identifies its bundled languages by short codes, not BCP-47 tags: each
// locale file declares an Identifier and cn_Main.json uses "cn" (DisplayName
// "Chinese"), matching Settings.json's "Language": "Chinese". Registering under
// "zh-CN" alone would never match.
constexpr const char* kChineseLanguageId = "cn";
constexpr const char* kEnglishLanguageId = "en";

}  // namespace

void SetApi(void* api) {
    g_api = static_cast<AddonAPI_t*>(api);
}

// Settings compatibility: before the FollowNexus option existed, 0 meant
// Chinese and 1 meant English. Keep those two meanings and put FollowNexus at
// the new value 2 so existing installs keep their chosen language.
Language FromSettingsValue(int value) {
    switch (value) {
        case 0:
            return Language::ChineseSimplified;
        case 1:
            return Language::English;
        default:
            return Language::FollowNexus;
    }
}

int ToSettingsValue(Language language) {
    switch (language) {
        case Language::ChineseSimplified:
            return 0;
        case Language::English:
            return 1;
        default:
            return 2;
    }
}

void SetLanguage(Language language) {
    g_language = language;
}

Language Current() {
    return g_language;
}

const char* T(const char* text) {
    if (!text || text[0] == '\0') {
        return text;
    }

    // FollowNexus: let Nexus resolve the identifier against its own active
    // language. This is the same approach the other CN addons use - the addon
    // registers en + cn copies and simply asks for the current one. Anything
    // Nexus cannot resolve comes back unchanged (the English source), which is
    // exactly the fallback we want.
    if (g_language == Language::FollowNexus) {
        if (g_api && g_api->Localization_Translate) {
            if (const char* translated = g_api->Localization_Translate(text)) {
                return translated;
            }
        }
        // No Nexus localization available: fall through to the bundled table.
    } else if (g_language != Language::ChineseSimplified) {
        return text;
    }

    const auto overrideIt = g_overrides.find(text);
    if (overrideIt != g_overrides.end()) {
        return overrideIt->second.c_str();
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
        nexusApi->Localization_Set(entry.first.c_str(), kEnglishLanguageId, entry.first.c_str());
    }
}

}  // namespace Locale
}  // namespace cm
