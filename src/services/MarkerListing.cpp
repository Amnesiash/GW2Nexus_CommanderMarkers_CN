#include "services/MarkerListing.h"

#include "EmbeddedBuiltinIds.h"
#include "EmbeddedDefaults.h"
#include "data/MarkerSetJson.h"
#include "data/SyncBaselineHash.h"
#include "utils/UuidUtils.h"
#include "core/Locale.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace cm {

namespace {

// Short alias so the wrapped literals stay readable.
using cm::Locale::T;


struct BuiltinEntry {
    std::string id;
    std::string author;
    std::string name;
};

bool MarkerSetsEqual(const MarkerSet& a, const MarkerSet& b) {
    if (!a.communitySetId.empty() && a.communitySetId == b.communitySetId && !a.syncDetached &&
        !b.syncDetached) {
        return true;
    }
    return a.name == b.name && a.mapId == b.mapId && a.description == b.description &&
           a.trigger.x == b.trigger.x && a.trigger.y == b.trigger.y &&
           a.trigger.z == b.trigger.z && a.markers.size() == b.markers.size();
}

bool MarkerSetContentEqual(const MarkerSet& a, const MarkerSet& b) {
    if (a.name != b.name || a.description != b.description || a.mapId != b.mapId ||
        a.trigger.x != b.trigger.x || a.trigger.y != b.trigger.y || a.trigger.z != b.trigger.z ||
        a.markers.size() != b.markers.size()) {
        return false;
    }
    for (size_t i = 0; i < a.markers.size(); ++i) {
        const MarkerCoord& left = a.markers[i];
        const MarkerCoord& right = b.markers[i];
        if (left.icon != right.icon || left.name != right.name || left.x != right.x ||
            left.y != right.y || left.z != right.z) {
            return false;
        }
    }
    return true;
}

bool ReadFileText(const std::filesystem::path& path, std::string& outContent) {
    if (!std::filesystem::exists(path)) {
        return false;
    }
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }
    outContent.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !outContent.empty();
}

bool ParseListingJson(const std::string& json, MarkerListingFile& out) {
    try {
        out = MarkerSetJson::MarkerListingFileFromJson(nlohmann::json::parse(json));
        return !out.squadMarkerPreset.empty();
    } catch (...) {
        return false;
    }
}

std::vector<BuiltinEntry> LoadBuiltinMap() {
    std::vector<BuiltinEntry> rows;
    const char* json = EmbeddedBuiltinIds::Json();
    const std::size_t size = EmbeddedBuiltinIds::JsonSize();
    if (!json || size == 0) {
        return rows;
    }
    try {
        const auto j = nlohmann::json::parse(std::string(json, size));
        if (!j.is_array()) {
            return rows;
        }
        for (const auto& row : j) {
            rows.push_back({row.value("id", ""), row.value("author", ""), row.value("name", "")});
        }
    } catch (...) {
    }
    return rows;
}

const BuiltinEntry* MatchBuiltin(const std::vector<BuiltinEntry>& map, const std::string& name) {
    const std::string key = NormalizeNameKey(name);
    const BuiltinEntry* exact = nullptr;
    int matches = 0;
    for (const auto& row : map) {
        if (NormalizeNameKey(row.name) != key) {
            continue;
        }
        ++matches;
        if (row.name == name) {
            return &row;
        }
        exact = &row;
    }
    return matches == 1 ? exact : nullptr;
}

void MigratePreset(MarkerSet& preset, const std::vector<BuiltinEntry>& builtinMap) {
    if (preset.id.empty()) {
        preset.id = GenerateUuidV4();
    }
    if (!preset.communitySetId.empty()) {
        if (preset.source.empty()) {
            preset.source = preset.syncDetached ? "custom" : "community";
        }
        return;
    }
    if (const BuiltinEntry* match = MatchBuiltin(builtinMap, preset.name)) {
        preset.communitySetId = match->id;
        preset.author = match->author;
        preset.source = "builtin";
        preset.syncDetached = false;
        preset.syncBaselineHash = SyncBaselineHash::Compute(preset);
        return;
    }
    if (preset.source.empty()) {
        preset.source = "custom";
    }
}

void MigrateListing(MarkerListingFile& listing) {
    const auto builtinMap = LoadBuiltinMap();
    for (auto& preset : listing.squadMarkerPreset) {
        MigratePreset(preset, builtinMap);
    }
    listing.version = "3.0.0";
    listing.migratedAt = CurrentIso8601Utc();
}

}  // namespace

MarkerListing::MarkerListing(std::string addonDir) : addonDir_(std::move(addonDir)) {}

void MarkerListing::SetAddonDir(std::string addonDir) { addonDir_ = std::move(addonDir); }

void MarkerListing::SetOnMarkersChanged(std::function<void()> callback) {
    std::lock_guard lock(mutex_);
    onMarkersChanged_ = std::move(callback);
}

void MarkerListing::NotifyChanged() {
    if (suppressNotify_ || !onMarkersChanged_) {
        return;
    }
    onMarkersChanged_();
}

bool MarkerListing::LoadBuiltinDefaults(MarkerListingFile& out) {
    const char* json = EmbeddedDefaults::DefaultMarkersJson();
    const std::size_t size = EmbeddedDefaults::DefaultMarkersJsonSize();
    if (!json || size == 0) {
        return false;
    }
    if (!ParseListingJson(std::string(json, size), out)) {
        return false;
    }
    MigrateListing(out);
    return true;
}

void MarkerListing::ApplyListing(MarkerListingFile listing) {
    version_ = std::move(listing.version);
    presets_ = std::move(listing.squadMarkerPreset);
}

void MarkerListing::Load() {
    suppressNotify_ = true;
    const auto configPath = std::filesystem::path(addonDir_) / kFilename;
    if (!std::filesystem::exists(configPath)) {
        MarkerListingFile listing{};
        if (!LoadBuiltinDefaults(listing)) {
            suppressNotify_ = false;
            return;
        }
        ApplyListing(std::move(listing));
        Save();
        suppressNotify_ = false;
        return;
    }

    std::string fileText;
    if (!ReadFileText(configPath, fileText)) {
        MarkerListingFile listing{};
        if (LoadBuiltinDefaults(listing)) {
            ApplyListing(std::move(listing));
            Save();
        }
        suppressNotify_ = false;
        return;
    }

    try {
        MarkerListingFile listing =
            MarkerSetJson::MarkerListingFileFromJson(nlohmann::json::parse(fileText));
        if (listing.version == "1.0.0") {
            ResetToDefault();
            suppressNotify_ = false;
            return;
        }
        if (listing.version != "3.0.0") {
            MigrateListing(listing);
            ApplyListing(std::move(listing));
            Save();
            suppressNotify_ = false;
            return;
        }
        ApplyListing(std::move(listing));
    } catch (...) {
        MarkerListingFile listing{};
        if (LoadBuiltinDefaults(listing)) {
            ApplyListing(std::move(listing));
            Save();
        }
    }
    suppressNotify_ = false;
}

void MarkerListing::Save() {
    MarkerListingFile listing{};
    std::function<void()> changedCallback;
    {
        std::lock_guard lock(mutex_);
        listing.version = version_;
        listing.squadMarkerPreset = presets_;
        changedCallback = onMarkersChanged_;
    }

    const auto configPath = std::filesystem::path(addonDir_) / kFilename;
    if (configPath.has_parent_path()) {
        std::filesystem::create_directories(configPath.parent_path());
    }

    const auto json = MarkerSetJson::MarkerListingFileToJson(listing).dump(2);
    std::ofstream out(configPath);
    if (!out.is_open()) {
        return;
    }
    out << json;

    if (changedCallback && !suppressNotify_) {
        changedCallback();
    }
}

std::vector<MarkerSet> MarkerListing::GetAllMarkerSets() const {
    std::lock_guard lock(mutex_);
    return presets_;
}

std::vector<MarkerSet> MarkerListing::GetMarkersForMap(int mapId) const {
    std::lock_guard lock(mutex_);
    std::vector<MarkerSet> result;
    for (const auto& markerSet : presets_) {
        if (markerSet.mapId == mapId) {
            result.push_back(markerSet);
        }
    }
    return result;
}

void MarkerListing::SaveMarker(const MarkerSet& markerSet) {
    {
        std::lock_guard lock(mutex_);
        for (const auto& existing : presets_) {
            if (MarkerSetsEqual(existing, markerSet)) {
                return;
            }
        }
        presets_.push_back(markerSet);
    }
    Save();
}

bool MarkerListing::ContainsMarkerSet(const MarkerSet& markerSet) const {
    std::lock_guard lock(mutex_);
    for (const auto& existing : presets_) {
        if (MarkerSetsEqual(existing, markerSet)) {
            return true;
        }
    }
    return false;
}

bool MarkerListing::ContainsCommunitySetId(const std::string& communitySetId) const {
    if (communitySetId.empty()) {
        return false;
    }
    std::lock_guard lock(mutex_);
    for (const auto& existing : presets_) {
        if (existing.communitySetId == communitySetId) {
            return true;
        }
    }
    return false;
}

void MarkerListing::EditMarker(size_t index, const MarkerSet& markerSet) {
    MarkerSet updated = markerSet;
    {
        std::lock_guard lock(mutex_);
        if (index >= presets_.size()) {
            return;
        }
        const MarkerSet& existing = presets_[index];
        if (IsCommunityLinked(existing) && updated.id == existing.id &&
            !MarkerSetContentEqual(existing, updated)) {
            return;
        }
    }
    if (!updated.id.empty() && !updated.communitySetId.empty() && !updated.syncDetached) {
        updated.localModifiedAt = CurrentIso8601Utc();
    }
    {
        std::lock_guard lock(mutex_);
        if (index >= presets_.size()) {
            return;
        }
        presets_[index] = std::move(updated);
    }
    Save();
}

void MarkerListing::DeleteMarker(const MarkerSet& markerSet) {
    {
        std::lock_guard lock(mutex_);
        presets_.erase(
            std::remove_if(presets_.begin(), presets_.end(),
                           [&](const MarkerSet& existing) { return MarkerSetsEqual(existing, markerSet); }),
            presets_.end());
    }
    Save();
}

void MarkerListing::ResetToDefault() {
    MarkerListingFile listing{};
    if (!LoadBuiltinDefaults(listing)) {
        return;
    }
    ApplyListing(std::move(listing));
    version_ = "3.0.0";
    Save();
}

void MarkerListing::ReloadFromFile() { Load(); }

std::string MarkerListing::DisplayAuthor(const MarkerSet& markerSet) {
    if (!markerSet.author.empty()) {
        return markerSet.author;
    }
    if (!markerSet.communitySetId.empty()) {
        return markerSet.author;
    }
    return T("You");
}

bool MarkerListing::IsCommunityLinked(const MarkerSet& markerSet) {
    return !markerSet.communitySetId.empty() && !markerSet.syncDetached;
}

bool MarkerListing::IsShareableWithCommunity(const MarkerSet& markerSet) {
    if (IsCommunityLinked(markerSet)) {
        return false;
    }
    return markerSet.source != "builtin";
}

MarkerSet MarkerListing::DuplicateAsEditableCopy(const MarkerSet& markerSet) {
    MarkerSet copy = markerSet;
    copy.id = GenerateUuidV4();
    copy.communitySetId.clear();
    copy.communityUpdatedAt.clear();
    copy.localModifiedAt.clear();
    copy.author.clear();
    copy.syncDetached = false;
    copy.source = "custom";
    copy.syncBaselineHash = SyncBaselineHash::Compute(copy);
    return copy;
}

}  // namespace cm
