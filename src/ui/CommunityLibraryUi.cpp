#include "ui/CommunityLibraryUi.h"

#include "core/AppState.h"
#include "core/MumbleUtils.h"
#include "data/MarkerSetJson.h"
#include "services/HttpClient.h"
#include "services/MarkerListing.h"
#include "ui/LibrarySearchUi.h"
#include "ui/MapPreviewPopupUi.h"
#include "ui/OptionsApiTab.h"
#include "ui/OptionsUiKit.h"
#include "ui/TextureService.h"
#include "core/Locale.h"

#include <algorithm>
#include <imgui.h>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>

namespace cm {
namespace CommunityLibraryUi {
namespace {

// Short alias so the wrapped literals stay readable.
using cm::Locale::T;
using cm::Locale::Ts;


struct UiState {
    int categoryIndex = 0;
};

struct ShareRowState {
    int categoryIndex = 0;
    char customCategory[128] = "";
    std::string error;
    std::string status;
    bool sharing = false;
};

UiState g_state;
std::mutex g_shareMutex;
std::unordered_map<std::string, ShareRowState> g_shareRows;
char g_searchQuery[128] = "";

constexpr float kImportColumnWidth = 72.0f;
constexpr float kIconColumnWidth = 72.0f;
constexpr float kIconSize = 64.0f;
constexpr float kActionsColumnWidth = 180.0f;

bool NuclearModifierHeld() {
    return ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift;
}

float CalcCommunityRowHeight() {
    const float textHeight =
        ImGui::GetTextLineHeightWithSpacing() * 3.0f + ImGui::GetTextLineHeight();
    const float iconHeight = kIconSize + ImGui::GetStyle().CellPadding.y * 2.0f;
    return std::max(textHeight + ImGui::GetStyle().CellPadding.y * 2.0f, iconHeight);
}

void CenterCursorInCell(float columnWidth, float contentWidth, float rowHeight,
                        float contentHeight) {
    const ImVec2 cellPos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cellPos.x + (columnWidth - contentWidth) * 0.5f,
                               cellPos.y + (rowHeight - contentHeight) * 0.5f));
}

float ActionButtonHeight() { return ImGui::GetFrameHeight(); }

SquadMarker ListingIconForRow(size_t listingIndex) {
    return static_cast<SquadMarker>(((listingIndex + 1) % 8) + 1);
}

bool SummaryMatches(const AppState& state,
                    const CommunitySetSummary& summary,
                    const std::string& queryLower) {
    if (queryLower.empty()) {
        return true;
    }
    const auto contains = [&](const std::string& text) {
        return LibrarySearchUi::ToLowerCopy(text).find(queryLower) != std::string::npos;
    };
    return contains(summary.name) || contains(summary.description) ||
           contains(summary.author) || contains(state.mapData.Describe(summary.mapId));
}

ImTextureID RowIcon(AppState& state, const CommunitySetSummary& summary, size_t rowIndex) {
    state.previewImageCache.RequestThumb(summary.id, summary.previewThumbUrl);
    const std::string thumbPath = state.previewImageCache.ThumbPathForSet(summary.id);
    if (!thumbPath.empty()) {
        if (ImTextureID tex = TextureService::GetThumbTexture("CM_THUMB_" + summary.id, thumbPath)) {
            return tex;
        }
    }
    return TextureService::GetTexture(ListingIconForRow(rowIndex));
}

void NotifyImportSuccess(const AppState& state, const std::string& markerSetName) {
    if (!state.api || !state.api->GUI_SendAlert) {
        return;
    }
    // Translated as a whole sentence first, then the set name is spliced in, so
    // the name lands in the right place for Chinese.
    std::string message;
    if (markerSetName.empty()) {
        message = T("Imported community marker set into your library.");
    } else {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer), Ts(T("Imported \"%s\" into your library.")).c_str(),
                      markerSetName.c_str());
        message = buffer;
    }
    state.api->GUI_SendAlert(message.c_str());
}

bool FetchSetForActions(AppState& state,
                        const CommunitySetSummary& summary,
                        MarkerSet& outSet) {
    return state.communityCatalog.FetchSetDetail(summary.id, outSet, nullptr);
}

void RenderImportButton(AppState& state, const CommunitySetSummary& summary) {
    using namespace OptionsUiKit;

    const bool alreadyImported = state.markerListing.ContainsCommunitySetId(summary.id);
    const float actionHeight = ActionButtonHeight();
    const ImVec2 buttonSize(-1.0f, actionHeight);

    CenterCursorInCell(kImportColumnWidth, kImportColumnWidth, CalcCommunityRowHeight(),
                       actionHeight);

    if (alreadyImported) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.45f);
        TexturedButton("import_disabled", T("Import"), TextureService::GetUiTexture("iconImport"),
                       16.0f, buttonSize);
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip(T("Already in your library."));
        }
        return;
    }

    if (TexturedButton("import", T("Import"), TextureService::GetUiTexture("iconImport"), 16.0f,
                       buttonSize)) {
        MarkerSet imported{};
        if (FetchSetForActions(state, summary, imported)) {
            state.markerListing.SaveMarker(imported);
            NotifyImportSuccess(state, summary.name);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(T("Import this community marker set into your library."));
    }
}

void RenderCommunityRow(AppState& state,
                        const CommunitySetSummary& summary,
                        size_t rowIndex,
                        int currentMapId) {
    using namespace OptionsUiKit;

    ImGui::PushID(static_cast<int>(rowIndex));
    ImGui::TableNextRow();

    const float rowHeight = CalcCommunityRowHeight();
    const float actionHeight = ActionButtonHeight();
    const ImGuiStyle& style = ImGui::GetStyle();

    ImGui::TableSetColumnIndex(0);
    RenderImportButton(state, summary);

    ImGui::TableSetColumnIndex(1);
    CenterCursorInCell(kIconColumnWidth, kIconSize, rowHeight, kIconSize);
    const ImTextureID icon = RowIcon(state, summary, rowIndex);
    const bool hasMapPreview = !summary.id.empty() && icon != nullptr &&
                               !state.previewImageCache.ThumbPathForSet(summary.id).empty();
    MapPreviewPopupUi::Target previewTarget{};
    if (hasMapPreview) {
        previewTarget.communitySetId = summary.id;
        previewTarget.previewLargeUrl = summary.previewLargeUrl;
        previewTarget.previewThumbUrl = summary.previewThumbUrl;
        previewTarget.label = summary.name;
        previewTarget.description = summary.description;
        previewTarget.zoomable = true;
    }
    MapPreviewPopupUi::RenderIcon(state, previewTarget, icon, ImVec2(kIconSize, kIconSize));

    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%s", summary.name.c_str());
    if (!summary.description.empty()) {
        ImGui::TextDisabled("%s", summary.description.c_str());
    } else {
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));
    }
    ImGui::TextDisabled(T("Map: %s"), state.mapData.Describe(summary.mapId).c_str());
    ImGui::TextDisabled(T("Author: %s"), summary.author.c_str());

    ImGui::TableSetColumnIndex(3);
    if (summary.mapId == currentMapId) {
        const float previewWidth =
            style.FramePadding.x * 2.0f + ImGui::CalcTextSize(T("Preview")).x;
        const float stopWidth =
            style.FramePadding.x * 2.0f + ImGui::CalcTextSize(T("Stop")).x;
        const float placeWidth =
            style.FramePadding.x * 2.0f + ImGui::CalcTextSize(T("Place")).x;
        const float actionsWidth = previewWidth + style.ItemSpacing.x + stopWidth +
                                   style.ItemSpacing.x + placeWidth;

        CenterCursorInCell(kActionsColumnWidth, actionsWidth, rowHeight, actionHeight);

        if (ImGui::Button(T("Preview"), ImVec2(previewWidth, actionHeight))) {
            MarkerSet markerSet{};
            if (FetchSetForActions(state, summary, markerSet)) {
                state.mapWatch.SetPreviewMarkerSet(markerSet);
            }
        }
        ImGui::SameLine(0.0f, style.ItemSpacing.x);
        if (ImGui::Button(T("Stop"), ImVec2(stopWidth, actionHeight))) {
            state.mapWatch.RemovePreviewMarkerSet();
        }
        ImGui::SameLine(0.0f, style.ItemSpacing.x);
        if (ImGui::Button(T("Place"), ImVec2(placeWidth, actionHeight)) && state.mumbleLink &&
            state.nexusLink) {
            MarkerSet markerSet{};
            if (FetchSetForActions(state, summary, markerSet)) {
                state.mapWatch.PlaceMarkerSet(markerSet, state.mumbleLink, state.nexusLink);
            }
        }
    }

    ImGui::PopID();
}

std::vector<CommunitySetSummary> FilterSets(const AppState& state,
                                            const std::vector<CommunitySetSummary>& sets,
                                            const std::string& selectedCategoryName,
                                            int currentMapId,
                                            const std::string& searchLower) {
    std::vector<CommunitySetSummary> filtered;
    for (const auto& summary : sets) {
        if (!selectedCategoryName.empty() && summary.categoryName != selectedCategoryName) {
            continue;
        }
        if (state.settings.libraryFilterCurrentMap && summary.mapId != currentMapId) {
            continue;
        }
        if (!SummaryMatches(state, summary, searchLower)) {
            continue;
        }
        if (state.settings.communityLibraryShowAvailable &&
            state.markerListing.ContainsCommunitySetId(summary.id)) {
            continue;
        }
        filtered.push_back(summary);
    }
    std::sort(filtered.begin(), filtered.end(),
              [](const CommunitySetSummary& a, const CommunitySetSummary& b) {
                  if (a.categoryName != b.categoryName) return a.categoryName < b.categoryName;
                  return a.name < b.name;
              });
    return filtered;
}

std::vector<std::string> BuildCategoryFilterLabels(
    const std::vector<CommunityCategoryEntry>& categories,
    const std::vector<CommunitySetSummary>& sets) {
    std::vector<std::string> labels;
    labels.push_back(T("All categories"));
    std::set<std::string> seen;
    for (const auto& category : categories) {
        if (!category.name.empty() && seen.insert(category.name).second) {
            labels.push_back(category.name);
        }
    }
    if (labels.size() == 1) {
        for (const auto& summary : sets) {
            if (!summary.categoryName.empty() && seen.insert(summary.categoryName).second) {
                labels.push_back(summary.categoryName);
            }
        }
    }
    return labels;
}

std::vector<std::string> BuildShareCategoryNames(
    const std::vector<CommunityCategoryEntry>& categories,
    const std::vector<CommunitySetSummary>& sets) {
    const auto labels = BuildCategoryFilterLabels(categories, sets);
    if (labels.size() <= 1) {
        return {};
    }
    return std::vector<std::string>(labels.begin() + 1, labels.end());
}

std::string ResolveShareCategory(const ShareRowState& rowState,
                                 const std::vector<std::string>& categoryNames) {
    const int customIndex = static_cast<int>(categoryNames.size());
    if (rowState.categoryIndex == customIndex || categoryNames.empty()) {
        return rowState.customCategory;
    }
    if (rowState.categoryIndex >= 0 &&
        rowState.categoryIndex < static_cast<int>(categoryNames.size())) {
        return categoryNames[static_cast<size_t>(rowState.categoryIndex)];
    }
    return rowState.customCategory;
}

void RenderShareCategoryPicker(ShareRowState& rowState,
                               const std::vector<std::string>& categoryNames) {
    // Rebuilt every frame (not static/constexpr) so a language switch takes
    // effect immediately.
    const std::string customLabel = T("Custom...");

    std::vector<std::string> comboStorage = categoryNames;
    comboStorage.emplace_back(customLabel);
    const int customIndex = static_cast<int>(categoryNames.size());

    std::vector<const char*> comboLabels;
    comboLabels.reserve(comboStorage.size());
    for (const auto& label : comboStorage) {
        comboLabels.push_back(label.c_str());
    }

    if (rowState.categoryIndex < 0 || rowState.categoryIndex > customIndex) {
        rowState.categoryIndex = categoryNames.empty() ? customIndex : 0;
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##category", &rowState.categoryIndex, comboLabels.data(),
                 static_cast<int>(comboLabels.size()));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(T("Pick an existing category or choose Custom to type your own."));
    }

    if (rowState.categoryIndex == customIndex || categoryNames.empty()) {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##category_custom", T("Type category name"), rowState.customCategory,
                                 sizeof(rowState.customCategory));
    }
}

std::string FormatShareError(const HttpResponse& response, const char* fallback) {
    if (!response.body.empty()) {
        try {
            const auto j = nlohmann::json::parse(response.body);
            if (j.contains("message") && j["message"].is_string()) {
                return j["message"].get<std::string>();
            }
            if (j.contains("error") && j["error"].is_string()) {
                return j["error"].get<std::string>();
            }
        } catch (...) {
        }
    }
    // Translated as a whole sentence so the status code and punctuation land in
    // the right place for Chinese.
    char buffer[512];
    std::snprintf(buffer, sizeof(buffer), Ts(T("%s (%d).")).c_str(), fallback,
                  response.statusCode);
    return std::string(buffer);
}

std::optional<std::string> ValidateShareRequest(const AppState& state,
                                                const MarkerSet& markerSet,
                                                const std::string& category) {
    if (category.empty()) {
        return T("Enter a category name.");
    }
    if (markerSet.name.empty()) {
        return T("Marker set name is required.");
    }
    if (markerSet.mapId <= 0) {
        return T("Marker set must have a valid map.");
    }
    if (!state.accountRegistry.ActiveApiKey()) {
        return T("Register a GW2 API key below first.");
    }
    return std::nullopt;
}

void ExecuteShareWithCommunity(AppState& state,
                               const std::string& apiKey,
                               const std::string& submissionsUrl,
                               const MarkerSet& markerSet,
                               const std::string& category,
                               std::string& errorOut,
                               std::string& statusOut) {
    errorOut.clear();
    statusOut.clear();

    const auto subtoken = state.subtokenService.GetSubtoken(apiKey);
    if (!subtoken) {
        errorOut = T("Failed to obtain share token from server.");
        return;
    }

    const nlohmann::json payload =
        MarkerSetJson::SubmissionPayloadToJson(markerSet, category);

    HttpRequestOptions options;
    options.bearerToken = *subtoken;
    const auto response = HttpPostJsonUrlEx(submissionsUrl, payload.dump(), options);
    if (response.statusCode < 200 || response.statusCode >= 300) {
        errorOut = FormatShareError(response, T("Share failed"));
        return;
    }
    statusOut = T("Sent for moderator review.");
}

void StartShareRequest(AppState& state,
                       const MarkerSet& markerSet,
                       const std::string& category) {
    if (const auto validationError = ValidateShareRequest(state, markerSet, category)) {
        std::lock_guard lock(g_shareMutex);
        ShareRowState& rowState = g_shareRows[markerSet.id];
        rowState.error = *validationError;
        rowState.status.clear();
        return;
    }

    const std::string apiKey = *state.accountRegistry.ActiveApiKey();
    const std::string submissionsUrl =
        state.moduleManifest.Get().Absolute(state.moduleManifest.Get().submissionsUrl);
    const MarkerSet markerSetCopy = markerSet;
    const std::string markerSetId = markerSet.id;

    {
        std::lock_guard lock(g_shareMutex);
        ShareRowState& rowState = g_shareRows[markerSetId];
        if (rowState.sharing) {
            return;
        }
        rowState.sharing = true;
        rowState.error.clear();
        rowState.status.clear();
    }

    std::thread([apiKey, submissionsUrl, markerSetCopy, category, markerSetId]() {
        std::string error;
        std::string status;
        ExecuteShareWithCommunity(AppState::Instance(), apiKey, submissionsUrl, markerSetCopy,
                                  category, error, status);

        std::lock_guard lock(g_shareMutex);
        ShareRowState& rowState = g_shareRows[markerSetId];
        rowState.sharing = false;
        if (error.empty()) {
            rowState.status = status;
            rowState.error.clear();
        } else {
            rowState.error = error;
            rowState.status.clear();
        }
    }).detach();
}

void RenderShareWithCommunitySection(AppState& state,
                                       const std::vector<std::string>& shareCategoryNames) {
    using namespace OptionsUiKit;

    ImGui::Spacing();
    SectionHeading(T("Share with community"));

    if (!state.accountRegistry.ActiveApiKey()) {
        SectionSubtext(
            T("Register a GW2 API key to share your marker sets with the community library."));
        if (ImGui::BeginChild("##cm_share_api_key", ImVec2(0.0f, 140.0f), true)) {
            OptionsApiTab::RenderApiKeyManagement(state);
        }
        ImGui::EndChild();
        return;
    }

    SectionSubtext(T("Share your custom marker sets with the community library."));

    const auto markerSets = state.markerListing.GetAllMarkerSets();
    bool anyShareable = false;

    if (ImGui::BeginChild("##cm_share_list", ImVec2(0.0f, 180.0f), true)) {
        if (ImGui::BeginTable("##cm_share_rows", 3,
                              ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("##details", ImGuiTableColumnFlags_WidthStretch, 4.0f);
            ImGui::TableSetupColumn("##category", ImGuiTableColumnFlags_WidthFixed, 200.0f);
            ImGui::TableSetupColumn("##share", ImGuiTableColumnFlags_WidthFixed, 160.0f);

            for (size_t i = 0; i < markerSets.size(); ++i) {
                const MarkerSet& markerSet = markerSets[i];
                if (!MarkerListing::IsShareableWithCommunity(markerSet)) {
                    continue;
                }
                anyShareable = true;

                ImGui::PushID(markerSet.id.c_str());
                ShareRowState& rowState = g_shareRows[markerSet.id];

                bool sharing = false;
                std::string displayError;
                std::string displayStatus;
                {
                    std::lock_guard lock(g_shareMutex);
                    sharing = rowState.sharing;
                    displayError = rowState.error;
                    displayStatus = rowState.status;
                }

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", markerSet.name.c_str());
                if (!markerSet.description.empty()) {
                    ImGui::TextDisabled("%s", markerSet.description.c_str());
                }
                ImGui::TextDisabled(T("Map: %s | Markers: %zu"),
                                    state.mapData.Describe(markerSet.mapId).c_str(),
                                    markerSet.markers.size());

                ImGui::TableSetColumnIndex(1);
                RenderShareCategoryPicker(rowState, shareCategoryNames);

                ImGui::TableSetColumnIndex(2);
                const float shareButtonHeight = ActionButtonHeight();
                if (sharing) {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.55f);
                    ImGui::Button(T("Sharing..."), ImVec2(-1.0f, shareButtonHeight));
                    ImGui::PopStyleVar();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip(T("Uploading marker set for moderator review."));
                    }
                } else if (OptionsUiKit::TexturedButton(
                               "share", T("Share with community"),
                               TextureService::GetUiTexture("iconImport"), 16.0f,
                               ImVec2(-1.0f, shareButtonHeight))) {
                    const std::string category =
                        ResolveShareCategory(rowState, shareCategoryNames);
                    StartShareRequest(state, markerSet, category);
                }

                if (!displayError.empty()) {
                    WarningText(displayError.c_str());
                } else if (!displayStatus.empty()) {
                    ImGui::TextColored(ImVec4(0.45f, 0.95f, 0.45f, 1.0f), "%s",
                                       displayStatus.c_str());
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        if (!anyShareable) {
            DisabledGateText(T("No custom marker sets available to share."));
        }
    }
    ImGui::EndChild();
}

}  // namespace

void Render(AppState& state) {
    using namespace OptionsUiKit;

    SectionHeading(T("Community Library"));
    SectionSubtext(T("Browse and import marker sets from the GeoGuesser community library."));

    const auto sets = state.communityCatalog.GetSets();
    const auto categories = state.communityCatalog.GetCategories();
    const int currentMapId =
        state.mumbleLink ? static_cast<int>(state.mumbleLink->Context.MapID) : 0;

    if (sets.empty()) {
        DisabledGateText(T("Community library not loaded yet."));
        if (ImGui::Button(T("Download Community Library"))) {
            state.RequestCommunitySync(true);
        }
        RenderShareWithCommunitySection(state, {});
        return;
    }

    std::vector<const char*> categoryLabels;
    const std::vector<std::string> categoryLabelStorage =
        BuildCategoryFilterLabels(categories, sets);
    categoryLabels.reserve(categoryLabelStorage.size());
    for (const auto& label : categoryLabelStorage) {
        categoryLabels.push_back(label.c_str());
    }
    if (g_state.categoryIndex >= static_cast<int>(categoryLabels.size())) {
        g_state.categoryIndex = 0;
    }

    SettingCombo(T("Category"), &g_state.categoryIndex, categoryLabels.data(),
                 static_cast<int>(categoryLabels.size()));

    ImGui::SameLine();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float redownloadWidth = style.FramePadding.x * 2.0f + ImGui::CalcTextSize(T("Redownload")).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - redownloadWidth);
    const bool canForceRedownload = NuclearModifierHeld();
    if (!canForceRedownload) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.45f);
    }
    if (ImGui::Button(T("Redownload")) && canForceRedownload) {
        state.RequestCommunitySync(true);
    }
    if (!canForceRedownload) {
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip(T("Hold Ctrl+Shift to force a community library redownload."));
        }
    }

    SettingCheckbox(T("Filter to current map"), &state.settings.libraryFilterCurrentMap);
    ImGui::SameLine();
    SettingCheckbox(T("Only Show available"), &state.settings.communityLibraryShowAvailable,
                    T("Hide marker sets already in your library."));

    ImGui::SameLine();
    const float searchX =
        ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - LibrarySearchUi::kSearchFieldWidth;
    ImGui::SetCursorPosX(searchX);
    ImGui::SetNextItemWidth(LibrarySearchUi::kSearchFieldWidth);
    ImGui::InputTextWithHint("##cm_community_search", T("Search"), g_searchQuery, sizeof(g_searchQuery));

    ImGui::Separator();

    const std::string selectedCategoryName =
        g_state.categoryIndex == 0 ? "" :
                                       categoryLabelStorage[static_cast<size_t>(g_state.categoryIndex)];
    const std::string searchLower = LibrarySearchUi::ToLowerCopy(g_searchQuery);
    const auto visibleSets =
        FilterSets(state, sets, selectedCategoryName, currentMapId, searchLower);

    if (ImGui::BeginChild("##cm_community_list", ImVec2(0.0f, 360.0f), true)) {
        bool anyVisible = false;

        if (ImGui::BeginTable("##cm_community_rows", 4,
                              ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("##import", ImGuiTableColumnFlags_WidthFixed, kImportColumnWidth);
            ImGui::TableSetupColumn("##icon", ImGuiTableColumnFlags_WidthFixed, kIconColumnWidth);
            ImGui::TableSetupColumn("##details", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed,
                                    kActionsColumnWidth);

            for (size_t i = 0; i < visibleSets.size(); ++i) {
                anyVisible = true;
                RenderCommunityRow(state, visibleSets[i], i, currentMapId);
            }

            ImGui::EndTable();
        }

        if (!anyVisible) {
            DisabledGateText(T("No marker sets match the current filters."));
        }
    }
    ImGui::EndChild();

    const std::vector<std::string> shareCategoryNames =
        BuildShareCategoryNames(categories, sets);
    RenderShareWithCommunitySection(state, shareCategoryNames);
}

}  // namespace CommunityLibraryUi
}  // namespace cm
