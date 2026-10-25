#include "ui/UiFontService.h"

#include <imgui.h>

namespace cm {
namespace UiFontService {

namespace {

ImFont* g_menomoniaFont = nullptr;

// The addon renders with whatever font Nexus already loaded. Nexus ships CJK
// coverage for its cn locale, so no font of our own is loaded or merged.
void OnMenomoniaFont(const char* /*identifier*/, void* font) {
    g_menomoniaFont = static_cast<ImFont*>(font);
}

}  // namespace

void Initialize(AddonAPI_t* api, NexusLinkData_t* nexusLink) {
    (void)nexusLink;
    g_menomoniaFont = nullptr;
    if (api && api->Fonts_Get) {
        api->Fonts_Get(kMenomoniaIdentifier, OnMenomoniaFont);
    }
}

void Shutdown(AddonAPI_t* api) {
    if (api && api->Fonts_Release) {
        api->Fonts_Release(kMenomoniaIdentifier, OnMenomoniaFont);
    }
    g_menomoniaFont = nullptr;
}

ImFont* GetUiFont(NexusLinkData_t* nexusLink) {
    if (g_menomoniaFont) {
        return g_menomoniaFont;
    }
    if (nexusLink && nexusLink->Font) {
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
