#include "ui/SettingsWindow.h"

#include "core/AppState.h"
#include "core/Branding.h"
#include "data/MarkerDefinition.h"
#include "services/RtApiService.h"
#include "ui/CommunityLibraryUi.h"
#include "ui/LibraryEditorUi.h"
#include "ui/OptionsApiTab.h"
#include "ui/MarkersPanel.h"
#include "ui/OptionsUiKit.h"
#include "Version.h"
#include "core/Locale.h"

#include <imgui.h>
#include <shellapi.h>
#include <vector>

namespace cm {
namespace OptionsPanel {
namespace {

// Short alias so the wrapped literals stay readable.
using cm::Locale::T;


void OpenExternalUrl(const char* url) {
    if (!url || url[0] == '\0') {
        return;
    }
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

void StatusIndicator(const char* label, bool ok, const char* okText, const char* noText) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    if (ok) {
        ImGui::TextColored(ImVec4(0.45f, 0.95f, 0.45f, 1.0f), "%s", okText);
    } else {
        ImGui::TextDisabled("%s", noText);
    }
}

ImGuiTabItemFlags PendingTabFlags(SettingsTab tab, SettingsTab pendingTab, bool applyPendingTab) {
    if (applyPendingTab && tab == pendingTab) {
        return ImGuiTabItemFlags_SetSelected;
    }
    return ImGuiTabItemFlags_None;
}

void RenderAboutTab() {
    using namespace OptionsUiKit;

    SectionHeading(T("About"));
    ImGui::TextWrapped(
        T("%s is a Nexus port of the BlishHUD Commander Markers module. Place squad markers with a click, or auto-place saved marker sets near triggers."),
        kDisplayName);

    ImGui::Spacing();
    ImGui::Text(T("Version: %s"), V_VERSION_STRING);
    ImGui::Text(T("Author: %s"), kAuthor);

    ImGui::Spacing();
    if (ImGui::Button(T("Patch Notes"))) {
        OpenExternalUrl(kPatchNotesUrl);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(T("Open patch notes in your default web browser."));
    }

    SectionHeading(T("Thank you to:"));
    ImGui::BulletText(T("Freesnöw - continued static data and config file hosting"));
    ImGui::BulletText(T("Manlaan - original clickable markers in BlishHUD"));

    SectionHeading(T("Links"));
    ImGui::TextWrapped(T("Original module:"));
    ImGui::TextWrapped("%s", kSourceRepoUrl);
    ImGui::Spacing();
    ImGui::TextWrapped(T("Community marker library:"));
    ImGui::TextWrapped("%s", kCommunityMarkersUrl);

    SectionHeading(T("Integrations"));
    SectionSubtext(T("Optional co-modules detected at runtime."));
    const bool rtapiDetected = RtApiService::IsDetected();
    const bool rtapiActive = RtApiService::IsActive();
    StatusIndicator(T("Real-Time API detected:"), rtapiDetected, T("Yes"), T("No"));
    StatusIndicator(T("Real-Time API active:"), rtapiActive, T("Yes"), T("No"));
    if (!rtapiDetected) {
        SectionSubtext(
            T("Install Real-Time API from the Nexus addon library to enable squad commander/lieutenant permission detection."));
    } else if (!rtapiActive) {
        SectionSubtext(T("Real-Time API is installed but not currently loaded or ready."));
    } else {
        SectionSubtext(
            T("Squad commander and lieutenant roles are available for commander permission checks."));
    }
}

void RenderClickableMarkersTab(AppState& state) {
    using namespace OptionsUiKit;

    SectionHeading(T("Clickable Markers"));
    SectionSubtext(T("Configure the on-screen squad marker panel."));

    SettingCheckbox(T("Show clickable markers on screen"), &state.settings.showMarkerPanel);
    SettingCheckbox(T("Only when commander"), &state.settings.onlyCommanderPanel);
    SettingCheckbox(T("Drag panel to reposition"), &state.settings.dragPanel);
    SettingCheckbox(T("Ground markers enabled"), &state.settings.groundMarkersEnabled);
    SettingCheckbox(T("Object markers enabled"), &state.settings.objectMarkersEnabled);

    SettingSliderInt(T("Icon width"), &state.settings.iconWidth, 16, 64);
    SettingSliderFloat(T("Panel opacity"), &state.settings.panelOpacity, 0.2f, 1.0f);

    const char* layoutLabels[] = {T("Stacked"), T("Side-by-side"), T("Single row"),
                                  T("Single column")};
    static constexpr PanelLayout kLayoutOrder[] = {PanelLayout::Stacked, PanelLayout::SideBySide,
                                                   PanelLayout::SingleRow,
                                                   PanelLayout::SingleColumn};

    int layoutIndex = 0;
    for (int i = 0; i < 4; ++i) {
        if (kLayoutOrder[i] == state.settings.panelOrientation) {
            layoutIndex = i;
            break;
        }
    }
    if (SettingCombo(T("Panel orientation"), &layoutIndex, layoutLabels, 4)) {
        state.settings.panelOrientation = kLayoutOrder[layoutIndex];
    }

    ImGui::Spacing();
    SectionSubtext(T("Preview"));
    if (state.IsReady()) {
        MarkersPanel::Render(state, true);
    } else {
        DisabledGateText(T("Preview available after entering gameplay."));
    }
}

void RenderAutoMarkerSettingsTab(AppState& state) {
    using namespace OptionsUiKit;

    SectionHeading(T("AutoMarker Settings"));
    SectionSubtext(T("Allows for rapidly placing saved marker sets."));

    SettingCheckbox(T("Enable"), &state.settings.autoMarkerEnabled,
                    T("Enable/Disable the entire AutoMarker feature."));
    SettingCheckbox(T("Only show when I am the Commander"), &state.settings.autoMarkerOnlyCommander,
                    T("Only show the AutoMarker activation zones on the map when you are the Commander."));

    SectionHeading(T("Map Icons"));
    SettingCheckbox(T("Enable Map Marker"), &state.settings.autoMarkerShowTrigger,
                    T("Display the map marker icon in locations where AutoMarker sets may be activated from the map."));
    SettingCheckbox(T("Show preview when map is open"), &state.settings.autoMarkerShowPreview,
                    T("Show a preview of the markers when the map is open and you are close enough to place the set."));

    SectionHeading(T("In Game World Icons"));
    SettingCheckbox(T("Enable markers in 3D game world"), &state.settings.billboardEnabled,
                    T("Show markers in the 3D game world."));
    SettingCheckbox(T("Allow placement without having the map open"),
                    &state.settings.billboardPlacement,
                    T("Allow the marker placement even when the map is closed."));
    SettingCheckbox(T("Preview marker set when near trigger"), &state.settings.billboardPreview,
                    T("Show a preview of the markers to be placed in the game world."));

    const char* triggerMarkerSizes[] = {T("Small"), T("Normal"), T("Large")};
    int triggerSizeIndex = static_cast<int>(state.settings.triggerMarkerSize);
    if (SettingCombo(T("In-world trigger marker size"), &triggerSizeIndex, triggerMarkerSizes, 3,
                     T("Size of the trigger marker icon shown in the 3D game world."))) {
        state.settings.triggerMarkerSize = static_cast<TriggerMarkerSize>(triggerSizeIndex);
    }

    SettingCheckbox(T("Allow AutoMarker features while In Combat"), &state.settings.combatPlacement,
                    T("AutoMarker features will be available while in combat."));
    SettingSliderInt(T("Placement delay"), &state.settings.placementDelayMs, 25, 500,
                     T("Delay in milliseconds to wait between marker placement."));

    SectionHeading(T("Library Editor"));
    SettingCheckbox(T("Confirm before closing with unsaved changes"),
                    &state.settings.libraryEditorConfirmUnsaved,
                    T("Prompt to save or discard when closing the marker set editor."));
}

void RenderGeneralTab(AppState& state) {
    using namespace OptionsUiKit;

    SectionHeading(T("General"));

    if (!state.requiredBindsOk) {
        WarningBanner(
            T("One or more squad marker keybinds are missing in Guild Wars 2. Configure ground and object marker binds under Squad Markers in game options."));
    }

    SettingCheckbox(T("Quick access icon enabled"), &state.settings.cornerIconEnabled);

    // Labels are rebuilt each frame so switching language applies immediately.
    std::vector<const char*> cornerActionLabels;
    for (const char* text : {"Show icon menu", "Show settings", "Open library",
                             "Toggle lieutenant mode", "Toggle clickable markers"}) {
        cornerActionLabels.push_back(T(text));
    }
    int actionIndex = static_cast<int>(state.settings.cornerIconAction);
    if (SettingCombo(T("Quick access icon left-click action"), &actionIndex,
                     cornerActionLabels.data(), static_cast<int>(cornerActionLabels.size()))) {
        state.settings.cornerIconAction = static_cast<CornerIconAction>(actionIndex);
    }

    std::vector<const char*> markerLabels;
    std::vector<int> markerValues;
    for (std::size_t i = 0; i < kMarkerDefinitionCount; ++i) {
        if (kAllMarkers[i].markerType == SquadMarker::None ||
            kAllMarkers[i].markerType == SquadMarker::Clear) {
            continue;
        }
        markerLabels.push_back(kAllMarkers[i].displayName);
        markerValues.push_back(static_cast<int>(kAllMarkers[i].markerType));
    }

    int textureIndex = 0;
    for (size_t i = 0; i < markerValues.size(); ++i) {
        if (markerValues[i] == static_cast<int>(state.settings.cornerTexture)) {
            textureIndex = static_cast<int>(i);
            break;
        }
    }

    if (SettingCombo(T("Quick access icon texture"), &textureIndex, markerLabels.data(),
                     static_cast<int>(markerLabels.size()))) {
        state.settings.cornerTexture = static_cast<SquadMarker>(markerValues[textureIndex]);
    }

    // Interface language sits last among the general settings, without a section
    // heading of its own. Labels are never translated: picking a language must
    // stay readable whatever the current language is.
    static constexpr const char* kLanguageLabels[] = {"跟随 Nexus", "简体中文", "English"};
    int languageIndex = Locale::ToSettingsValue(Locale::Current());
    if (SettingCombo(T("Interface language"), &languageIndex, kLanguageLabels, 3)) {
        const Locale::Language chosen = Locale::FromSettingsValue(languageIndex);
        Locale::SetLanguage(chosen);
        state.settings.language = Locale::ToSettingsValue(chosen);
    }
    SectionSubtext(T("Follow Nexus uses the language Nexus itself is running. Interface text needs a Nexus restart to fully refresh."));

    ImGui::Spacing();
    OptionsApiTab::Render(state);
}

}  // namespace

void RenderNexusConfigEntry(AppState& state) {
    using namespace OptionsUiKit;

    (void)state;

    ImGui::TextColored(GoldColor(), "%s", kDisplayName);
    ImGui::Spacing();
    ImGui::TextWrapped("%s", T(kDescription));
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    const ImVec2 buttonSize(ImGui::GetContentRegionAvail().x, 52.0f);
    if (ImGui::Button(T("Open Commander Marker CN Settings"), buttonSize)) {
        SettingsWindow::Open();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(T("Opens the full settings window."));
    }
}

void RenderWindow(AppState& state, SettingsTab pendingTab, bool applyPendingTab) {
    using namespace OptionsUiKit;

    ImGui::TextColored(GoldColor(), kDisplayName);
    ImGui::Separator();
    ImGui::Spacing();

    if (!state.IsReady()) {
        DisabledGateText(T("Enter gameplay to load marker libraries and previews."));
    }

    if (ImGui::BeginTabBar("##cm_options_tabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem(T("Clickable Markers"), nullptr,
                                PendingTabFlags(SettingsTab::ClickableMarkers, pendingTab,
                                                applyPendingTab))) {
            BeginContentPanel();
            RenderClickableMarkersTab(state);
            EndContentPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(T("AutoMarker Settings"), nullptr,
                                PendingTabFlags(SettingsTab::AutoMarkerSettings, pendingTab,
                                                applyPendingTab))) {
            BeginContentPanel();
            RenderAutoMarkerSettingsTab(state);
            EndContentPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(T("AutoMarker Library"), nullptr,
                                PendingTabFlags(SettingsTab::AutoMarkerLibrary, pendingTab,
                                                applyPendingTab))) {
            BeginContentPanel();
            if (state.IsReady()) {
                LibraryEditorUi::Render(state);
            } else {
                DisabledGateText(T("Available after entering gameplay."));
            }
            EndContentPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(T("Community Library"), nullptr,
                                PendingTabFlags(SettingsTab::CommunityLibrary, pendingTab,
                                                applyPendingTab))) {
            BeginContentPanel();
            if (state.IsReady()) {
                CommunityLibraryUi::Render(state);
            } else {
                DisabledGateText(T("Available after entering gameplay."));
            }
            EndContentPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(T("General"), nullptr,
                                PendingTabFlags(SettingsTab::General, pendingTab,
                                                applyPendingTab))) {
            BeginContentPanel();
            RenderGeneralTab(state);
            EndContentPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(T("About"), nullptr,
                                PendingTabFlags(SettingsTab::About, pendingTab, applyPendingTab))) {
            BeginContentPanel();
            RenderAboutTab();
            EndContentPanel();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (state.IsOptionsReady()) {
        state.settings.Save(state.settingsPath());
    }
}

}  // namespace OptionsPanel
}  // namespace cm
