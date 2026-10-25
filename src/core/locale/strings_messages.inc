// Player-facing messages, status text and window titles.
//
// Owned source files:
//   src/ui/SettingsWindow.cpp      -- settings window title (ImGui "label###ID" split)
//   src/ui/OptionsApiTab.cpp       -- GW2 API key section: labels, buttons, status messages
//   src/core/AccountRegistry.cpp   -- RegisterKey() result messages
//   src/services/MarkerListing.cpp -- "You" fallback author name
//
// Audited and intentionally left untranslated (no player-visible literals):
//   src/core/AppStateInit.cpp      -- api->Log developer/local-test logs only
//   src/core/AppState.cpp          -- api->Log developer notices only
//   src/core/AppStateBackground.cpp-- api->Log developer/local-test logs only
//   src/services/MapWatchService.cpp-- no string literals at all
//   src/core/MumbleUtils.cpp       -- no string literals at all
//   src/core/SettingsStore.cpp     -- JSON keys only, no visible copy
//
// NOTE: keys must match the *concatenated* C++ literal exactly, because
// scripts/wrap_strings.js merges adjacent literals before wrapping them in T().
//
// Terminology (kept consistent with strings_options.inc / strings_library.inc):
//   API key -> API 密钥     account -> 账户       community library -> 社区库
//   marker set -> 标记组    Community Library -> 社区库

// --- SettingsWindow.cpp: window title rendered through ImGui::Begin() ---
{"Commander Marker CN Settings", "Commander Marker CN 设置"},

// --- OptionsApiTab.cpp: GW2 API key section ---
{"GW2 API key", "GW2 API 密钥"},
{"Required to share marker sets with the community library. The key needs account permission.",
 "分享标记组到社区库需要 API 密钥，密钥必须具备 account 权限。"},
{"API key", "API 密钥"},
{"Register key", "注册密钥"},
{"Clear active key", "清除当前密钥"},
{"Removed active API key.", "已移除当前 API 密钥。"},
{"No API key registered.", "尚未注册 API 密钥。"},
{"Active account: %s", "当前账户：%s"},

// --- OptionsApiTab.cpp: share section ---
// "Share with community" itself is owned by strings_library.inc and is reused
// verbatim from that bundle; not duplicated here.
{"Use Share with community in the Community Library tab when an API key is registered.",
 "注册 API 密钥后，请在「社区库」标签页中使用「分享到社区」。"},
{"Open web library", "打开网页版社区库"},

// --- AccountRegistry::RegisterKey(): status text built at the source ---
// AccountRegistry.cpp translates these itself (T() / snprintf) so every caller
// shows the localised form; the keys stay here with the other player messages.
{"API key is empty.", "API 密钥不能为空。"},
{"API key validation failed.", "API 密钥校验失败。"},
{"API key must have account permission.", "API 密钥必须具备 account 权限。"},
{"Failed to fetch account name.", "获取账户名失败。"},
{"Registered %s.", "已注册 %s。"},

// --- MarkerListing.cpp: fallback author name shown as "Author: %s" ---
{"You", "你"},
