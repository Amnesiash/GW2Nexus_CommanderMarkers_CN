#include "core/AppState.h"

#include "core/Branding.h"
#include "services/Gw2MapApiClient.h"
#include "core/AppStateDetail.h"

#include <chrono>
#include <sstream>
#include <thread>

namespace cm {

void AppState::WaitForBackgroundWork() {
    using namespace std::chrono_literals;
    const auto deadline = std::chrono::steady_clock::now() + 20s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (!manifestFetchInProgress.load() && !communitySyncInProgress.load() &&
            !mapRefreshInProgress.load()) {
            return;
        }
        std::this_thread::sleep_for(10ms);
    }

    if (api) {
        api->Log(LOGL_WARNING, kLogChannel,
                 "Unload timed out waiting for background work to finish.");
    }
}

void AppState::HaltBackgroundWork() {
    shutdownRequested_.store(true);
    WaitForBackgroundWork();
}

void AppState::RequestCommunitySync(bool force) {
    communitySyncForce.store(force);
    communitySyncPending.store(true);
}

void AppState::ProcessPendingCommunitySync() {
    if (!loadInitialized || !communitySyncPending.load() || shutdownRequested_.load()) {
        return;
    }
    if (communitySyncInProgress.exchange(true)) {
        return;
    }
    communitySyncPending.store(false);
    const bool force = communitySyncForce.exchange(false);

    std::thread([this, force]() {
        detail::BackgroundWorkGuard guard(communitySyncInProgress);

        if (shutdownRequested_.load()) {
            return;
        }

        const bool updated = communityCatalog.SyncCatalog(force);
        if (shutdownRequested_.load()) {
            return;
        }

        if (kLocalTest && api) {
            const auto count = communityCatalog.GetSets().size();
            if (updated) {
                std::ostringstream msg;
                msg << "Local test catalog synced: " << count << " set(s) from "
                    << moduleManifest.Get().serverUrl;
                api->Log(LOGL_INFO, kLogChannel, msg.str().c_str());
            } else if (count == 0) {
                api->Log(LOGL_WARNING, kLogChannel,
                         "Local test catalog sync returned no sets — is the CM server running?");
            }
        }

        if (updated) {
            communityUpdatedNotice.store(true);
        }
    }).detach();
}

void AppState::RequestMapRefresh() {
    mapRefreshPending.store(true);
}

void AppState::ProcessPendingMapRefresh() {
    if (!loadInitialized) {
        return;
    }

    const int buildId = mumbleLink ? static_cast<int>(mumbleLink->Context.BuildID) : 0;
    if (buildId != 0 && buildId != lastMapRefreshBuildId) {
        lastMapRefreshBuildId = buildId;
        if (Gw2MapApiClient::NeedsRefresh(mapData, buildId)) {
            RequestMapRefresh();
        }
    }

    if (!mapRefreshPending.load() || shutdownRequested_.load()) {
        return;
    }
    if (buildId == 0) {
        return;
    }
    if (mapRefreshInProgress.exchange(true)) {
        return;
    }
    mapRefreshPending.store(false);

    std::thread([this, buildId]() {
        detail::BackgroundWorkGuard guard(mapRefreshInProgress);

        if (shutdownRequested_.load()) {
            return;
        }

        if (Gw2MapApiClient::Refresh(mapData, buildId, nullptr) && !shutdownRequested_.load()) {
            mapUpdatedNotice.store(true);
        }
    }).detach();
}

}  // namespace cm
