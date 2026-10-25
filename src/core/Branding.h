#pragma once

#include <cstdint>

namespace cm {

// Nexus signature 0xFE71B519 (Markers)
inline constexpr int32_t kSignature = 0xFE71B519;
// Name shown in the Nexus addon library, the options window and the corner icon
// tooltip. The addon data directory (AppState::EnsureOptionsReady) keys off the
// separate "CommanderMarkers" literal, so renaming here does not migrate or
// orphan existing settings.
inline constexpr const char* kDisplayName = "Commander Marker CN";
inline constexpr const char* kLogChannel = "Commander Markers";
inline constexpr const char* kAuthor = "Soeed、vaon";
// English source of truth: shown by Nexus in the addon library (entry.cpp) and
// on the addon's config page (OptionsPanel::RenderNexusConfigEntry). Both call
// sites display it through T(), so the Chinese text below is what players see
// with the default language.
inline constexpr const char* kDescription =
    "Place squad markers with a click, or auto-place saved marker sets.";
inline constexpr const char* kSourceRepoUrl =
    "https://github.com/manlaan/BlishHud-CommanderMarkers";
inline constexpr const char* kCommunityMarkersUrl =
    "https://bhm.blishhud.com/Manlaan.CommanderMarkers/Community/Markers.json";

#if defined(CM_LOCAL_TEST)
inline constexpr bool kLocalTest = true;
inline constexpr const char* kManifestUrl =
    "http://localhost:3000/commander_markers_v1.json";
inline constexpr const char* kDefaultServerUrl = "http://localhost:3000";
#else
inline constexpr bool kLocalTest = false;
inline constexpr const char* kManifestUrl =
    "https://addons.soeed.com/commander_markers_v1.json";
inline constexpr const char* kDefaultServerUrl = "https://addons.soeed.com";
#endif

inline constexpr const char* kPatchNotesUrl =
    "https://pkgs.blishhud.com/Manlaan.CommanderMarkers.html";
inline constexpr const char* kUpdateLink =
    "https://github.com/a727891/GW2Nexus-commanderMarkers";

}  // namespace cm
