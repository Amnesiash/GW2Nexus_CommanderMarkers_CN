#include "ui/ScreenMapOverlay.h"

#include "core/AppState.h"
#include "core/MumbleUtils.h"
#include "data/MapDataCache.h"
#include "services/MapWatchService.h"
#include "services/MarkerListing.h"
#include "ui/TextureService.h"
#include "ui/UiFontService.h"
#include "ui/UiLayer.h"
#include "utils/InteractBindService.h"
#include "core/Locale.h"

#include <cmath>
#include <imgui.h>
#include <vector>

namespace cm {
namespace ScreenMapOverlay {
namespace {

// Short alias so the wrapped literals stay readable.
using cm::Locale::T;


constexpr float kFloorFilterMeters = 30.0f;
constexpr float kTriggerMarkerIconSize = 32.0f;
constexpr float kPreviewMarkerIconSize = 32.0f;

constexpr ImGuiWindowFlags kMapOverlayFlags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;

struct MapOverlayWindow {
    int styleVars = 0;
    int styleColors = 0;

    bool Begin(const ScreenRect& bounds) {
        if (bounds.w <= 0 || bounds.h <= 0) {
            return false;
        }

        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(bounds.x), static_cast<float>(bounds.y)),
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(static_cast<float>(bounds.w), static_cast<float>(bounds.h)), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        styleVars = 2;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
        styleColors = 1;

        if (!ImGui::Begin("##cm_screen_map_overlay", nullptr, kMapOverlayFlags)) {
            End();
            return false;
        }
        SendWindowToDisplayBack();
        return true;
    }

    void End() {
        ImGui::End();
        if (styleColors > 0) {
            ImGui::PopStyleColor(styleColors);
            styleColors = 0;
        }
        if (styleVars > 0) {
            ImGui::PopStyleVar(styleVars);
            styleVars = 0;
        }
    }
};

bool ShouldRender(const AppState& state) {
    if (!state.settings.autoMarkerEnabled) {
        return false;
    }
    if (!IsInGame(state.mumbleLink, state.nexusLink)) {
        return false;
    }
    if (IsInCombat(state.mumbleLink) && !state.settings.combatPlacement) {
        return false;
    }
    if (state.settings.autoMarkerOnlyCommander || state.ltMode) {
        if (!HasCommanderPermissions(state.mumbleLink, state.playerIdentity) && !state.ltMode) {
            return false;
        }
    }
    if (!state.settings.autoMarkerShowTrigger && !state.settings.autoMarkerShowPreview) {
        return false;
    }
    return true;
}

bool SameFloor(const Vec3f& player, const Vec3f& marker) {
    return std::fabs(player.z - marker.z) <= kFloorFilterMeters;
}

std::vector<MarkerSet> EnabledMarkersOnMap(const AppState& state, int mapId) {
    std::vector<MarkerSet> markers;
    for (const MarkerSet& markerSet : state.markerListing.GetMarkersForMap(mapId)) {
        if (markerSet.enabled) {
            markers.push_back(markerSet);
        }
    }
    return markers;
}

void DrawMarkerAt(ImDrawList* draw,
                  const Vec2f& screenPos,
                  ImTextureID texture,
                  float iconSize,
                  float alpha) {
    if (!draw || !texture) {
        return;
    }

    const float half = iconSize * 0.5f;
    const ImVec2 p0(screenPos.x - half, screenPos.y - half);
    const ImVec2 p1(screenPos.x + half, screenPos.y + half);
    const ImU32 tint = IM_COL32(255, 255, 255, static_cast<int>(255.0f * alpha));
    draw->AddImage(texture, p0, p1, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), tint);
}

ImTextureID TriggerMarkerTexture() {
    return TextureService::GetTriggerMarkerTexture();
}

void DrawTriggerHearts(const AppState& state,
                       ImDrawList* draw,
                       const ScreenMapData& screenMap,
                       const Vec3f& player,
                       const std::vector<MarkerSet>& markers) {
    if (!state.settings.autoMarkerShowTrigger) {
        return;
    }

    const ImTextureID triggerIcon = TriggerMarkerTexture();
    for (const MarkerSet& markerSet : markers) {
        const Vec3f trigger{markerSet.trigger.x, markerSet.trigger.y, markerSet.trigger.z};
        if (!SameFloor(player, trigger)) {
            continue;
        }

        const Vec2f screenPos =
            MapDataCache::WorldToScreenMap(markerSet.mapId, trigger, screenMap, state.mapData);
        if (!screenMap.screenBounds.Contains(screenPos.x, screenPos.y)) {
            continue;
        }
        DrawMarkerAt(draw, screenPos, triggerIcon, kTriggerMarkerIconSize, 1.0f);
    }
}

void DrawPreviewMarkers(const AppState& state,
                        ImDrawList* draw,
                        const ScreenMapData& screenMap,
                        const Vec3f& player) {
    if (!state.settings.autoMarkerShowPreview) {
        return;
    }

    if (!state.mapWatch.ShouldShowMapPreview(state.mumbleLink)) {
        return;
    }

    const MarkerSet* preview = state.mapWatch.ActivePreview();
    if (!preview) {
        return;
    }

    for (const MarkerCoord& marker : preview->markers) {
        const Vec3f world{marker.x, marker.y, marker.z};
        if (!SameFloor(player, world)) {
            continue;
        }

        const SquadMarker squadMarker =
            marker.icon > 0 && marker.icon <= 9 ? static_cast<SquadMarker>(marker.icon)
                                                : SquadMarker::Arrow;
        const ImTextureID texture = TextureService::GetTexture(squadMarker);
        const Vec2f screenPos =
            MapDataCache::WorldToScreenMap(preview->mapId, world, screenMap, state.mapData);
        if (!screenMap.screenBounds.Contains(screenPos.x, screenPos.y)) {
            continue;
        }
        DrawMarkerAt(draw, screenPos, texture, kPreviewMarkerIconSize, 0.85f);
    }
}

void DrawInteractPrompt(AppState& state,
                        ImDrawList* draw,
                        const Vec3f& player,
                        const std::vector<MarkerSet>& markers) {
    if (!MapUiIsOpen(state.mumbleLink)) {
        return;
    }

    const MarkerSet* closest = nullptr;
    float closestDistance = MapWatchService::TRIGGER_DISTANCE_OPEN_MAP;
    for (const MarkerSet& markerSet : markers) {
        const Vec3f trigger{markerSet.trigger.x, markerSet.trigger.y, markerSet.trigger.z};
        const float dx = player.x - trigger.x;
        const float dy = player.y - trigger.y;
        const float dz = player.z - trigger.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance < closestDistance) {
            closest = &markerSet;
            closestDistance = distance;
        }
    }

    if (!closest || !state.nexusLink || !draw) {
        return;
    }

    char prompt[256];
    std::snprintf(prompt, sizeof(prompt), T("Press '%s' to place markers\n%s"),
                  InteractBindService::GetPrimaryKeyLabel(), closest->name.c_str());

    const float scale = state.nexusLink->Scaling > 0.0f ? state.nexusLink->Scaling : 1.0f;
    ImFont* font = UiFontService::GetUiFont(state.nexusLink);
    const float fontSize = UiFontService::EffectiveFontSize(font, scale);
    const ImVec2 textSize = UiFontService::MeasureText(prompt, font, fontSize);

    const float centerX = static_cast<float>(state.nexusLink->Width) * 0.5f;
    const float bottomY = static_cast<float>(state.nexusLink->Height) - 120.0f * scale;
    const ImVec2 textPos(centerX - textSize.x * 0.5f, bottomY - textSize.y * 0.5f);
    UiFontService::DrawTextShadowed(draw, font, fontSize, textPos, IM_COL32(255, 165, 0, 255),
                                    prompt);
}

}  // namespace

void Render(AppState& state) {
    if (!ShouldRender(state) || !state.mumbleLink || !state.nexusLink) {
        return;
    }

    const int mapId = static_cast<int>(state.mumbleLink->Context.MapID);
    const std::vector<MarkerSet> markers = EnabledMarkersOnMap(state, mapId);
    if (markers.empty() || !state.mapData.MapHasGeometry(mapId)) {
        return;
    }

    const ScreenMapData screenMap =
        BuildScreenMapData(state.mumbleLink, state.nexusLink, state.playerIdentity);
    if (screenMap.screenBounds.w <= 0 || screenMap.screenBounds.h <= 0) {
        return;
    }

    const Vec3f player = PlayerPosition(state.mumbleLink);

    MapOverlayWindow overlay;
    if (!overlay.Begin(screenMap.screenBounds)) {
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    DrawTriggerHearts(state, draw, screenMap, player, markers);
    DrawPreviewMarkers(state, draw, screenMap, player);
    DrawInteractPrompt(state, draw, player, markers);
    overlay.End();
}

}  // namespace ScreenMapOverlay
}  // namespace cm
