#include "services/CommunityCatalogService.h"

#include "core/Branding.h"
#include "data/MarkerSetJson.h"
#include "data/SyncBaselineHash.h"
#include "data/StaticDataLoader.h"
#include "services/HttpClient.h"
#include "utils/UuidUtils.h"

#include <nlohmann/json.hpp>
#include <sstream>

namespace cm {

CommunityCatalogService::CommunityCatalogService(std::string addonDir)
    : addonDir_(std::move(addonDir)) {}

void CommunityCatalogService::SetAddonDir(std::string addonDir) {
    addonDir_ = std::move(addonDir);
}

void CommunityCatalogService::SetManifest(const CommanderMarkersManifest* manifest) {
    manifest_ = manifest;
}

std::string CommunityCatalogService::ServerUrl() const {
    return manifest_ ? manifest_->serverUrl : kDefaultServerUrl;
}

std::string CommunityCatalogService::LastEdit() const {
    std::lock_guard lock(mutex_);
    return lastEdit_;
}

CommunitySetSummary CommunityCatalogService::SummaryFromJson(const nlohmann::json& j) {
    CommunitySetSummary summary{};
    summary.id = j.value("id", "");
    summary.categoryId = j.value("categoryId", 0);
    summary.categoryName = j.value("categoryName", "");
    summary.author = j.value("author", "");
    summary.name = j.value("name", "");
    summary.description = j.value("description", "");
    summary.mapId = j.value("mapId", 0);
    summary.mapName = j.value("mapName", "");
    summary.enabled = j.value("enabled", true);
    summary.previewThumbUrl = j.value("previewThumbUrl", "");
    summary.previewLargeUrl = j.value("previewLargeUrl", "");
    summary.updatedAt = j.value("updatedAt", "");
    return summary;
}

MarkerSet CommunityCatalogService::MarkerSetFromDetailJson(const nlohmann::json& j,
                                                           const CommunitySetSummary* summary) {
    MarkerSet markerSet = MarkerSetJson::MarkerSetFromJson(j);
    markerSet.id = GenerateUuidV4();
    if (summary) {
        markerSet.communitySetId = summary->id;
        markerSet.author = summary->author;
        markerSet.communityUpdatedAt = summary->updatedAt;
    } else {
        markerSet.communitySetId = j.value("id", "");
        markerSet.author = j.value("author", "");
        markerSet.communityUpdatedAt = j.value("updatedAt", "");
    }
    markerSet.source = "community";
    markerSet.syncDetached = false;
    markerSet.localModifiedAt.clear();
    markerSet.syncBaselineHash = SyncBaselineHash::Compute(markerSet);
    return markerSet;
}

void CommunityCatalogService::LoadCached() {
    std::string json;
    if (!StaticDataLoader::LoadCached(addonDir_, kIndexFilename, json)) {
        return;
    }
    try {
        const auto j = nlohmann::json::parse(json);
        std::lock_guard lock(mutex_);
        lastEdit_ = j.value("lastEdit", "");
        sets_.clear();
        if (j.contains("sets") && j["sets"].is_array()) {
            for (const auto& row : j["sets"]) {
                sets_.push_back(SummaryFromJson(row));
            }
        }
    } catch (...) {
    }
}

bool CommunityCatalogService::SaveIndex() {
    nlohmann::json j;
    {
        std::lock_guard lock(mutex_);
        j["lastEdit"] = lastEdit_;
        j["fetchedAt"] = CurrentIso8601Utc();
        j["sets"] = nlohmann::json::array();
        for (const auto& summary : sets_) {
            j["sets"].push_back({{"id", summary.id},
                                 {"categoryId", summary.categoryId},
                                 {"categoryName", summary.categoryName},
                                 {"author", summary.author},
                                 {"name", summary.name},
                                 {"description", summary.description},
                                 {"mapId", summary.mapId},
                                 {"mapName", summary.mapName},
                                 {"enabled", summary.enabled},
                                 {"previewThumbUrl", summary.previewThumbUrl},
                                 {"previewLargeUrl", summary.previewLargeUrl},
                                 {"updatedAt", summary.updatedAt}});
        }
    }
    return StaticDataLoader::WriteCached(addonDir_, kIndexFilename, j.dump(2));
}

bool CommunityCatalogService::SyncCatalog(bool force) {
    if (!manifest_) {
        return false;
    }

    // Match Blish: do not send If-None-Match. CDNs can answer 304 from a stale
    // cached ETag and skip origin, so Nexus would never see a new lastEdit.
    std::string cachedLastEdit;
    bool hasCachedSets = false;
    {
        std::lock_guard lock(mutex_);
        cachedLastEdit = lastEdit_;
        hasCachedSets = !sets_.empty();
    }

    const auto checkUrl = manifest_->Absolute(manifest_->communityCheckUrl);
    const auto checkResponse = HttpGetUrlEx(checkUrl);
    if (checkResponse.statusCode < 200 || checkResponse.statusCode >= 300) {
        return false;
    }

    try {
        const auto checkJson = nlohmann::json::parse(checkResponse.body);
        const std::string remoteLastEdit = checkJson.value("lastEdit", "");
        if (!force && !remoteLastEdit.empty() && remoteLastEdit == cachedLastEdit &&
            hasCachedSets) {
            return false;
        }
        if (!FetchAllPages()) {
            return false;
        }
        {
            std::lock_guard lock(mutex_);
            lastEdit_ = remoteLastEdit;
        }
        SaveIndex();
        return true;
    } catch (...) {
        return false;
    }
}

bool CommunityCatalogService::FetchAllPages() {
    std::vector<CommunitySetSummary> fetched;
    int offset = 0;
    const int limit = 200;
    int total = -1;

    while (total < 0 || offset < total) {
        std::ostringstream url;
        url << manifest_->Absolute(manifest_->setsUrl) << "?limit=" << limit << "&offset=" << offset;
        const auto body = HttpGetUrl(url.str());
        if (!body) {
            return false;
        }
        const auto j = nlohmann::json::parse(*body);
        total = j.value("total", 0);
        if (!j.contains("sets") || !j["sets"].is_array()) {
            break;
        }
        for (const auto& row : j["sets"]) {
            fetched.push_back(SummaryFromJson(row));
        }
        offset += limit;
        if (j["sets"].empty()) {
            break;
        }
    }

    const auto categoriesBody = HttpGetUrl(manifest_->Absolute(manifest_->categoriesUrl));
    std::vector<CommunityCategoryEntry> categories;
    if (categoriesBody) {
        try {
            const auto cats = nlohmann::json::parse(*categoriesBody);
            if (cats.is_array()) {
                for (const auto& row : cats) {
                    categories.push_back({row.value("id", 0), row.value("name", "")});
                }
            }
        } catch (...) {
        }
    }

    std::lock_guard lock(mutex_);
    sets_ = std::move(fetched);
    categories_ = std::move(categories);
    return true;
}

std::vector<CommunityCategoryEntry> CommunityCatalogService::GetCategories() const {
    std::lock_guard lock(mutex_);
    return categories_;
}

std::vector<CommunitySetSummary> CommunityCatalogService::GetSets() const {
    std::lock_guard lock(mutex_);
    return sets_;
}

bool CommunityCatalogService::FetchSetDetail(const std::string& setId,
                                             MarkerSet& outSet,
                                             CommunitySetSummary* summary) {
    if (!manifest_ || setId.empty()) {
        return false;
    }
    const auto url = manifest_->Resolve(manifest_->setDetailUrl, setId);
    const auto body = HttpGetUrl(url);
    if (!body) {
        return false;
    }
    try {
        const auto j = nlohmann::json::parse(*body);
        CommunitySetSummary localSummary = SummaryFromJson(j);
        outSet = MarkerSetFromDetailJson(j, summary ? summary : &localSummary);
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace cm
