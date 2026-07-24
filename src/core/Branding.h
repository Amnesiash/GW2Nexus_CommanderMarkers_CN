#pragma once

#include <cstdint>

namespace cm {

// Nexus signature 0x60018002 (Markers, index 02)
inline constexpr int32_t kSignature = 0x60018002;
inline constexpr const char* kDisplayName = "Commander Markers";
inline constexpr const char* kLogChannel = "Commander Markers";
inline constexpr const char* kAuthor = "Soeed.4160";
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
