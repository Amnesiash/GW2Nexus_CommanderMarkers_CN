#include "ui/MapPickerUi.h"
#include "core/Locale.h"

#include <cctype>
#include <cstdio>
#include <imgui.h>
#include <string>

namespace cm {
namespace MapPickerUi {

std::string FormatMapLabel(const MapDataCache& cache, int mapId);

namespace {

// Short alias so the wrapped literals stay readable.
using cm::Locale::T;


constexpr int kMaxVisibleResults = 100;

struct PickerState {
    char input[128] = {};
    int syncedMapId = -1;
};

std::string ToLowerAscii(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }

    return ToLowerAscii(haystack).find(ToLowerAscii(needle)) != std::string::npos;
}

bool MatchesMapFilter(const Gw2Map& map, const std::string& filter) {
    if (filter.empty()) {
        return true;
    }

    if (ContainsCaseInsensitive(map.name, filter)) {
        return true;
    }

    const std::string idText = std::to_string(map.id);
    return idText.find(filter) != std::string::npos;
}

std::string BuildLabel(const Gw2Map& map) {
    if (map.name.empty()) {
        return "(" + std::to_string(map.id) + ")";
    }
    return map.name + " (" + std::to_string(map.id) + ")";
}

void SyncInputFromMapId(PickerState& state, const MapDataCache& cache, int mapId) {
    const std::string label = FormatMapLabel(cache, mapId);
    std::snprintf(state.input, sizeof(state.input), "%s", label.c_str());
    state.syncedMapId = mapId;
}

PickerState& GetPickerState() {
    ImGuiStorage* storage = ImGui::GetStateStorage();
    ImGuiID id = ImGui::GetID("##cm_map_picker_state");
    auto* state = static_cast<PickerState*>(storage->GetVoidPtr(id));
    if (!state) {
        state = IM_NEW(PickerState)();
        storage->SetVoidPtr(id, state);
    }
    return *state;
}

bool TryParseMapId(const char* text, int& mapId) {
    if (!text || text[0] == '\0') {
        return false;
    }

    for (const char* c = text; *c != '\0'; ++c) {
        if (!std::isdigit(static_cast<unsigned char>(*c))) {
            return false;
        }
    }

    try {
        mapId = std::stoi(text);
        return true;
    } catch (...) {
        return false;
    }
}

void RenderResultList(const MapDataCache& cache, const std::string& filter, int* mapId,
                      PickerState& state, bool& changed) {
    const std::vector<Gw2Map> maps = cache.GetAllMaps();
    int shown = 0;

    for (const Gw2Map& map : maps) {
        if (!MatchesMapFilter(map, filter)) {
            continue;
        }

        const std::string itemLabel = BuildLabel(map);
        const bool selected = map.id == *mapId;
        if (ImGui::Selectable(itemLabel.c_str(), selected)) {
            *mapId = map.id;
            SyncInputFromMapId(state, cache, *mapId);
            changed = true;
            ImGui::CloseCurrentPopup();
        }

        if (++shown >= kMaxVisibleResults) {
            ImGui::TextDisabled(T("Showing first %d matches. Refine your search."),
                                kMaxVisibleResults);
            break;
        }
    }

    if (shown == 0) {
        ImGui::TextDisabled(T("No maps match your search."));
    }
}

}  // namespace

std::string FormatMapLabel(const MapDataCache& cache, int mapId) {
    if (const Gw2Map* map = cache.GetMap(mapId)) {
        return BuildLabel(*map);
    }
    if (mapId == 0) {
        return "";
    }
    // Translated as a whole sentence so the map id lands inside the brackets in
    // the active language.
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), T("Unknown map (%s)"), std::to_string(mapId).c_str());
    return std::string(buffer);
}

bool Draw(const char* label, int* mapId, const MapDataCache& cache) {
    if (!mapId) {
        return false;
    }

    bool changed = false;

    ImGui::PushID(label);
    PickerState& state = GetPickerState();

    if (*mapId != state.syncedMapId) {
        SyncInputFromMapId(state, cache, *mapId);
    }

    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-1.0f);

    const bool inputChanged =
        ImGui::InputText("##map_input", state.input, sizeof(state.input));
    const bool inputActive = ImGui::IsItemActive();

    if (inputActive || inputChanged) {
        ImGui::OpenPopup("##cm_map_picker_results");
    }

    if (ImGui::IsItemDeactivatedAfterEdit()) {
        int parsedId = 0;
        if (TryParseMapId(state.input, parsedId)) {
            *mapId = parsedId;
            SyncInputFromMapId(state, cache, *mapId);
            changed = true;
        } else {
            SyncInputFromMapId(state, cache, *mapId);
        }
    }

    ImGui::SetNextWindowSize(ImVec2(ImGui::GetItemRectSize().x, 0.0f));
    if (ImGui::BeginPopup("##cm_map_picker_results")) {
        RenderResultList(cache, state.input, mapId, state, changed);
        ImGui::EndPopup();
    }

    ImGui::PopID();
    return changed;
}

}  // namespace MapPickerUi
}  // namespace cm
