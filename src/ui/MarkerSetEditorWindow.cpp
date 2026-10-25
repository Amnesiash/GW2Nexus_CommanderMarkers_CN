#include "ui/MarkerSetEditorWindow.h"

#include "core/AppState.h"
#include "core/MumbleUtils.h"
#include "data/MarkerSetClipboard.h"
#include "data/MarkerSetJson.h"
#include "services/MarkerListing.h"
#include "utils/UuidUtils.h"
#include "services/RtApiService.h"
#include "ui/MapPickerUi.h"
#include "ui/TextureService.h"
#include "services/MapWatchService.h"
#include "ui/OptionsUiKit.h"
#include "core/Locale.h"

#include <cctype>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace cm {
namespace MarkerSetEditorWindow {
namespace {

// Short alias so the wrapped literals stay readable.
using cm::Locale::T;


// ImGui splits a window name on "###": the part before it is the visible title,
// the part after it is the immutable window ID. Titles are translated, IDs are
// not, so window identity survives a language switch.
constexpr const char* kWindowId = "###CM_MARKER_SET_EDITOR";
constexpr const char* kUnsavedDialogId = "###CM_MARKER_UNSAVED";
constexpr const char* kDeleteDialogId = "###CM_MARKER_DELETE";

// Composes a localised title with its immutable ID suffix.
std::string MakeWindowName(const char* title, const char* id) {
    return std::string(T(title)) + id;
}

constexpr int kMarkerSlotCount = 8;

struct MarkerSlot {
    bool set = false;
    std::string name;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct EditorState {
    bool open = false;
    int listingIndex = -1;
    MarkerSet draft{};
    MarkerSet baseline{};
    MarkerSlot slots[kMarkerSlotCount]{};
    bool hasBaseline = false;
    std::string importBuffer;
    std::string importError;
    std::string validationError;
    bool previewActive = false;
    bool showUnsavedDialog = false;
    bool showDeleteDialog = false;
};

EditorState g_state;

void StopPreview(AppState& state) {
    if (g_state.previewActive) {
        state.mapWatch.RemovePreviewMarkerSet();
        g_state.previewActive = false;
    }
}

bool MarkersEqual(const MarkerCoord& a, const MarkerCoord& b) {
    return a.icon == b.icon && a.name == b.name && a.x == b.x && a.y == b.y && a.z == b.z;
}

bool MarkerSetsDeepEqual(const MarkerSet& a, const MarkerSet& b) {
    if (a.name != b.name || a.description != b.description || a.mapId != b.mapId ||
        a.enabled != b.enabled) {
        return false;
    }

    if (a.trigger.x != b.trigger.x || a.trigger.y != b.trigger.y ||
        a.trigger.z != b.trigger.z) {
        return false;
    }

    if (a.markers.size() != b.markers.size()) {
        return false;
    }

    for (size_t i = 0; i < a.markers.size(); ++i) {
        if (!MarkersEqual(a.markers[i], b.markers[i])) {
            return false;
        }
    }

    return true;
}

void ClearSlot(MarkerSlot& slot) {
    slot = {};
}

bool SlotHasFiniteCoords(const MarkerSlot& slot) {
    return std::isfinite(slot.x) && std::isfinite(slot.y) && std::isfinite(slot.z);
}

void SanitizeSlot(MarkerSlot& slot) {
    if (!SlotHasFiniteCoords(slot)) {
        slot.x = 0.0f;
        slot.y = 0.0f;
        slot.z = 0.0f;
        slot.set = false;
    }
}

void SyncDraftFromSlots() {
    g_state.draft.markers.clear();
    for (int slotIndex = 0; slotIndex < kMarkerSlotCount; ++slotIndex) {
        const MarkerSlot& slot = g_state.slots[slotIndex];
        if (!slot.set || !SlotHasFiniteCoords(slot)) {
            continue;
        }

        MarkerCoord marker{};
        marker.icon = slotIndex + 1;
        marker.name = slot.name;
        marker.x = slot.x;
        marker.y = slot.y;
        marker.z = slot.z;
        g_state.draft.markers.push_back(marker);
    }
}

void LoadSlotsFromDraft() {
    for (MarkerSlot& slot : g_state.slots) {
        slot = {};
    }

    for (const MarkerCoord& marker : g_state.draft.markers) {
        if (marker.icon < 1 || marker.icon > kMarkerSlotCount) {
            continue;
        }

        MarkerSlot& slot = g_state.slots[marker.icon - 1];
        slot.set = true;
        slot.name = marker.name;
        slot.x = marker.x;
        slot.y = marker.y;
        slot.z = marker.z;
        SanitizeSlot(slot);
    }
}

bool IsDirty() {
    if (!g_state.hasBaseline) {
        return false;
    }

    SyncDraftFromSlots();
    return !MarkerSetsDeepEqual(g_state.draft, g_state.baseline);
}

void SetBaselineFromDraft() {
    SyncDraftFromSlots();
    g_state.baseline = g_state.draft;
    g_state.hasBaseline = true;
}

void ApplyPlayerPosition(AppState& state, WorldCoord& coord, int* mapId = nullptr) {
    if (!state.mumbleLink) {
        return;
    }

    const Vec3f position = PlayerPosition(state.mumbleLink);
    coord.x = position.x;
    coord.y = position.y;
    coord.z = position.z;
    if (mapId) {
        *mapId = static_cast<int>(state.mumbleLink->Context.MapID);
    }
}

void ApplyPlayerPosition(AppState& state, MarkerSlot& slot) {
    if (!state.mumbleLink) {
        return;
    }

    const Vec3f position = PlayerPosition(state.mumbleLink);
    slot.set = true;
    slot.x = position.x;
    slot.y = position.y;
    slot.z = position.z;
}

bool TriggerIsSet(const WorldCoord& trigger) {
    if (!std::isfinite(trigger.x) || !std::isfinite(trigger.y) || !std::isfinite(trigger.z)) {
        return false;
    }

    return std::fabs(trigger.x) > 0.01f || std::fabs(trigger.y) > 0.01f ||
           std::fabs(trigger.z) > 0.01f;
}

std::string TrimCopy(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::optional<std::string> ValidateDraft(const AppState& state) {
    SyncDraftFromSlots();

    if (TrimCopy(g_state.draft.name).empty()) {
        return T("Name is required.");
    }

    if (g_state.draft.mapId <= 0) {
        return T("Map is invalid. Set trigger location while on a map.");
    }

    if (!TriggerIsSet(g_state.draft.trigger)) {
        return T("Trigger location is required. Use Set Trigger Location.");
    }

    if (g_state.draft.markers.empty()) {
        return T("At least one marker with a position is required.");
    }

    for (const MarkerCoord& marker : g_state.draft.markers) {
        if (!std::isfinite(marker.x) || !std::isfinite(marker.y) || !std::isfinite(marker.z)) {
            return T("One or more markers have invalid coordinates.");
        }
    }

    (void)state;
    return std::nullopt;
}

constexpr float kCoordWidth = 72.0f;
constexpr float kIconColumnWidth = 36.0f;
constexpr float kClearColumnWidth = 28.0f;
constexpr float kDeleteButtonSize = 22.0f;

float CalcActionButtonWidth() {
    const ImGuiStyle& style = ImGui::GetStyle();
    return style.FramePadding.x * 2.0f + ImGui::CalcTextSize(T("Set Location")).x;
}

float CalcMarkerFieldsWidth(float coordWidth) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float labelPad = style.ItemInnerSpacing.x;
    const float labelX = ImGui::CalcTextSize("X").x + labelPad;
    const float labelY = ImGui::CalcTextSize("Y").x + labelPad;
    const float labelZ = ImGui::CalcTextSize("Z").x + labelPad;
    return labelX + coordWidth + style.ItemSpacing.x + labelY + coordWidth + style.ItemSpacing.x +
           labelZ + coordWidth;
}

float CalcMarkerRowHeight() {
    const ImGuiStyle& style = ImGui::GetStyle();
    return ImGui::GetFrameHeight() * 2.0f + style.ItemSpacing.y;
}

float CalcEditorMinWidth() {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float fieldsWidth = CalcMarkerFieldsWidth(kCoordWidth);
    const float actionButtonWidth = CalcActionButtonWidth();
    const float tableWidth = kIconColumnWidth + fieldsWidth + kClearColumnWidth + actionButtonWidth +
                               style.CellPadding.x * 8.0f;
    const float metaRowWidth = fieldsWidth + style.ItemSpacing.x + 24.0f;
    return (std::max(tableWidth, metaRowWidth) + style.WindowPadding.x * 2.0f) + 5.0f;
}

bool SlotIsActive(const MarkerSlot& slot) {
    return slot.set && SlotHasFiniteCoords(slot);
}

bool ShiftModifierHeld() { return ImGui::GetIO().KeyShift; }

void CenterCursorInCell(float columnWidth, float contentWidth, float rowHeight,
                        float contentHeight) {
    const ImVec2 cellPos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cellPos.x + (columnWidth - contentWidth) * 0.5f,
                               cellPos.y + (rowHeight - contentHeight) * 0.5f));
}

void ItemTooltip(const char* tooltip) {
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
}

bool InputTextHint(const char* label, char* buf, size_t bufSize, const char* hint, float width) {
    ImGui::SetNextItemWidth(width);
    return ImGui::InputTextWithHint(label, hint, buf, bufSize);
}

bool InputOptionalFloat(const char* label, const char* hint, float& value, bool active,
                        float width) {
    ImGui::SetNextItemWidth(width);
    if (active) {
        return ImGui::InputFloat(label, &value, 0.0f, 0.0f, "%.3f");
    }

    char buf[32] = "";
    if (!ImGui::InputTextWithHint(label, hint, buf, sizeof(buf),
                                  ImGuiInputTextFlags_CharsDecimal)) {
        return false;
    }

    if (buf[0] == '\0') {
        return false;
    }

    value = std::strtof(buf, nullptr);
    return true;
}

bool DrawDeleteButton() {
    return OptionsUiKit::IconButton("clear_marker", TextureService::GetUiTexture("iconDelete"),
                                    kDeleteButtonSize, nullptr);
}

bool ImportSlotFromRtApi(int slotIndex, MarkerSlot& slot) {
    MarkerCoord marker{};
    if (!RtApiService::ImportSquadMarkerPosition(slotIndex, marker)) {
        return false;
    }

    slot.set = true;
    slot.x = marker.x;
    slot.y = marker.y;
    slot.z = marker.z;
    SanitizeSlot(slot);
    return slot.set;
}

void ImportAllSlotsFromRtApi() {
    for (int slotIndex = 0; slotIndex < kMarkerSlotCount; ++slotIndex) {
        ImportSlotFromRtApi(slotIndex, g_state.slots[slotIndex]);
    }
}

bool InputWorldCoord(const char* label, WorldCoord& coord) {
    bool changed = false;
    if (TriggerIsSet(coord)) {
        changed = ImGui::InputFloat3(label, &coord.x);
    } else {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        changed |= InputOptionalFloat("X##trigger", "X", coord.x, false, kCoordWidth);
        ImGui::SameLine();
        changed |= InputOptionalFloat("Y##trigger", "Y", coord.y, false, kCoordWidth);
        ImGui::SameLine();
        changed |= InputOptionalFloat("Z##trigger", "Z", coord.z, false, kCoordWidth);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(T("Trigger world position (X east/west, Y north/south, Z height)."));
    }
    return changed;
}

void SaveDraft(AppState& state) {
    SyncDraftFromSlots();
    if (g_state.listingIndex >= 0) {
        state.markerListing.EditMarker(static_cast<size_t>(g_state.listingIndex), g_state.draft);
    } else {
        state.markerListing.SaveMarker(g_state.draft);
    }
}

bool TrySaveDraft(AppState& state) {
    if (const std::optional<std::string> error = ValidateDraft(state)) {
        g_state.validationError = *error;
        return false;
    }

    g_state.validationError.clear();
    if (g_state.draft.id.empty()) {
        g_state.draft.id = GenerateUuidV4();
    }
    if (g_state.draft.source.empty()) {
        g_state.draft.source = "custom";
    }
    SaveDraft(state);
    return true;
}

void ForceClose(AppState& state) {
    StopPreview(state);
    g_state.open = false;
    g_state.listingIndex = -1;
    g_state.importBuffer.clear();
    g_state.importError.clear();
    g_state.validationError.clear();
    g_state.showUnsavedDialog = false;
    g_state.showDeleteDialog = false;
    g_state.hasBaseline = false;
}

void RequestClose(AppState& state) {
    if (!state.settings.libraryEditorConfirmUnsaved || !IsDirty()) {
        ForceClose(state);
        return;
    }

    g_state.showUnsavedDialog = true;
}

void RenderDisabledRtApiButton(const char* id, const char* label, const char* iconName,
                               const char* tooltip, const ImVec2& size = ImVec2(-1.0f, 0.0f)) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.45f);
    OptionsUiKit::TexturedButton(id, label, TextureService::GetUiTexture(iconName), 16.0f, size);
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s", tooltip);
    }
}

void RenderMarkerTable(AppState& state) {
    const bool rtapiAvailable = RtApiService::IsActive();

    if (rtapiAvailable) {
        if (OptionsUiKit::TexturedButton("import_all", " Import active squad markers",
                                         TextureService::GetUiTexture("iconImport"))) {
            ImportAllSlotsFromRtApi();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Copy currently placed squad marker locations from the Real-Time API.");
        }
    } else {
        RenderDisabledRtApiButton("import_all_disabled", T("Import active squad markers"),
                                  "iconImport", "Requires the Real-Time API addon.");
    }

    ImGui::Spacing();

    const float fieldsWidth = CalcMarkerFieldsWidth(kCoordWidth);
    const float actionButtonWidth = CalcActionButtonWidth();
    constexpr float kIconSize = 28.0f;
    const ImVec2 actionButtonSize(-1.0f, 0.0f);
    const float rowHeight = CalcMarkerRowHeight();

    if (ImGui::BeginTable("##cm_marker_table", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("##icon", ImGuiTableColumnFlags_WidthFixed, kIconColumnWidth);
        ImGui::TableSetupColumn("##fields", ImGuiTableColumnFlags_WidthFixed, fieldsWidth);
        ImGui::TableSetupColumn("##clear", ImGuiTableColumnFlags_WidthFixed, kClearColumnWidth);
        ImGui::TableSetupColumn("##commands", ImGuiTableColumnFlags_WidthFixed, actionButtonWidth);

        for (int slotIndex = 0; slotIndex < kMarkerSlotCount; ++slotIndex) {
            ImGui::TableNextRow();
            ImGui::PushID(slotIndex);

            const SquadMarker markerType = static_cast<SquadMarker>(slotIndex + 1);
            MarkerSlot& slot = g_state.slots[slotIndex];
            const bool active = SlotIsActive(slot);

            if (!active) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                       ImGui::GetColorU32(ImVec4(0.05f, 0.05f, 0.05f, 0.45f)));
            }

            ImGui::TableSetColumnIndex(0);
            CenterCursorInCell(kIconColumnWidth, kIconSize, rowHeight, kIconSize);
            const ImTextureID icon = active ? TextureService::GetTexture(markerType) :
                                              TextureService::GetFadedTexture(markerType);
            if (icon) {
                ImGui::Image(icon, ImVec2(kIconSize, kIconSize));
            } else {
                ImGui::Dummy(ImVec2(kIconSize, kIconSize));
            }

            ImGui::TableSetColumnIndex(1);
            if (!active) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.55f);
            }

            char nameBuf[96];
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", slot.name.c_str());
            if (InputTextHint("##desc", nameBuf, sizeof(nameBuf), T("Label"), fieldsWidth)) {
                slot.name = nameBuf;
            }
            ItemTooltip(T("Optional label shown with this marker in the set."));

            float x = slot.set ? slot.x : 0.0f;
            float y = slot.set ? slot.y : 0.0f;
            float z = slot.set ? slot.z : 0.0f;

            if (InputOptionalFloat("X##x", "X", x, active, kCoordWidth)) {
                slot.x = x;
                slot.set = true;
            }
            ItemTooltip(T("World X (east/west)."));

            ImGui::SameLine();
            if (InputOptionalFloat("Y##y", "Y", y, active, kCoordWidth)) {
                slot.y = y;
                slot.set = true;
            }
            ItemTooltip(T("World Y (north/south)."));

            ImGui::SameLine();
            if (InputOptionalFloat("Z##z", "Z", z, active, kCoordWidth)) {
                slot.z = z;
                slot.set = true;
            }
            ItemTooltip(T("World Z (height/elevation)."));

            if (!active) {
                ImGui::PopStyleVar();
            }

            ImGui::TableSetColumnIndex(2);
            if (active) {
                CenterCursorInCell(kClearColumnWidth, kDeleteButtonSize, rowHeight,
                                   kDeleteButtonSize);
                if (DrawDeleteButton()) {
                    ClearSlot(slot);
                }
                ItemTooltip(T("Remove this marker from the set"));
            }

            ImGui::TableSetColumnIndex(3);
            if (ImGui::Button(T("Set Location"), actionButtonSize)) {
                ApplyPlayerPosition(state, slot);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(T("Use your current player position for this marker."));
            }
            if (rtapiAvailable) {
                if (OptionsUiKit::TexturedButton("import", T("Import"),
                                                 TextureService::GetUiTexture("iconImport"), 16.0f,
                                                 actionButtonSize)) {
                    ImportSlotFromRtApi(slotIndex, slot);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Import this marker's position from squad markers placed in-game.");
                }
            } else {
                RenderDisabledRtApiButton("import_disabled", T("Import"), "iconImport",
                                          "Requires the Real-Time API addon.", actionButtonSize);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void RenderUnsavedDialog(AppState& state) {
    if (!g_state.showUnsavedDialog) {
        return;
    }

    const std::string unsavedDialogName =
        MakeWindowName(T("Unsaved Marker Set Changes"), kUnsavedDialogId);
    ImGui::OpenPopup(unsavedDialogName.c_str());

    if (!ImGui::BeginPopupModal(unsavedDialogName.c_str(), nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    OptionsUiKit::WarningText(T("You have unsaved changes to this marker set."));
    ImGui::TextUnformatted(T("Save your changes, discard them, or keep editing."));

    if (!g_state.validationError.empty()) {
        OptionsUiKit::WarningText(g_state.validationError.c_str());
    }

    if (OptionsUiKit::TexturedButton("save", T("Save"), TextureService::GetUiTexture("iconSave"))) {
        if (TrySaveDraft(state)) {
            ForceClose(state);
            g_state.showUnsavedDialog = false;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (OptionsUiKit::TexturedButton("discard", "Discard",
                                     TextureService::GetUiTexture("iconGoBack"))) {
        ForceClose(state);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Keep Editing")) {
        g_state.showUnsavedDialog = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void DeleteDraft(AppState& state) {
    const auto markerSets = state.markerListing.GetAllMarkerSets();
    if (g_state.listingIndex >= 0 &&
        static_cast<size_t>(g_state.listingIndex) < markerSets.size()) {
        state.markerListing.DeleteMarker(markerSets[static_cast<size_t>(g_state.listingIndex)]);
    }
    g_state.showDeleteDialog = false;
    ForceClose(state);
}

void RenderDeleteDialog(AppState& state) {
    if (!g_state.showDeleteDialog) {
        return;
    }

    const std::string deleteDialogName = MakeWindowName(T("Delete Marker Set"), kDeleteDialogId);
    ImGui::OpenPopup(deleteDialogName.c_str());

    if (!ImGui::BeginPopupModal(deleteDialogName.c_str(), nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    OptionsUiKit::WarningText(T("Delete this marker set?"));
    const std::string displayName = TrimCopy(g_state.draft.name);
    if (!displayName.empty()) {
        ImGui::Text(T("\"%s\" will be removed from your library."), displayName.c_str());
    } else {
        ImGui::TextUnformatted("This cannot be undone.");
    }

    if (OptionsUiKit::TexturedButton("delete_confirm", T("Delete"),
                                     TextureService::GetUiTexture("iconDelete"))) {
        DeleteDraft(state);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (OptionsUiKit::TexturedButton("delete_cancel", T("Cancel"),
                                     TextureService::GetUiTexture("iconGoBack"))) {
        g_state.showDeleteDialog = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

}  // namespace

void OpenNew(AppState& state) {
    g_state.open = true;
    g_state.listingIndex = -1;
    g_state.importBuffer.clear();
    g_state.importError.clear();
    g_state.validationError.clear();
    g_state.showUnsavedDialog = false;
    g_state.showDeleteDialog = false;

    MarkerSet markerSet{};
    markerSet.id = GenerateUuidV4();
    markerSet.source = "custom";
    if (state.mumbleLink) {
        markerSet.mapId = static_cast<int>(state.mumbleLink->Context.MapID);
        ApplyPlayerPosition(state, markerSet.trigger, &markerSet.mapId);
    }
    g_state.draft = markerSet;
    LoadSlotsFromDraft();
    SetBaselineFromDraft();
}

void OpenExisting(AppState& state, const MarkerSet& markerSet, int listingIndex) {
    if (MarkerListing::IsCommunityLinked(markerSet)) {
        OpenPersonalizedFromTemplate(state, markerSet);
        return;
    }
    g_state.open = true;
    g_state.listingIndex = listingIndex;
    g_state.draft = markerSet;
    g_state.importBuffer.clear();
    g_state.importError.clear();
    g_state.validationError.clear();
    g_state.showUnsavedDialog = false;
    g_state.showDeleteDialog = false;
    LoadSlotsFromDraft();
    SetBaselineFromDraft();
    (void)state;
}

void OpenPersonalizedFromTemplate(AppState& state, const MarkerSet& templateSet) {
    g_state.open = true;
    g_state.listingIndex = -1;
    g_state.draft = MarkerListing::DuplicateAsEditableCopy(templateSet);
    if (g_state.draft.name == templateSet.name && !g_state.draft.name.empty()) {
        g_state.draft.name += T(" (personal)");
    }
    g_state.importBuffer.clear();
    g_state.importError.clear();
    g_state.validationError.clear();
    g_state.showUnsavedDialog = false;
    g_state.showDeleteDialog = false;
    LoadSlotsFromDraft();
    SetBaselineFromDraft();
    (void)state;
}

void Close() {
    g_state.open = false;
    g_state.listingIndex = -1;
    g_state.importBuffer.clear();
    g_state.importError.clear();
    g_state.validationError.clear();
    g_state.showUnsavedDialog = false;
    g_state.showDeleteDialog = false;
    g_state.hasBaseline = false;
}

bool IsOpen() { return g_state.open; }

void Render(AppState& state) {
    if (!g_state.open) {
        return;
    }

    using namespace OptionsUiKit;

    const float minWidth = CalcEditorMinWidth();
    const float fieldsWidth = CalcMarkerFieldsWidth(kCoordWidth);

    ImGui::SetNextWindowSize(ImVec2(minWidth, 650.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(minWidth, 650.0f), ImVec2(9999.0f, 9999.0f));

    bool windowOpen = g_state.open;
    const std::string windowName = MakeWindowName(T("Create/edit markers"), kWindowId);
    if (!ImGui::Begin(windowName.c_str(), &windowOpen, ImGuiWindowFlags_None)) {
        ImGui::End();
        if (!windowOpen) {
            g_state.open = true;
            RequestClose(state);
        }
        RenderUnsavedDialog(state);
        RenderDeleteDialog(state);
        return;
    }

    if (!windowOpen) {
        g_state.open = true;
        RequestClose(state);
        ImGui::End();
        RenderUnsavedDialog(state);
        RenderDeleteDialog(state);
        return;
    }

    char nameBuf[128];
    char descBuf[256];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", g_state.draft.name.c_str());
    std::snprintf(descBuf, sizeof(descBuf), "%s", g_state.draft.description.c_str());

    if (InputTextHint(T("Name"), nameBuf, sizeof(nameBuf), "Marker set name", fieldsWidth)) {
        g_state.draft.name = nameBuf;
    }
    ItemTooltip(T("Name shown on the map when you are near this marker set's trigger."));
    ImGui::SameLine();
    OptionsUiKit::IconButton("preview", TextureService::GetUiTexture("eye"), 20.0f,
                             T("Hover to preview markers on the map"));
    if (ImGui::IsItemHovered()) {
        SyncDraftFromSlots();
        state.mapWatch.SetPreviewMarkerSet(g_state.draft);
        g_state.previewActive = true;
    } else if (g_state.previewActive) {
        StopPreview(state);
    }

    if (InputTextHint(T("Description"), descBuf, sizeof(descBuf), T("Description"), fieldsWidth)) {
        g_state.draft.description = descBuf;
    }
    ItemTooltip(T("Description shown on the map when you are near this marker set's trigger."));

    ImGui::TextUnformatted(T("Map"));
    ImGui::SameLine();
    ImGui::TextUnformatted(
        MapPickerUi::FormatMapLabel(state.mapData, g_state.draft.mapId).c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(T("Updated when you use Set Trigger Location."));
    }

    InputWorldCoord(T("Trigger"), g_state.draft.trigger);
    if (ImGui::Button(T("Set Trigger Location"))) {
        ApplyPlayerPosition(state, g_state.draft.trigger, &g_state.draft.mapId);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(T("Snap trigger to your current position and update the map ID."));
    }

    ImGui::Spacing();
    SectionHeading("Markers");
    RenderMarkerTable(state);

    if (!g_state.validationError.empty()) {
        WarningText(g_state.validationError.c_str());
    }

    ImGui::Spacing();
    if (OptionsUiKit::TexturedButton("save", T("Save"), TextureService::GetUiTexture("iconSave"))) {
        if (TrySaveDraft(state)) {
            ForceClose(state);
        }
    }
    ImGui::SameLine();
    if (OptionsUiKit::TexturedButton("cancel", T("Cancel"),
                                     TextureService::GetUiTexture("iconGoBack"))) {
        if (ShiftModifierHeld()) {
            ForceClose(state);
        } else {
            RequestClose(state);
        }
    }
    if (g_state.listingIndex >= 0) {
        ImGui::SameLine();
        if (OptionsUiKit::TexturedButton("delete", T("Delete"),
                                         TextureService::GetUiTexture("iconDelete"))) {
            if (ShiftModifierHeld()) {
                DeleteDraft(state);
            } else {
                g_state.showDeleteDialog = true;
            }
        }
    }
    ImGui::SameLine();
    if (OptionsUiKit::TexturedButton("export", T("Export"),
                                     TextureService::GetUiTexture("iconExport"))) {
        SyncDraftFromSlots();
        ImGui::SetClipboardText(MarkerSetClipboard::ExportToBase64(g_state.draft).c_str());
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(T("Copy a BlishHUD-compatible Base64 marker set share code to the clipboard."));
    }
    ImGui::SameLine();
    if (OptionsUiKit::TexturedButton("import", T("Import"),
                                     TextureService::GetUiTexture("iconImport"))) {
        g_state.importError.clear();
        if (const char* clipboard = ImGui::GetClipboardText()) {
            g_state.importBuffer = clipboard;
        } else {
            g_state.importBuffer.clear();
        }
        ImGui::OpenPopup(T("Import Marker Set"));
    }

    if (ImGui::BeginPopupModal(T("Import Marker Set"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        SectionSubtext(T("Paste a marker set share code from BlishHUD or this module."));

        char buffer[8192];
        std::snprintf(buffer, sizeof(buffer), "%s", g_state.importBuffer.c_str());
        if (ImGui::InputTextMultiline("##import", buffer, sizeof(buffer), ImVec2(480.0f, 120.0f))) {
            g_state.importBuffer = buffer;
        }

        if (OptionsUiKit::TexturedButton("paste", T("Paste Clipboard"),
                                         TextureService::GetUiTexture("iconCopy"))) {
            if (const char* clipboard = ImGui::GetClipboardText()) {
                g_state.importBuffer = clipboard;
            }
        }

        if (!g_state.importError.empty()) {
            WarningText(g_state.importError.c_str());
        }

        if (OptionsUiKit::TexturedButton("import_confirm", T("Import"),
                                         TextureService::GetUiTexture("iconImport"))) {
            const auto resolver = [&state](const std::string& communitySetId,
                                           const std::string& name, MarkerSet& out) -> bool {
                (void)name;
                return state.communityCatalog.FetchSetDetail(communitySetId, out, nullptr);
            };
            if (const std::optional<MarkerSet> imported =
                    MarkerSetClipboard::ImportFromText(g_state.importBuffer, resolver)) {
                g_state.draft = *imported;
                if (g_state.draft.id.empty()) {
                    g_state.draft.id = GenerateUuidV4();
                }
                LoadSlotsFromDraft();
                g_state.importError.clear();
                ImGui::CloseCurrentPopup();
            } else {
                g_state.importError = "Could not parse marker set. Paste valid Base64 or JSON.";
            }
        }
        ImGui::SameLine();
        if (OptionsUiKit::TexturedButton("import_close", T("Close"),
                                         TextureService::GetUiTexture("iconGoBack"))) {
            g_state.importError.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
    RenderUnsavedDialog(state);
    RenderDeleteDialog(state);
}

}  // namespace MarkerSetEditorWindow
}  // namespace cm
