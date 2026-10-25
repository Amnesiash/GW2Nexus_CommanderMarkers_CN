#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace cm {
namespace Locale {

// Every bundle below is a list of {"english source", "简体中文"} pairs. Each
// file lives under src/core/locale/ and owns the strings of one source module:
//   strings_options.inc   -> src/ui/OptionsPanel.cpp
//   strings_library.inc   -> src/ui/LibraryEditorUi.cpp, src/ui/CommunityLibraryUi.cpp
//   strings_editor.inc    -> src/ui/MarkerSetEditorWindow.cpp, src/ui/MapPickerUi.cpp
//   strings_overlay.inc   -> src/ui/MarkersPanel.cpp, src/ui/ScreenMapOverlay.cpp,
//                            src/ui/BillboardRenderer.cpp, src/ui/QuickAccessService.cpp
//   strings_messages.inc  -> src/ui/SettingsWindow.cpp, src/ui/OptionsApiTab.cpp,
//                            src/services/MarkerListing.cpp, src/core/AccountRegistry.cpp
// Add a new .inc plus one #include line here when a module gets translated.
using StringMap = std::unordered_map<std::string, std::string_view>;

inline const StringMap BuildChineseSimplified() {
    return StringMap{
#include "core/locale/strings_options.inc"
#include "core/locale/strings_library.inc"
#include "core/locale/strings_editor.inc"
#include "core/locale/strings_overlay.inc"
#include "core/locale/strings_messages.inc"
    };
}

}  // namespace Locale
}  // namespace cm
