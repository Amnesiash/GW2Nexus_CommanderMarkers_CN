#include "ui/QuickAccessService.h"

#include "core/AppState.h"
#include "core/Branding.h"
#include "core/MumbleUtils.h"
#include "EmbeddedTextures.h"
#include "ui/OptionsPanel.h"
#include "ui/SettingsWindow.h"
#include "ui/TextureService.h"
#include "core/Locale.h"

#include <filesystem>
#include <imgui.h>
#include <string>

namespace cm {
namespace QuickAccessService {
namespace {

// Short alias so the wrapped literals stay readable.
using cm::Locale::T;


constexpr const char* kShortcutId = "CM_QUICKACCESS";
constexpr const char* kToggleBind = "CM_TOGGLE";
constexpr const char* kContextMenuId = "CM_CTX_MENU";

bool registered_ = false;
bool contextMenuRequested_ = false;
std::string cachedTooltip_;
SquadMarker cachedCornerTexture = SquadMarker::Heart;
std::string cachedIconTexId_;
std::string cachedHoverTexId_;

const char* CornerIconFadedAssetId(SquadMarker marker) {
    const char* assetId = TextureService::SquadMarkerFadedAssetId(marker);
    return assetId ? assetId : "heart_fade";
}

const char* CornerIconFullAssetId(SquadMarker marker) {
    const char* assetId = TextureService::SquadMarkerAssetId(marker);
    return assetId ? assetId : "heart";
}

std::string QuickAccessTextureId(const char* assetId) {
    return std::string("CM_QA_") + assetId;
}

void RenderContextMenu() {
    auto& state = AppState::Instance();
    auto& settings = state.settings;

    ImGui::Separator();

    if (ImGui::Selectable(T("Open Settings"))) {
        SettingsWindow::Open(SettingsTab::General);
    }

    if (ImGui::MenuItem(T("Lieutenant mode"), nullptr, &state.ltMode)) {
    }

    ImGui::Separator();
    ImGui::TextDisabled(T("Library (current map)"));
    const int mapId = state.mumbleLink ? static_cast<int>(state.mumbleLink->Context.MapID) : 0;
    for (const MarkerSet& markerSet : state.markerListing.GetMarkersForMap(mapId)) {
        if (!markerSet.enabled) {
            continue;
        }
        if (ImGui::Selectable(markerSet.name.c_str())) {
            if (state.mumbleLink && state.nexusLink) {
                state.mapWatch.PlaceMarkerSet(markerSet, state.mumbleLink, state.nexusLink);
            }
        }
    }

    ImGui::Separator();
    if (ImGui::Selectable(settings.showMarkerPanel ? T("Hide Clickable Markers")
                                                   : T("Show Clickable Markers"))) {
        settings.showMarkerPanel = !settings.showMarkerPanel;
        state.settings.Save(state.settingsPath());
    }
}

bool RegisterTextureAlias(AddonAPI_t* api, const char* identifier, const char* assetId) {
    if (!api || !api->Textures_GetOrCreateFromMemory) {
        return false;
    }

    const EmbeddedTextures::Asset* asset = EmbeddedTextures::Find(assetId);
    if (!asset) {
        return false;
    }

    return api->Textures_GetOrCreateFromMemory(
               identifier, const_cast<unsigned char*>(asset->data),
               static_cast<uint32_t>(asset->size)) != nullptr;
}

bool RegisterTextureFromFile(AddonAPI_t* api, const char* identifier,
                             const std::filesystem::path& path) {
    if (!api || !api->Textures_GetOrCreateFromFile) {
        return false;
    }
    if (!std::filesystem::exists(path)) {
        return false;
    }
    return api->Textures_GetOrCreateFromFile(identifier, path.string().c_str()) != nullptr;
}

bool LoadQuickAccessTexture(AddonAPI_t* api, const char* identifier, const char* assetId,
                            const std::string& addonDir) {
    if (RegisterTextureAlias(api, identifier, assetId)) {
        return true;
    }

    return RegisterTextureFromFile(api, identifier,
                                   std::filesystem::path(addonDir) / "textures" /
                                       (std::string(assetId) + ".png"));
}

bool LoadTextures(AddonAPI_t* api, const AppState& state, std::string* iconTexId,
                  std::string* hoverTexId) {
    const char* fadedAssetId = CornerIconFadedAssetId(state.settings.cornerTexture);
    const char* fullAssetId = CornerIconFullAssetId(state.settings.cornerTexture);

    *iconTexId = QuickAccessTextureId(fadedAssetId);
    *hoverTexId = QuickAccessTextureId(fullAssetId);

    return LoadQuickAccessTexture(api, iconTexId->c_str(), fadedAssetId, state.addonDir) &&
           LoadQuickAccessTexture(api, hoverTexId->c_str(), fullAssetId, state.addonDir);
}

std::string BuildTooltip(const AppState& state) {
    std::string tooltip = kDisplayName;
    if (cm::IsInGame(state.mumbleLink, state.nexusLink)) {
        tooltip += T("\nUse the context menu for library and settings.");
    }
    return tooltip;
}

void ReregisterShortcut(AddonAPI_t* api, AppState& state) {
    if (!api || !registered_) {
        return;
    }

    api->QuickAccess_RemoveContextMenu(kContextMenuId);
    api->QuickAccess_Remove(kShortcutId);
    registered_ = false;
    Register(api, state);
}

void ExecuteLeftClickAction(AppState& state) {
    switch (state.settings.cornerIconAction) {
        case CornerIconAction::ShowSettings:
            SettingsWindow::Open(SettingsTab::General);
            break;
        case CornerIconAction::Library:
            SettingsWindow::Open(SettingsTab::AutoMarkerLibrary);
            break;
        case CornerIconAction::Lieutenant:
            state.ltMode = !state.ltMode;
            break;
        case CornerIconAction::ClickMarkerToggle:
            state.settings.showMarkerPanel = !state.settings.showMarkerPanel;
            state.settings.Save(state.settingsPath());
            break;
        case CornerIconAction::ShowIconMenu:
        default:
            contextMenuRequested_ = true;
            break;
    }
}

void RenderLeftClickContextMenu() {
    if (contextMenuRequested_) {
        ImGui::SetNextWindowPos(ImGui::GetMousePos());
        ImGui::OpenPopup("CM_CORNER_CTX");
        contextMenuRequested_ = false;
    }

    if (ImGui::BeginPopup("CM_CORNER_CTX")) {
        RenderContextMenu();
        ImGui::EndPopup();
    }
}

bool TexturesReady(AddonAPI_t* api, const std::string& iconTexId, const std::string& hoverTexId) {
    if (!api || !api->Textures_Get) {
        return false;
    }

    const Texture_t* icon = api->Textures_Get(iconTexId.c_str());
    const Texture_t* hover = api->Textures_Get(hoverTexId.c_str());
    return icon && icon->Resource && hover && hover->Resource;
}

}  // namespace

void Register(AddonAPI_t* api, AppState& state) {
    if (!api || registered_) {
        return;
    }

    std::string iconTexId;
    std::string hoverTexId;
    if (!LoadTextures(api, state, &iconTexId, &hoverTexId)) {
        return;
    }
    if (!TexturesReady(api, iconTexId, hoverTexId)) {
        return;
    }

    cachedTooltip_ = BuildTooltip(state);
    cachedCornerTexture = state.settings.cornerTexture;
    cachedIconTexId_ = iconTexId;
    cachedHoverTexId_ = hoverTexId;
    api->QuickAccess_Add(kShortcutId, iconTexId.c_str(), hoverTexId.c_str(), kToggleBind,
                         cachedTooltip_.c_str());
    api->QuickAccess_AddContextMenu(kContextMenuId, kShortcutId, RenderContextMenu);
    registered_ = true;
    api->Log(LOGL_INFO, cm::kLogChannel, "QuickAccess icon registered.");
}

void Unregister(AddonAPI_t* api) {
    if (!api || !registered_) {
        return;
    }

    api->QuickAccess_RemoveContextMenu(kContextMenuId);
    api->QuickAccess_Remove(kShortcutId);
    registered_ = false;
    cachedTooltip_.clear();
    cachedIconTexId_.clear();
    cachedHoverTexId_.clear();
}

void Refresh(AddonAPI_t* api, AppState& state) {
    if (!api || !state.settings.cornerIconEnabled) {
        return;
    }

    if (!registered_) {
        Register(api, state);
        return;
    }

    const auto tooltip = BuildTooltip(state);
    if (tooltip != cachedTooltip_ || state.settings.cornerTexture != cachedCornerTexture) {
        ReregisterShortcut(api, state);
    }
}

void OnShortcutActivated(AppState& state) { ExecuteLeftClickAction(state); }

void ProcessFrame() { RenderLeftClickContextMenu(); }

void SyncVisibility(AddonAPI_t* api, AppState& state) {
    if (!api) {
        return;
    }
    if (state.settings.cornerIconEnabled) {
        Refresh(api, state);
    } else {
        Unregister(api);
    }
}

bool IsRegistered() { return registered_; }

}  // namespace QuickAccessService
}  // namespace cm
