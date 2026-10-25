#include "ui/UiFontService.h"

#include "core/Locale.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace cm {
namespace UiFontService {

namespace {

ImFont* g_menomoniaFont = nullptr;
AddonAPI_t* g_api = nullptr;
bool g_cjkMerged = false;

void OnMenomoniaFont(const char* /*identifier*/, void* font) {
    g_menomoniaFont = static_cast<ImFont*>(font);
    if (font && !g_cjkMerged) {
        Locale::RegisterFallbackFont(font);
        g_cjkMerged = true;
    }
}

}  // namespace

void Initialize(AddonAPI_t* api, NexusLinkData_t* nexusLink) {
    (void)nexusLink;
    g_api = api;
    g_menomoniaFont = nullptr;
    // The ImGui default font is used whenever neither the MENOMONIA font nor the
    // Nexus link font resolves, so merge the CJK fallback into it here too.
    if (!g_cjkMerged) {
        Locale::RegisterFallbackFont(ImGui::GetFont());
        g_cjkMerged = true;
    }
    if (api && api->Fonts_Get) {
        api->Fonts_Get(kMenomoniaIdentifier, OnMenomoniaFont);
    }
}

void Shutdown(AddonAPI_t* api) {
    if (api && api->Fonts_Release) {
        api->Fonts_Release(kMenomoniaIdentifier, OnMenomoniaFont);
    }
    g_menomoniaFont = nullptr;
    g_api = nullptr;
}

ImFont* GetUiFont(NexusLinkData_t* nexusLink) {
    if (g_menomoniaFont) {
        return g_menomoniaFont;
    }
    if (nexusLink && nexusLink->Font) {
        // The Nexus link font never passes through the callback above, so the
        // CJK fallback is merged here the first time it is handed out.
        if (!g_cjkMerged) {
            Locale::RegisterFallbackFont(nexusLink->Font);
            g_cjkMerged = true;
        }
        return static_cast<ImFont*>(nexusLink->Font);
    }
    return nullptr;
}

float EffectiveFontSize(ImFont* font, float scale) {
    if (font) {
        return font->FontSize * scale;
    }
    return ImGui::GetFontSize() * scale;
}

ImVec2 MeasureText(const char* text, ImFont* font, float fontSize) {
    if (font) {
        return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    }
    return ImGui::CalcTextSize(text);
}

void DrawTextShadowed(ImDrawList* draw,
                      ImFont* font,
                      float fontSize,
                      const ImVec2& pos,
                      ImU32 color,
                      const char* text,
                      float shadowOffset) {
    if (!draw || !text) {
        return;
    }

    const ImVec2 shadowPos(pos.x + shadowOffset, pos.y + shadowOffset);
    if (font) {
        draw->AddText(font, fontSize, shadowPos, IM_COL32(0, 0, 0, 255), text);
        draw->AddText(font, fontSize, pos, color, text);
        return;
    }

    draw->AddText(shadowPos, IM_COL32(0, 0, 0, 255), text);
    draw->AddText(pos, color, text);
}

}  // namespace UiFontService
}  // namespace cm
