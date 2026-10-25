#include "core/SettingsStore.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace cm {

namespace {

PanelLayout LayoutFromInt(int value) {
    switch (value) {
        case 1:
            return PanelLayout::Stacked;
        case 2:
            return PanelLayout::SingleRow;
        case 3:
            return PanelLayout::SingleColumn;
        default:
            return PanelLayout::SideBySide;
    }
}

CornerIconAction CornerActionFromInt(int value) {
    switch (value) {
        case 1:
            return CornerIconAction::ShowSettings;
        case 2:
            return CornerIconAction::Library;
        case 3:
            return CornerIconAction::Lieutenant;
        case 4:
            return CornerIconAction::ClickMarkerToggle;
        default:
            return CornerIconAction::ShowIconMenu;
    }
}

SquadMarker SquadMarkerFromInt(int value) {
    if (value < 0 || value > 9) return SquadMarker::Heart;
    return static_cast<SquadMarker>(value);
}

TriggerMarkerSize TriggerMarkerSizeFromInt(int value) {
    switch (value) {
        case 1:
            return TriggerMarkerSize::Normal;
        case 2:
            return TriggerMarkerSize::Large;
        default:
            return TriggerMarkerSize::Small;
    }
}

}  // namespace

void SettingsStore::Load(const std::string& filePath) {
    std::ifstream in(filePath);
    if (!in.is_open()) return;

    nlohmann::json j;
    in >> j;

    if (j.contains("CmdMrkShowMarkerPanelr")) showMarkerPanel = j["CmdMrkShowMarkerPanelr"].get<bool>();
    if (j.contains("CmdMrkOnlyCommander")) onlyCommanderPanel = j["CmdMrkOnlyCommander"].get<bool>();
    if (j.contains("CmdMrkDrag")) dragPanel = j["CmdMrkDrag"].get<bool>();
    if (j.contains("CmdMrkLoc")) {
        panelLocation.x = j["CmdMrkLoc"]["X"].get<float>();
        panelLocation.y = j["CmdMrkLoc"]["Y"].get<float>();
    }
    if (j.contains("CmdMrkOrientation2")) {
        panelOrientation = LayoutFromInt(j["CmdMrkOrientation2"].get<int>());
    }
    if (j.contains("CmdMrkImgWidth")) iconWidth = j["CmdMrkImgWidth"].get<int>();
    if (j.contains("CmdMrkOpacity")) panelOpacity = j["CmdMrkOpacity"].get<float>();
    if (j.contains("CmdMrkGnnEnabled")) groundMarkersEnabled = j["CmdMrkGnnEnabled"].get<bool>();
    if (j.contains("CmdMrkTgtEnabled")) objectMarkersEnabled = j["CmdMrkTgtEnabled"].get<bool>();

    if (j.contains("CmdMrkAMEnabled")) autoMarkerEnabled = j["CmdMrkAMEnabled"].get<bool>();
    if (j.contains("CmdMrkAMOnlyCommander")) autoMarkerOnlyCommander = j["CmdMrkAMOnlyCommander"].get<bool>();
    if (j.contains("CmdMrkAMShowPreview")) autoMarkerShowPreview = j["CmdMrkAMShowPreview"].get<bool>();
    if (j.contains("CmdMrkAMShowTrigger")) autoMarkerShowTrigger = j["CmdMrkAMShowTrigger"].get<bool>();
    if (j.contains("CmdMrkBillboardEnabled")) billboardEnabled = j["CmdMrkBillboardEnabled"].get<bool>();
    if (j.contains("CmdMrkAMCanBypassMapOpen")) billboardPlacement = j["CmdMrkAMCanBypassMapOpen"].get<bool>();
    if (j.contains("CmdMrkBillboardPreview")) billboardPreview = j["CmdMrkBillboardPreview"].get<bool>();
    if (j.contains("CmdMrkTriggerMarkerSize")) {
        triggerMarkerSize = TriggerMarkerSizeFromInt(j["CmdMrkTriggerMarkerSize"].get<int>());
    }
    if (j.contains("CmdMrkCombatPlacement")) combatPlacement = j["CmdMrkCombatPlacement"].get<bool>();
    if (j.contains("CmdMrkPlacementDelay")) placementDelayMs = j["CmdMrkPlacementDelay"].get<int>();
    if (j.contains("CmdMrkAMLibraryFilter")) libraryFilterCurrentMap = j["CmdMrkAMLibraryFilter"].get<bool>();
    if (j.contains("CmdMrkAMLibraryFilterMine")) {
        libraryFilterMine = j["CmdMrkAMLibraryFilterMine"].get<bool>();
    }
    if (j.contains("CmdMrkAMCommunityShowAvailable")) {
        communityLibraryShowAvailable = j["CmdMrkAMCommunityShowAvailable"].get<bool>();
    }
    if (j.contains("CmdMrkAMEditorConfirmUnsaved")) {
        libraryEditorConfirmUnsaved = j["CmdMrkAMEditorConfirmUnsaved"].get<bool>();
    }

    if (j.contains("CmdMrkCornerIconEnabled")) cornerIconEnabled = j["CmdMrkCornerIconEnabled"].get<bool>();
    if (j.contains("CmdMrkAMCornerIconAction")) {
        cornerIconAction = CornerActionFromInt(j["CmdMrkAMCornerIconAction"].get<int>());
    }
    if (j.contains("CmdMrkCornerTexture")) {
        cornerTexture = SquadMarkerFromInt(j["CmdMrkCornerTexture"].get<int>());
    }
}

void SettingsStore::Save(const std::string& filePath) const {
    nlohmann::json j = {
        {"CmdMrkShowMarkerPanelr", showMarkerPanel},
        {"CmdMrkOnlyCommander", onlyCommanderPanel},
        {"CmdMrkDrag", dragPanel},
        {"CmdMrkLoc", {{"X", panelLocation.x}, {"Y", panelLocation.y}}},
        {"CmdMrkOrientation2", static_cast<int>(panelOrientation)},
        {"CmdMrkImgWidth", iconWidth},
        {"CmdMrkOpacity", panelOpacity},
        {"CmdMrkGnnEnabled", groundMarkersEnabled},
        {"CmdMrkTgtEnabled", objectMarkersEnabled},
        {"CmdMrkAMEnabled", autoMarkerEnabled},
        {"CmdMrkAMOnlyCommander", autoMarkerOnlyCommander},
        {"CmdMrkAMShowPreview", autoMarkerShowPreview},
        {"CmdMrkAMShowTrigger", autoMarkerShowTrigger},
        {"CmdMrkBillboardEnabled", billboardEnabled},
        {"CmdMrkAMCanBypassMapOpen", billboardPlacement},
        {"CmdMrkBillboardPreview", billboardPreview},
        {"CmdMrkTriggerMarkerSize", static_cast<int>(triggerMarkerSize)},
        {"CmdMrkCombatPlacement", combatPlacement},
        {"CmdMrkPlacementDelay", placementDelayMs},
        {"CmdMrkAMLibraryFilter", libraryFilterCurrentMap},
        {"CmdMrkAMLibraryFilterMine", libraryFilterMine},
        {"CmdMrkAMCommunityShowAvailable", communityLibraryShowAvailable},
        {"CmdMrkAMEditorConfirmUnsaved", libraryEditorConfirmUnsaved},
        {"CmdMrkCornerIconEnabled", cornerIconEnabled},
        {"CmdMrkAMCornerIconAction", static_cast<int>(cornerIconAction)},
        {"CmdMrkCornerTexture", static_cast<int>(cornerTexture)},
    };

    std::filesystem::path path(filePath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream out(filePath);
    out << j.dump(2);
}

}  // namespace cm
