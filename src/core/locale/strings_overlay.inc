// HUD overlays, clickable marker panel, world billboards and the Nexus
// quick-access icon.
//
// Owned source files:
//   src/ui/MarkersPanel.cpp
//   src/ui/ScreenMapOverlay.cpp
//   src/ui/BillboardRenderer.cpp
//   src/ui/QuickAccessService.cpp
//
// Rule: marker names (Arrow / Circle / Heart / Square / Star / Cross / Spiral /
// Triangle / X / Clear ...) come from the game's own marker data and are NOT
// translated. Only the UI chrome around them is.

// --- MarkersPanel.cpp ---
// "Drag" -> 拖拽, kept consistent with "Drag panel to reposition" -> 拖拽面板调整位置
{"Drag", "拖拽"},
// snprintf-built per-marker tooltips: "<MarkerName> Ground" / "<MarkerName> Object"
{"%s Ground", "%s 地面标记"},
{"%s Object", "%s 物件标记"},

// --- ScreenMapOverlay.cpp / BillboardRenderer.cpp (shared interact prompt) ---
{"Press '%s' to place markers\n%s", "按「%s」放置标记\n%s"},

// --- QuickAccessService.cpp: Nexus corner icon ---
{"Open Settings", "打开设置"},
{"Lieutenant mode", "副官模式"},
// "Library" -> 库, consistent with "Library Editor" -> 库编辑器 and
// "Community Library" -> 社区库
{"Library (current map)", "库（当前地图）"},
{"Hide Clickable Markers", "隐藏可点击标记"},
{"Show Clickable Markers", "显示可点击标记"},
{"\nUse the context menu for library and settings.", "\n通过右键菜单打开标记库与设置。"},
