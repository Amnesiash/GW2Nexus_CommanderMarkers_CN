#pragma once

#include "core/Types.h"
#include "services/ModuleManifestService.h"

#include <nlohmann/json.hpp>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace cm {

class CommunityCatalogService {
public:
    static constexpr const char* kIndexFilename = "community_index.json";

    explicit CommunityCatalogService(std::string addonDir = {});

    void SetAddonDir(std::string addonDir);
    void SetManifest(const CommanderMarkersManifest* manifest);

    void LoadCached();
    // When force is true, skip lastEdit short-circuit and always refetch /sets.
    bool SyncCatalog(bool force = false);
    bool FetchSetDetail(const std::string& setId, MarkerSet& outSet, CommunitySetSummary* summary);

    std::vector<CommunityCategoryEntry> GetCategories() const;
    std::vector<CommunitySetSummary> GetSets() const;
    std::string LastEdit() const;
    std::string ServerUrl() const;

private:
    bool FetchAllPages();
    bool SaveIndex();
    static CommunitySetSummary SummaryFromJson(const nlohmann::json& j);
    static MarkerSet MarkerSetFromDetailJson(const nlohmann::json& j,
                                             const CommunitySetSummary* summary);

    std::string addonDir_;
    const CommanderMarkersManifest* manifest_ = nullptr;
    std::vector<CommunityCategoryEntry> categories_;
    std::vector<CommunitySetSummary> sets_;
    std::string lastEdit_;
    mutable std::mutex mutex_;
};

}  // namespace cm
