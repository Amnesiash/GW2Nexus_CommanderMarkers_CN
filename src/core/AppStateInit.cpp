#include "core/AppState.h"

#include "core/Branding.h"
#include "core/MumbleUtils.h"
#include "services/Gw2MapApiClient.h"
#include "services/HttpClient.h"
#include "core/AppStateDetail.h"
#include "ui/DatAssetIconService.h"
#include "ui/TextureService.h"
#include "ui/UiFontService.h"

#include <sstream>
#include <thread>

namespace cm {
namespace {

HttpRequestOptions ManifestHttpOptions() {
    HttpRequestOptions opts;
#if defined(CM_LOCAL_TEST)
    opts.connectTimeoutMs = 750;
    opts.readTimeoutMs = 3000;
#endif
    return opts;
}

}  // namespace

bool AppState::AdvanceMumbleStability() {
    if (!mumbleLink) {
        stableFrameCount = 0;
        return false;
    }

    const uint32_t mapId = mumbleLink->Context.MapID;
    const uint32_t tick = mumbleLink->UITick;
    if (mapId == 0 || tick == 0 || mumbleLink->Context.Compass.Width < 1 ||
        mumbleLink->Context.Compass.Height < 1) {
        stableMapId = 0;
        stableMumbleTick = 0;
        stableFrameCount = 0;
        return false;
    }

    if (mapId == stableMapId) {
        if (tick != stableMumbleTick) {
            stableMumbleTick = tick;
            stableFrameCount++;
        }
    } else {
        stableMapId = mapId;
        stableMumbleTick = tick;
        stableFrameCount = 1;
    }

    return stableFrameCount >= kStableFramesRequired;
}

void AppState::WireManifestServices() {
    communityCatalog.SetManifest(&moduleManifest.Get());
    subtokenService.SetManifest(&moduleManifest.Get());
    previewImageCache.SetServerUrl(moduleManifest.Get().serverUrl);
}

void AppState::LogLocalTestManifest(const ManifestFetchResult& result, bool succeeded) {
    if (!kLocalTest || !api) {
        return;
    }

    std::ostringstream msg;
    msg << "Local test manifest " << (succeeded ? "OK" : "FAILED") << " — " << kManifestUrl
        << " (server " << moduleManifest.Get().serverUrl << ")";
    api->Log(succeeded ? LOGL_INFO : LOGL_WARNING, kLogChannel, msg.str().c_str());

    if (result.communityCheckStatus == 0) {
        return;
    }

    const auto checkUrl = moduleManifest.Get().Absolute(moduleManifest.Get().communityCheckUrl);
    if (result.communityCheckStatus >= 200 && result.communityCheckStatus < 300) {
        std::ostringstream ok;
        ok << "Local test community check OK: " << checkUrl;
        api->Log(LOGL_INFO, kLogChannel, ok.str().c_str());
        return;
    }

    std::ostringstream err;
    err << "Local test community check failed (HTTP " << result.communityCheckStatus
        << "): " << checkUrl;
    api->Log(LOGL_WARNING, kLogChannel, err.str().c_str());
}

void AppState::StartManifestFetch() {
    if (manifestFetchStarted_ || shutdownRequested_.load()) {
        return;
    }
    manifestFetchStarted_ = true;
    manifestFetchInProgress.store(true);

    std::thread([this]() {
        detail::BackgroundWorkGuard guard(manifestFetchInProgress);

        const HttpRequestOptions opts = ManifestHttpOptions();
        ManifestFetchResult result;
        result.body = HttpGetUrl(kManifestUrl, opts);

        if (shutdownRequested_.load()) {
            return;
        }

        if (kLocalTest && result.body) {
            if (const auto parsed = ModuleManifestService::ParseBody(*result.body)) {
                const auto checkUrl = parsed->Absolute(parsed->communityCheckUrl);
                result.communityCheckStatus = HttpGetUrlEx(checkUrl, opts).statusCode;
            }
        }

        if (shutdownRequested_.load()) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(manifestFetchMutex_);
            manifestFetchResult_ = std::move(result);
        }
        manifestFetchComplete.store(true);
    }).detach();
}

void AppState::ProcessManifestFetchResult() {
    if (!manifestFetchComplete.load() || manifestFetchApplied.load() || !api ||
        shutdownRequested_.load()) {
        return;
    }

    ManifestFetchResult result;
    {
        std::lock_guard<std::mutex> lock(manifestFetchMutex_);
        result = std::move(manifestFetchResult_);
    }
    manifestFetchApplied.store(true);

    const bool succeeded = result.body && moduleManifest.ApplyFromBody(*result.body);
    WireManifestServices();

    if (loadInitialized && succeeded) {
        RequestCommunitySync();
    }

    LogLocalTestManifest(result, succeeded);
}

void AppState::InitStepPrepare() {
    EnsureOptionsReady();
    mapData.SetAddonDir(addonDir);
    markerListing.SetAddonDir(addonDir);
    communityCatalog.SetAddonDir(addonDir);
    previewImageCache.SetAddonDir(addonDir);
    moduleManifest.LoadDefaults();
    WireManifestServices();
    StartManifestFetch();
    accountRegistry.Load(apiAccountsPath());
    markerListing.SetOnMarkersChanged(nullptr);
    placementService.SetApi(api);
    initStep = 1;
}

void AppState::InitStepMapData() {
    mapData.LoadFromDisk();
    if (mumbleLink) {
        const int mapId = static_cast<int>(mumbleLink->Context.MapID);
        if (mapId != 0 && !mapData.MapHasGeometry(mapId)) {
            mapData.SetBuildId(0);
            RequestMapRefresh();
        }
    }
    initStep = 2;
}

void AppState::InitStepMarkers() {
    markerListing.Load();
    initStep = 3;
}

void AppState::InitStepServices() {
    communityCatalog.LoadCached();
    mapWatch.Initialize(&mapData, &markerListing, &placementService, &settings, &playerIdentity);
    TextureService::Initialize(api, addonDir);
    DatAssetIconService::Initialize(api, addonDir);
    UiFontService::Initialize(api, nexusLink);
    initStep = 4;
}

void AppState::InitStepFinalize() {
    CheckRequiredBinds();

    const int buildId = mumbleLink ? static_cast<int>(mumbleLink->Context.BuildID) : 0;
    if (Gw2MapApiClient::NeedsRefresh(mapData, buildId)) {
        RequestMapRefresh();
    }

    RequestCommunitySync();
    loadInitialized = true;
    renderInitialized = false;
    initStep = 5;
    api->Log(LOGL_INFO, kLogChannel, "Initialized.");
}

void AppState::RunDeferredInitStep() {
    switch (initStep) {
        case 0:
            InitStepPrepare();
            break;
        case 1:
            InitStepMapData();
            break;
        case 2:
            InitStepMarkers();
            break;
        case 3:
            InitStepServices();
            break;
        case 4:
            InitStepFinalize();
            break;
        default:
            break;
    }
}

void AppState::ProcessDeferredInit() {
    if (!api || shutdownRequested_.load()) {
        return;
    }

    ProcessManifestFetchResult();

    if (loadInitialized) {
        return;
    }

    if (!IsWorldReady(mumbleLink, nexusLink)) {
        stableFrameCount = 0;
        return;
    }
    if (!AdvanceMumbleStability()) {
        return;
    }

    RunDeferredInitStep();
}

void AppState::ResetLoadState() {
    loadInitialized = false;
    renderInitialized = false;
    optionsReady = false;
    initStep = 0;
    stableMapId = 0;
    stableMumbleTick = 0;
    stableFrameCount = 0;
    manifestFetchStarted_ = false;
    manifestFetchInProgress.store(false);
    manifestFetchComplete.store(false);
    manifestFetchApplied.store(false);
    {
        std::lock_guard<std::mutex> lock(manifestFetchMutex_);
        manifestFetchResult_ = {};
    }
    communitySyncPending.store(false);
    communitySyncForce.store(false);
    mapRefreshPending.store(false);
}

}  // namespace cm
