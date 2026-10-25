#include "core/Locale.h"
#include "core/LocaleStrings.h"

#include "nexus/Nexus.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <unordered_map>
#include <vector>
#include <windows.h>

namespace cm {
namespace Locale {
namespace {

Language g_language = kDefaultLanguage;
StringMap g_chinese = BuildChineseSimplified();
std::unordered_map<std::string, std::string> g_overrides;
// Language Nexus itself is running; only used when g_language == FollowNexus.
Language g_nexusLanguage = Language::ChineseSimplified;
// Nexus localization API, mirrored here so T() can ask Nexus first.
AddonAPI_t* g_api = nullptr;

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

Language Resolved() {
    if (g_language != Language::FollowNexus) {
        return g_language;
    }
    return g_nexusLanguage;
}

namespace {

// Nexus does not expose a "which language are you running" getter, so the CJK
// font decision probes it: register a string whose cn and en values differ, and
// see which one comes back. Translation itself does not need this - T() just
// calls Localization_Translate and lets Nexus answer.
constexpr const char* kProbeIdentifier = "CM_LANG_PROBE";
constexpr const char* kProbeChinese = "中文";
constexpr const char* kProbeEnglish = "English";

// Nexus identifies its bundled languages by short codes, not BCP-47 tags: each
// locale file declares an Identifier and cn_Main.json uses "cn" (DisplayName
// "Chinese"), matching Settings.json's "Language": "Chinese". Registering under
// "zh-CN" alone would never match.
constexpr const char* kChineseLanguageId = "cn";
constexpr const char* kEnglishLanguageId = "en";

}  // namespace

void DetectNexusLanguage(void* api) {
    auto* nexusApi = static_cast<AddonAPI_t*>(api);
    if (!nexusApi || !nexusApi->Localization_Set || !nexusApi->Localization_Translate) {
        return;
    }

    nexusApi->Localization_Set(kProbeIdentifier, kChineseLanguageId, kProbeChinese);
    nexusApi->Localization_Set(kProbeIdentifier, kEnglishLanguageId, kProbeEnglish);

    const char* translated = nexusApi->Localization_Translate(kProbeIdentifier);
    if (!translated) {
        return;
    }
    // Anything that came back as the Chinese probe means Nexus runs Chinese.
    g_nexusLanguage = std::string(translated) == kProbeChinese ? Language::ChineseSimplified
                                                               : Language::English;
}

bool NeedsCjkFont() {
    return Resolved() == Language::ChineseSimplified;
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

namespace {

// Chinese glyphs are frequently absent from the Nexus default font, so the
// translated UI would render as tofu boxes. Merge a Windows CJK font into the
// active font as a glyph-range fallback.
bool g_fontMerged = false;

constexpr ImWchar kChineseGlyphRanges[] = {
    0x0020, 0x00FF,  // Basic Latin + Latin-1 Supplement
    0x2000, 0x206F,  // General Punctuation (curly quotes, dashes, ellipsis)
    0x3000, 0x303F,  // CJK Symbols and Punctuation
    0x4E00, 0x9FFF,  // CJK Unified Ideographs
    0xFF00, 0xFFEF,  // Halfwidth and Fullwidth Forms
    0,
};

// Microsoft YaHei (微软雅黑) is the primary CJK fallback: it is present on every
// supported Windows install, has full Simplified Chinese coverage, and reads
// well at the small sizes used by this UI. msyh.ttc is a font *collection*, so
// the index inside it has to be selected explicitly via ImFontConfig::FontNo
// (index 0 is Microsoft YaHei / 微软雅黑; 1 is Microsoft YaHei Bold / 粗体).
const char* kPrimaryFallbackFile = "C:\\Windows\\Fonts\\msyh.ttc";
constexpr int kPrimaryFallbackFontNo = 0;

// Secondary fallbacks, used only when the primary file is missing or fails to
// load (non-Chinese Windows editions, stripped enterprise images, ...).
const char* CandidateFontFiles[] = {
    "C:\\Windows\\Fonts\\msyh.ttc",
    "C:\\Windows\\Fonts\\msyh.ttf",
    "C:\\Windows\\Fonts\\simhei.ttf",
    "C:\\Windows\\Fonts\\simsun.ttc",
    "C:\\Windows\\Fonts\\Deng.ttf",
};

bool FileExists(const char* path) {
    if (!path) {
        return false;
    }
    const DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

}  // namespace

void RegisterFallbackFont(void* font) {
    auto* uiFont = static_cast<ImFont*>(font);
    if (!uiFont || g_fontMerged) {
        return;
    }
    // The font fallback follows the active language: an English UI has no CJK
    // glyphs to render, so merging a 20 MB Chinese font would only waste
    // texture memory.
    if (!NeedsCjkFont()) {
        return;
    }

    ImFontAtlas* atlas = uiFont->ContainerAtlas;
    if (!atlas) {
        return;
    }

    ImFontConfig config;
    config.MergeMode = true;
    config.PixelSnapH = true;
    // The fallback only supplies glyphs the default font lacks, so the English
    // metrics of the host font stay untouched.

    // Primary: Microsoft YaHei, with the collection index selected explicitly.
    if (FileExists(kPrimaryFallbackFile)) {
        config.FontNo = kPrimaryFallbackFontNo;
        if (atlas->AddFontFromFileTTF(kPrimaryFallbackFile, uiFont->FontSize, &config,
                                      kChineseGlyphRanges)) {
            g_fontMerged = true;
            return;
        }
    }

    // Fallbacks: plain TTF files (FontNo 0 is the only face they contain).
    config.FontNo = 0;
    for (const char* candidate : CandidateFontFiles) {
        if (!FileExists(candidate)) {
            continue;
        }
        if (atlas->AddFontFromFileTTF(candidate, uiFont->FontSize, &config, kChineseGlyphRanges)) {
            g_fontMerged = true;
            return;
        }
    }
}

}  // namespace Locale
}  // namespace cm
