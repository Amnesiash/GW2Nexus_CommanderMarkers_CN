#include "core/AppState.h"
#include "core/Branding.h"
#include "core/Locale.h"
#include "Version.h"
#include "core/MumbleUtils.h"
#include "ui/BillboardRenderer.h"
#include "ui/DatAssetIconService.h"
#include "ui/MarkersPanel.h"
#include "ui/OptionsPanel.h"
#include "ui/SettingsWindow.h"
#include "ui/MarkerSetEditorWindow.h"
#include "services/RtApiService.h"
#include "ui/QuickAccessService.h"
#include "ui/ScreenMapOverlay.h"
#include "ui/TextureService.h"
#include "utils/InteractBindService.h"

#include <imgui.h>
#include "mumble/Mumble.h"
#include "nexus/Nexus.h"
#include <windows.h>

namespace {

// Short alias so the wrapped literals stay readable. This translation unit is
// outside namespace cm, so the alias is spelled out in full here.
using cm::Locale::T;

AddonAPI_t* g_api = nullptr;

constexpr const char* kToggleBind = "CM_TOGGLE";

void AddonLoad(AddonAPI_t* api);
void AddonUnload();
void AddonRender();
void AddonOptions();

void RefreshDataLinks(cm::AppState& state) {
    if (!g_api) {
        return;
    }

    state.nexusLink = static_cast<NexusLinkData_t*>(g_api->DataLink_Get(DL_NEXUS_LINK));
    state.mumbleLink = static_cast<Mumble::Data*>(g_api->DataLink_Get(DL_MUMBLE_LINK));
    state.playerIdentity =
        static_cast<const Mumble::Identity*>(g_api->DataLink_Get(DL_MUMBLE_LINK_IDENTITY));
}

void OnGameInteract() {
    auto& state = cm::AppState::Instance();
    if (!state.IsReady()) {
        return;
    }

    state.mapWatch.OnInteractKey(state.mumbleLink, state.nexusLink, state.ltMode);
}

void OnToggle(const char*, bool isRelease) {
    if (isRelease) {
        return;
    }

    cm::QuickAccessService::OnShortcutActivated(cm::AppState::Instance());
}

void AddonLoad(AddonAPI_t* api) {
    g_api = api;
    api->Log(LOGL_INFO, cm::kLogChannel, "AddonLoad starting.");

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(api->ImguiContext));
    ImGui::SetAllocatorFunctions(
        reinterpret_cast<void* (*)(size_t, void*)>(api->ImguiMalloc),
        reinterpret_cast<void (*)(void*, void*)>(api->ImguiFree));

    cm::AppState::Instance().PrepareLoad(api);

    api->GUI_Register(RT_Render, AddonRender);
    api->GUI_Register(RT_OptionsRender, AddonOptions);

    Keybind_t defaultBind{};
    api->InputBinds_RegisterWithStruct(kToggleBind, OnToggle, defaultBind);
    cm::InteractBindService::Register(api, OnGameInteract);
    cm::MarkersPanel::RegisterInput(api);

    api->Log(LOGL_INFO, cm::kLogChannel, "Loaded.");
}

void AddonUnload() {
    if (!g_api) {
        return;
    }

    g_api->GUI_Deregister(AddonRender);
    g_api->GUI_Deregister(AddonOptions);
    cm::AppState::Instance().HaltBackgroundWork();
    g_api->InputBinds_Deregister(kToggleBind);
    cm::InteractBindService::Unregister(g_api);
    cm::MarkersPanel::UnregisterInput(g_api);
    cm::QuickAccessService::Unregister(g_api);
    cm::DatAssetIconService::Shutdown();
    cm::TextureService::Shutdown();

    cm::AppState::Instance().Shutdown();
    g_api->Log(LOGL_INFO, cm::kLogChannel, "Unloaded.");
    g_api = nullptr;
}

void AddonRender() {
    if (!g_api) {
        return;
    }

    auto& state = cm::AppState::Instance();
    RefreshDataLinks(state);

    if (!cm::IsWorldReady(state.mumbleLink, state.nexusLink)) {
        return;
    }

    state.ProcessDeferredInit();
    if (!state.IsReady()) {
        return;
    }

    state.ProcessBackgroundNotices();
    cm::RtApiService::Tick();
    state.ProcessPendingCommunitySync();
    state.ProcessPendingMapRefresh();

    state.ProcessRenderInit();
    cm::QuickAccessService::ProcessFrame();
    cm::QuickAccessService::SyncVisibility(g_api, state);

    if (state.mumbleLink && state.nexusLink) {
        state.mapWatch.Update(state.mumbleLink, state.nexusLink, state.ltMode);
    }
    state.placementService.Tick();
    cm::DatAssetIconService::ProcessDownloads();

    // World overlays first (sent to display back), settings window last so it stays on top.
    cm::MarkersPanel::Render(state);
    cm::ScreenMapOverlay::Render(state);
    cm::BillboardRenderer::Render(state);

    if (cm::SettingsWindow::IsOpen()) {
        state.EnsureOptionsReady();
        state.ProcessDeferredInit();
        cm::SettingsWindow::Render(state);
    }

    if (cm::MarkerSetEditorWindow::IsOpen()) {
        state.EnsureOptionsReady();
        cm::MarkerSetEditorWindow::Render(state);
    }
}

void AddonOptions() {
    if (!g_api) {
        return;
    }

    auto& state = cm::AppState::Instance();
    RefreshDataLinks(state);
    state.EnsureOptionsReady();
    if (cm::IsWorldReady(state.mumbleLink, state.nexusLink)) {
        state.ProcessDeferredInit();
    }
    cm::OptionsPanel::RenderNexusConfigEntry(state);
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    static AddonDefinition_t def{};
    static bool initialized = false;
    if (!initialized) {
        def.Signature = cm::kSignature;
        def.APIVersion = NEXUS_API_VERSION;
        def.Name = cm::kDisplayName;
        def.Version = {V_MAJOR, V_MINOR, V_BUILD, V_REVISION};
        def.Author = cm::kAuthor;
        // Translated at query time: the returned pointer lives in the static
        // translation table, so it stays valid for the addon's lifetime.
        def.Description = T(cm::kDescription);
        def.Load = AddonLoad;
        def.Unload = AddonUnload;
        def.Flags = AF_None;
#if defined(CM_LOCAL_TEST)
        def.Provider = UP_None;
        def.UpdateLink = nullptr;
#else
        def.Provider = UP_GitHub;
        def.UpdateLink = cm::kUpdateLink;
#endif
        initialized = true;
    }
    return &def;
}
