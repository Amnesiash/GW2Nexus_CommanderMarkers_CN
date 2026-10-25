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

}  // namespace

Language FromSettingsValue(int value) {
    return value == 1 ? Language::English : Language::ChineseSimplified;
}

int ToSettingsValue(Language language) {
    return language == Language::English ? 1 : 0;
}

void SetLanguage(Language language) {
    g_language = language;
}

Language Current() {
    return g_language;
}

const char* T(const char* text) {
    if (!text || text[0] == '\0' || g_language != Language::ChineseSimplified) {
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
        nexusApi->Localization_Set(entry.first.c_str(), "zh-CN", std::string(entry.second).c_str());
        nexusApi->Localization_Set(entry.first.c_str(), "en", entry.first.c_str());
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
