#include "ui/BillboardRenderer.h"

#include "core/AppState.h"
#include "core/MumbleUtils.h"
#include "data/MapDataCache.h"
#include "services/MapWatchService.h"
#include "ui/DatAssetIconService.h"
#include "ui/TextureService.h"
#include "ui/UiFontService.h"
#include "ui/UiLayer.h"
#include "utils/InteractBindService.h"
#include "utils/WorldProjection.h"
#include "core/Locale.h"

#include <cmath>
#include <algorithm>
#include <imgui.h>

namespace cm {
namespace BillboardRenderer {
namespace {

// Short alias so the wrapped literals stay readable.
using cm::Locale::T;


constexpr float kFloorFilterMeters = 30.0f;
constexpr float kMaxBillboardHorizontalDistanceGameUnits = 5000.0f;
constexpr float kMaxBillboardHorizontalDistanceMeters =
    kMaxBillboardHorizontalDistanceGameUnits / MapDataCache::kMetersToInches;
constexpr float kPreviewMarkerIconSize = 32.0f;
constexpr float kMinTriggerBillboardSize = 32.0f;
constexpr float kTriggerBillboardFullSizeDistance = 5.0f;

constexpr ImGuiWindowFlags kBillboardOverlayFlags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;

struct BillboardOverlayWindow {
    int styleVars = 0;

    bool Begin(const NexusLinkData_t* nexusLink) {
        if (!nexusLink || nexusLink->Width == 0 || nexusLink->Height == 0) {
            return false;
        }

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(static_cast<float>(nexusLink->Width), static_cast<float>(nexusLink->Height)),
            ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        styleVars = 2;

        if (!ImGui::Begin("##cm_billboard_overlay", nullptr, kBillboardOverlayFlags)) {
            End();
            return false;
        }
        SendWindowToDisplayBack();
        return true;
    }

    void End() {
        ImGui::End();
        if (styleVars > 0) {
            ImGui::PopStyleVar(styleVars);
            styleVars = 0;
        }
    }
};

float TriggerBillboardIconSize(float baseSize, float playerDistance) {
    // Full configured size when near the trigger; shrink with horizontal player distance.
    const float distance = std::max(playerDistance, kTriggerBillboardFullSizeDistance);
    const float scaled = baseSize * (kTriggerBillboardFullSizeDistance / distance);
    return std::max(kMinTriggerBillboardSize, std::min(baseSize, scaled));
}

bool ShouldRender(const AppState& state) {
    if (!state.settings.autoMarkerEnabled || !state.settings.billboardEnabled) {
        return false;
    }
    if (!IsInGame(state.mumbleLink, state.nexusLink)) {
        return false;
    }
    if (MapUiIsOpen(state.mumbleLink)) {
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
    return true;
}

bool IsWithinBillboardCull(const Vec3f& player, const Vec3f& marker) {
    if (std::fabs(player.z - marker.z) > kFloorFilterMeters) {
        return false;
    }

    const float dx = player.x - marker.x;
    const float dy = player.y - marker.y;
    const float horizontalDistanceSq = dx * dx + dy * dy;
    return horizontalDistanceSq <=
           kMaxBillboardHorizontalDistanceMeters * kMaxBillboardHorizontalDistanceMeters;
}

float Distance(const Vec3f& a, const Vec3f& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void DrawMarkerIcon(ImDrawList* draw,
                    ImTextureID texture,
                    const Vec2f& screenPos,
                    float iconSize,
                    float alpha) {
    if (!texture) {
        return;
    }

    const float half = iconSize * 0.5f;
    const ImVec2 p0(screenPos.x - half, screenPos.y - half);
    const ImVec2 p1(screenPos.x + half, screenPos.y + half);
    const ImU32 tint = IM_COL32(255, 255, 255, static_cast<int>(255.0f * alpha));
    draw->AddImage(texture, p0, p1, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), tint);
}

void DrawInteractBubble(AppState& state, const Vec3f& player, const MarkerSet& markerSet) {
    if (!state.settings.billboardPlacement) {
        return;
    }

    const Vec3f trigger{markerSet.trigger.x, markerSet.trigger.y, markerSet.trigger.z};
    if (Distance(player, trigger) >= MapWatchService::TRIGGER_DISTANCE_CLOSED_MAP) {
        return;
    }

    if (!state.nexusLink) {
        return;
    }

    const ImTextureID bubble =
        DatAssetIconService::Request(DatAssetIconService::kInteractBubbleAssetId);

    const float scale = state.nexusLink->Scaling > 0.0f ? state.nexusLink->Scaling : 1.0f;
    const float width = static_cast<float>(state.nexusLink->Width);
    const float height = static_cast<float>(state.nexusLink->Height);

    // GW2's interact prompt uses a fixed viewport ratio (lower-right), not screen center.
    constexpr float kBubbleWidth = 300.0f;
    constexpr float kBubbleHeight = 150.0f;
    constexpr float kIconInset = 70.0f;
    constexpr float kNativeInteractAnchorXRatio = 0.70f;
    constexpr float kNativeInteractAnchorYRatio = 0.70f;
    constexpr float kNativeInteractHalfHeight = 24.0f;
    constexpr float kGapBelowNative = 10.0f;

    const float bubbleW = kBubbleWidth * scale;
    const float bubbleH = kBubbleHeight * scale;
    const float anchorX = width * kNativeInteractAnchorXRatio;
    const float anchorY = height * kNativeInteractAnchorYRatio;
    const float centerX = anchorX;
    const float centerY =
        anchorY + (kNativeInteractHalfHeight + kGapBelowNative) * scale + bubbleH * 0.5f;
    const ImVec2 p0(centerX - bubbleW * 0.5f, centerY - bubbleH * 0.5f);
    const ImVec2 p1(centerX + bubbleW * 0.5f, centerY + bubbleH * 0.5f);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (bubble) {
        draw->AddImage(bubble, p0, p1);
    } else {
        draw->AddRectFilled(p0, p1, IM_COL32(40, 40, 40, 200), 8.0f);
    }

    char prompt[256];
    std::snprintf(prompt, sizeof(prompt), T("Press '%s' to place markers\n%s"),
                  InteractBindService::GetPrimaryKeyLabel(), markerSet.name.c_str());

    ImFont* font = UiFontService::GetUiFont(state.nexusLink);
    const float fontSize = UiFontService::EffectiveFontSize(font, scale);
    const ImVec2 textSize = UiFontService::MeasureText(prompt, font, fontSize);
    const ImVec2 textPos(p0.x + kIconInset * scale, centerY - textSize.y * 0.5f);
    UiFontService::DrawTextShadowed(draw, font, fontSize, textPos, IM_COL32(255, 255, 255, 255),
                                    prompt);
}

}  // namespace

void Render(AppState& state) {
    if (!ShouldRender(state) || !state.mumbleLink || !state.nexusLink) {
        return;
    }

    BillboardOverlayWindow overlay;
    if (!overlay.Begin(state.nexusLink)) {
        return;
    }

    const GameCamera camera = BuildGameCamera(state.mumbleLink);
    const Vec3f player = PlayerPosition(state.mumbleLink);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImTextureID triggerIcon = TextureService::GetTriggerMarkerTexture();
    const float triggerIconSize =
        TriggerMarkerSizePixels(state.settings.triggerMarkerSize);

    for (const MarkerSet& markerSet : state.mapWatch.CurrentMapMarkers()) {
        const Vec3f trigger{markerSet.trigger.x, markerSet.trigger.y, markerSet.trigger.z};
        if (!IsWithinBillboardCull(player, trigger)) {
            continue;
        }

        const float triggerDistance = Distance(player, trigger);
        Vec2f screenPos{};
        if (WorldToScreen(trigger, camera, state.nexusLink, screenPos)) {
            const float alpha =
                triggerDistance < MapWatchService::TRIGGER_DISTANCE_CLOSED_MAP ? 0.25f : 0.8f;
            const float iconSize =
                TriggerBillboardIconSize(triggerIconSize, triggerDistance);
            DrawMarkerIcon(draw, triggerIcon, screenPos, iconSize, alpha);
        }

        if (state.settings.billboardPreview &&
            triggerDistance < MapWatchService::TRIGGER_DISTANCE_CLOSED_MAP) {
            for (const MarkerCoord& marker : markerSet.markers) {
                const Vec3f world{marker.x, marker.y, marker.z};
                if (!IsWithinBillboardCull(player, world)) {
                    continue;
                }

                const SquadMarker squadMarker =
                    marker.icon > 0 && marker.icon <= 9 ? static_cast<SquadMarker>(marker.icon)
                                                        : SquadMarker::Arrow;
                const ImTextureID texture = TextureService::GetTexture(squadMarker);
                if (WorldToScreen(world, camera, state.nexusLink, screenPos)) {
                    DrawMarkerIcon(draw, texture, screenPos, kPreviewMarkerIconSize, 1.0f);
                }
            }
        }

        DrawInteractBubble(state, player, markerSet);
    }

    overlay.End();
}

}  // namespace BillboardRenderer
}  // namespace cm
