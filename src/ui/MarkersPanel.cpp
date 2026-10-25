#include "ui/MarkersPanel.h"

#include "core/AppState.h"
#include "core/MumbleUtils.h"
#include "data/MarkerDefinition.h"
#include "ui/OverlayPanel.h"
#include "ui/TextureService.h"
#include "utils/MarkerBindHelper.h"
#include "utils/MarkerPlacementHelper.h"
#include "core/Locale.h"

#include <imgui.h>
#include <optional>
#include <windows.h>
#include <windowsx.h>

namespace cm {
namespace MarkersPanel {
namespace {

// Short alias so the wrapped literals stay readable.
using cm::Locale::T;


constexpr int kHotkeyDelayMs = 50;

enum class IconFlow {
    Horizontal,
    Vertical,
};

struct PanelRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool Contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

struct PanelUiState {
    std::optional<SquadMarker> selectedGroundMarker;
    PanelRect panelRect{};
    bool panelRectValid = false;
    bool dragPanel = false;
    bool bindsOk = false;
    AddonAPI_t* api = nullptr;
};

PanelUiState g_uiState;
bool g_inputRegistered = false;

bool ShouldShowPanel(const AppState& state) {
    if (!state.settings.showMarkerPanel) {
        return false;
    }
    if (!IsInGame(state.mumbleLink, state.nexusLink)) {
        return false;
    }
    if (MapUiIsOpen(state.mumbleLink)) {
        return false;
    }
    if (state.settings.onlyCommanderPanel || state.ltMode) {
        return HasCommanderPermissions(state.mumbleLink, state.playerIdentity) || state.ltMode;
    }
    return true;
}

bool CanUseMarkerBinds() {
    return g_uiState.api && g_uiState.bindsOk;
}

void TryPlaceSelectedGroundMarkerAt(int screenX, int screenY) {
    if (!g_uiState.selectedGroundMarker || g_uiState.dragPanel || !CanUseMarkerBinds()) {
        return;
    }
    if (g_uiState.panelRectValid &&
        g_uiState.panelRect.Contains(static_cast<float>(screenX),
                                     static_cast<float>(screenY))) {
        return;
    }

    const bool useScreenCoords = UseScreenCoordinatesForPlacement();
    SetPlacementMousePosition({screenX, screenY}, useScreenCoords);
    InvokeGroundMarker(*g_uiState.selectedGroundMarker, g_uiState.api, kHotkeyDelayMs);
    g_uiState.selectedGroundMarker.reset();
}

UINT PanelWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg != WM_LBUTTONDOWN || !g_uiState.selectedGroundMarker || g_uiState.dragPanel) {
        return uMsg;
    }

    if (ImGui::GetCurrentContext()) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return uMsg;
        }
    }

    POINT screenPt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (hwnd) {
        ClientToScreen(hwnd, &screenPt);
    }

    TryPlaceSelectedGroundMarkerAt(screenPt.x, screenPt.y);
    return uMsg;
}

bool RenderMarkerButton(ImTextureID texture, int size, float opacity, bool selected) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 0.0f, 0.45f));
    }

    const bool clicked = ImGui::ImageButton(
        texture, ImVec2(static_cast<float>(size), static_cast<float>(size)),
        ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), 0,
        ImVec4(0.0f, 0.0f, 0.0f, 1.0f - opacity));

    if (selected) {
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();
    return clicked;
}

void RenderMarkerIcon(ImTextureID texture, int size, float opacity) {
    const ImVec2 iconSize(static_cast<float>(size), static_cast<float>(size));
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(iconSize);
    ImGui::SetItemAllowOverlap();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImU32 tint = IM_COL32(255, 255, 255, static_cast<int>(255.0f * opacity));
    draw->AddImage(texture, pos, ImVec2(pos.x + iconSize.x, pos.y + iconSize.y),
                   ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), tint);
}

IconFlow IconFlowForGroup(PanelLayout layout) {
    switch (layout) {
        case PanelLayout::SideBySide:
        case PanelLayout::SingleColumn:
            return IconFlow::Vertical;
        case PanelLayout::Stacked:
        case PanelLayout::SingleRow:
            return IconFlow::Horizontal;
    }
    return IconFlow::Vertical;
}

void RenderMarkerRow(AppState& state,
                     bool groundRow,
                     bool optionsPreview,
                     IconFlow iconFlow,
                     bool continueOnSameLine) {
    const SettingsStore& settings = state.settings;
    const bool rowEnabled =
        groundRow ? settings.groundMarkersEnabled : settings.objectMarkersEnabled;
    if (!rowEnabled) {
        return;
    }

    if (iconFlow == IconFlow::Vertical) {
        ImGui::BeginGroup();
    }

    bool firstIcon = true;
    for (std::size_t i = 0; i < kMarkerDefinitionCount; ++i) {
        const MarkerDefinition& def = kAllMarkers[i];
        if (groundRow && !def.supportsGroundTarget) {
            continue;
        }
        if (!groundRow && !def.supportsObjectTarget) {
            continue;
        }

        const ImTextureID texture = TextureService::GetTexture(def.markerType);
        if (!texture) {
            continue;
        }

        if (iconFlow == IconFlow::Horizontal) {
            if (!firstIcon || continueOnSameLine) {
                ImGui::SameLine(0.0f, 2.0f);
            }
        }
        firstIcon = false;

        ImGui::PushID(groundRow ? "ground" : "object");
        ImGui::PushID(static_cast<int>(def.markerType));

        char tooltip[64];
        if (groundRow) {
            std::snprintf(tooltip, sizeof(tooltip), T("%s Ground"), def.displayName);
        } else {
            std::snprintf(tooltip, sizeof(tooltip), T("%s Object"), def.displayName);
        }

        const bool selected = groundRow && g_uiState.selectedGroundMarker &&
                              *g_uiState.selectedGroundMarker == def.markerType;
        const bool dragMode = settings.dragPanel && !optionsPreview;

        if (dragMode) {
            RenderMarkerIcon(texture, settings.iconWidth, settings.panelOpacity);
        } else if (RenderMarkerButton(texture, settings.iconWidth, settings.panelOpacity, selected)) {
            if (groundRow) {
                if (selected) {
                    g_uiState.selectedGroundMarker.reset();
                } else {
                    g_uiState.selectedGroundMarker = def.markerType;
                }
            } else if (!optionsPreview && !state.placementService.IsBusy() && CanUseMarkerBinds()) {
                InvokeObjectMarker(def.markerType, state.api, kHotkeyDelayMs);
            }
        }

        if (!dragMode && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }

        if (!dragMode && groundRow && ImGui::IsItemClicked(ImGuiMouseButton_Right) &&
            CanUseMarkerBinds()) {
            InvokeGroundMarker(def.markerType, state.api, kHotkeyDelayMs);
            InvokeGroundMarker(def.markerType, state.api, kHotkeyDelayMs);
        }

        if (!dragMode && !groundRow && ImGui::IsItemClicked(ImGuiMouseButton_Right) &&
            CanUseMarkerBinds()) {
            InvokeObjectMarker(def.markerType, state.api, kHotkeyDelayMs);
        }

        ImGui::PopID();
        ImGui::PopID();
    }

    if (iconFlow == IconFlow::Vertical) {
        ImGui::EndGroup();
    }
}

void RenderPanelContent(AppState& state, bool optionsPreview) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, state.settings.panelOpacity);

    const PanelLayout layout = state.settings.panelOrientation;
    const IconFlow iconFlow = IconFlowForGroup(layout);

    switch (layout) {
        case PanelLayout::SideBySide:
            ImGui::BeginGroup();
            RenderMarkerRow(state, true, optionsPreview, iconFlow, false);
            ImGui::EndGroup();
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::BeginGroup();
            RenderMarkerRow(state, false, optionsPreview, iconFlow, false);
            ImGui::EndGroup();
            break;

        case PanelLayout::Stacked:
            RenderMarkerRow(state, true, optionsPreview, iconFlow, false);
            ImGui::Spacing();
            RenderMarkerRow(state, false, optionsPreview, iconFlow, false);
            break;

        case PanelLayout::SingleRow:
            RenderMarkerRow(state, true, optionsPreview, iconFlow, false);
            RenderMarkerRow(state, false, optionsPreview, iconFlow, true);
            break;

        case PanelLayout::SingleColumn:
            RenderMarkerRow(state, true, optionsPreview, iconFlow, false);
            RenderMarkerRow(state, false, optionsPreview, iconFlow, false);
            break;
    }

    if (state.settings.dragPanel && !optionsPreview) {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(windowPos,
                            ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
                            IM_COL32(96, 96, 96, 192));
        const char* label = T("Drag");
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        draw->AddText(
            ImVec2(windowPos.x + (windowSize.x - textSize.x) * 0.5f,
                   windowPos.y + (windowSize.y - textSize.y) * 0.5f),
            IM_COL32(0, 0, 0, 255), label);
    }

    ImGui::PopStyleVar();
}

}  // namespace

void RegisterInput(AddonAPI_t* api) {
    if (!api || !api->WndProc_Register || g_inputRegistered) {
        return;
    }
    api->WndProc_Register(PanelWndProc);
    g_inputRegistered = true;
}

void UnregisterInput(AddonAPI_t* api) {
    if (!api || !g_inputRegistered) {
        return;
    }
    if (api->WndProc_Deregister) {
        api->WndProc_Deregister(PanelWndProc);
    }
    g_inputRegistered = false;
}

void Render(AppState& state, bool optionsPreview) {
    g_uiState.api = state.api;
    g_uiState.bindsOk = state.requiredBindsOk;
    g_uiState.dragPanel = state.settings.dragPanel;

    if (!optionsPreview && !ShouldShowPanel(state)) {
        g_uiState.selectedGroundMarker.reset();
        g_uiState.panelRectValid = false;
        return;
    }

    if (optionsPreview) {
        RenderPanelContent(state, true);
        return;
    }

    if (!OverlayPanel::Begin("##cm_markers_panel", state.settings.panelLocation,
                             state.settings.dragPanel)) {
        return;
    }

    const ImVec2 winPos = ImGui::GetWindowPos();
    const ImVec2 winSize = ImGui::GetWindowSize();
    g_uiState.panelRect = {winPos.x, winPos.y, winSize.x, winSize.y};
    g_uiState.panelRectValid = true;

    RenderPanelContent(state, false);

    const uint32_t screenW = state.nexusLink ? state.nexusLink->Width : 0;
    const uint32_t screenH = state.nexusLink ? state.nexusLink->Height : 0;
    OverlayPanel::End(true, screenW, screenH);

    if (OverlayPanel::ConsumeDragFinished()) {
        state.settings.Save(state.settingsPath());
    }
}

}  // namespace MarkersPanel
}  // namespace cm
