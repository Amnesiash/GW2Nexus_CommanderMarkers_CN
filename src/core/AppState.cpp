#include "core/AppState.h"

#include "core/Branding.h"
#include "core/Locale.h"
#include "services/RtApiService.h"
#include "ui/QuickAccessService.h"
#include "ui/SettingsWindow.h"
#include "ui/UiFontService.h"
#include "utils/MarkerBindHelper.h"

#include <filesystem>
#include <sstream>

namespace cm {

AppState::AppState()
    : mapData(""),
      markerListing(""),
      communityCatalog(""),
      previewImageCache(""),
      mapWatch(),
      placementService(nullptr) {}

AppState& AppState::Instance() {
    static AppState state;
    return state;
}

std::string AppState::settingsPath() const {
    return (std::filesystem::path(addonDir) / "settings.json").string();
}

std::string AppState::apiAccountsPath() const {
    return (std::filesystem::path(addonDir) / "api_accounts.json").string();
}

void AppState::CheckRequiredBinds() {
    if (!api) {
        requiredBindsOk = false;
        return;
    }

    const BindCheckResult result =
        cm::CheckRequiredBinds(api, settings.objectMarkersEnabled);
    requiredBindsOk = result.ok;

    if (!result.ok && !warnedUnboundBinds) {
        warnedUnboundBinds = true;
        std::ostringstream message;
        message << "Missing squad marker keybinds: ";
        for (size_t i = 0; i < result.missing.size(); ++i) {
            if (i > 0) {
                message << ", ";
            }
            message << result.missing[i];
        }
        api->Log(LOGL_WARNING, kLogChannel, message.str().c_str());
    }
}

void AppState::PrepareLoad(AddonAPI_t* apiPtr) {
    shutdownRequested_.store(false);
    api = apiPtr;
    // Hand the API to the locale module as early as possible: T() asks Nexus for
    // translations when the language is set to FollowNexus.
    Locale::SetApi(apiPtr);
    nexusLink = static_cast<NexusLinkData_t*>(api->DataLink_Get(DL_NEXUS_LINK));
    mumbleLink = static_cast<Mumble::Data*>(api->DataLink_Get(DL_MUMBLE_LINK));
    playerIdentity =
        static_cast<const Mumble::Identity*>(api->DataLink_Get(DL_MUMBLE_LINK_IDENTITY));
    RtApiService::Initialize(apiPtr);
}

void AppState::EnsureOptionsReady() {
    if (optionsReady || !api) {
        return;
    }

    addonDir = api->Paths_GetAddonDirectory("CommanderMarkers");
    settings.Load(settingsPath());
    Locale::SetLanguage(Locale::FromSettingsValue(settings.language));
    // Publish the bundled strings first, then ask Nexus which language it runs
    // so FollowNexus can pick it up.
    Locale::RegisterNexusStrings(api);
    Locale::DetectNexusLanguage(api);
    optionsReady = true;
}

void AppState::Shutdown() {
    HaltBackgroundWork();

    markerListing.SetOnMarkersChanged(nullptr);
    placementService.SetApi(nullptr);
    SettingsWindow::Shutdown(api);
    RtApiService::Shutdown();
    if (api) {
        UiFontService::Shutdown(api);
    }
    if (loadInitialized || optionsReady) {
        settings.Save(settingsPath());
    }

    ResetLoadState();
    api = nullptr;
}

void AppState::ProcessRenderInit() {
    if (renderInitialized || !api || !loadInitialized) {
        return;
    }

    QuickAccessService::SyncVisibility(api, *this);
    if (QuickAccessService::IsRegistered() || !settings.cornerIconEnabled) {
        renderInitialized = true;
        api->Log(LOGL_INFO, kLogChannel, "Render init complete.");
    }
}

void AppState::ProcessBackgroundNotices() {
    if (!api) {
        return;
    }

    if (communityUpdatedNotice.exchange(false)) {
        api->Log(LOGL_INFO, kLogChannel, "Community marker library updated.");
    }
    if (mapUpdatedNotice.exchange(false)) {
        api->Log(LOGL_INFO, kLogChannel, "Updated map data cache from GW2 API.");
    }
}

}  // namespace cm
