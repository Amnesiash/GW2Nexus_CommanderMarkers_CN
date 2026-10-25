#include "ui/SettingsWindow.h"

#include "core/AppState.h"
#include "core/Locale.h"
#include "ui/OptionsPanel.h"
#include "ui/MarkerSetEditorWindow.h"

#include "nexus/Nexus.h"

#include <imgui.h>

#include <string>

namespace cm {
namespace SettingsWindow {
namespace {

// Short alias so the wrapped literals stay readable.
using cm::Locale::T;

// ImGui splits the window name on "###": everything before it is the visible
// title, everything after it is the immutable window ID. The ID half must stay
// identical across languages so the Nexus escape registration (below) keeps
// matching the same window.
constexpr const char* kWindowIdSuffix = "###CM_SETTINGS_WINDOW";

// Escape registration is keyed by the original, untranslated window ID.
constexpr const char* kWindowId = "Commander Marker CN Settings###CM_SETTINGS_WINDOW";

bool open_ = false;
bool escapeRegistered_ = false;
SettingsTab pendingTab_ = SettingsTab::ClickableMarkers;
bool pendingTabSet_ = false;

void RegisterCloseOnEscape(AddonAPI_t* api) {
    if (!api || !api->GUI_RegisterCloseOnEscape || escapeRegistered_) {
        return;
    }
    api->GUI_RegisterCloseOnEscape(kWindowId, &open_);
    escapeRegistered_ = true;
}

void DeregisterCloseOnEscape(AddonAPI_t* api) {
    if (!api || !api->GUI_DeregisterCloseOnEscape || !escapeRegistered_) {
        return;
    }
    api->GUI_DeregisterCloseOnEscape(kWindowId);
    escapeRegistered_ = false;
}

void SyncEscapeRegistration(AddonAPI_t* api) {
    if (open_) {
        RegisterCloseOnEscape(api);
    } else {
        DeregisterCloseOnEscape(api);
    }
}

void HandleEscapeKey() {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
        open_ = false;
    }
}

}  // namespace

void Open(SettingsTab tab) {
    open_ = true;
    pendingTab_ = tab;
    pendingTabSet_ = true;
    RegisterCloseOnEscape(AppState::Instance().api);
}

void Close() {
    open_ = false;
    DeregisterCloseOnEscape(AppState::Instance().api);
}

void Toggle(SettingsTab tab) {
    if (open_) {
        Close();
        return;
    }
    Open(tab);
}

bool IsOpen() { return open_; }

void Shutdown(AddonAPI_t* api) {
    DeregisterCloseOnEscape(api);
    MarkerSetEditorWindow::Close();
    open_ = false;
    pendingTabSet_ = false;
}

void Render(AppState& state) {
    if (!open_) {
        return;
    }

    RegisterCloseOnEscape(state.api);

    ImGui::SetNextWindowSize(ImVec2(660.0f, 680.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(660.0f, 680.0f), ImVec2(9999.0f, 9999.0f));

    // Localised title with the immutable ID appended, so the window ID (and the
    // escape registration that keys on it) is unaffected by the active language.
    const std::string windowName = std::string(T("Commander Marker CN Settings")) + kWindowIdSuffix;
    if (!ImGui::Begin(windowName.c_str(), &open_, ImGuiWindowFlags_None)) {
        ImGui::End();
        SyncEscapeRegistration(state.api);
        return;
    }

    OptionsPanel::RenderWindow(state, pendingTab_, pendingTabSet_);
    if (pendingTabSet_) {
        pendingTabSet_ = false;
    }

    HandleEscapeKey();
    ImGui::End();
    SyncEscapeRegistration(state.api);
}

}  // namespace SettingsWindow
}  // namespace cm
