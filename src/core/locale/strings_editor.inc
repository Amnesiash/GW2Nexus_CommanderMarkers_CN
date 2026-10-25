// Marker set editor + map picker (src/ui/MarkerSetEditorWindow.cpp, src/ui/MapPickerUi.cpp)

// --- Window titles (composed with an immutable "###ID" suffix) ---
{"Create/edit markers", "创建/编辑标记"},
{"Unsaved Marker Set Changes", "未保存的更改"},
{"Delete Marker Set", "删除标记组"},

// --- Meta / info ---
{"Name", "名称"},
{"Description", "描述"},
{"Map", "地图"},
{"Trigger", "触发器"},
{" (personal)", "（个人）"},

// --- Marker table ---
// "Label" 是 ImGui 输入提示（hint）。
{"Label", "标签"},
{"Optional label shown with this marker in the set.", "该标记在标记组中显示的可选标签。"},
{"World X (east/west).", "世界坐标 X（东西方向）。"},
{"World Y (north/south).", "世界坐标 Y（南北方向）。"},
{"World Z (height/elevation).", "世界坐标 Z（高度／海拔）。"},
{"Remove this marker from the set", "从标记组中移除该标记"},
{"Set Location", "设置位置"},
{"Set Trigger Location", "设置触发器位置"},
{"Trigger world position (X east/west, Y north/south, Z height).", "触发器世界坐标（X 东西、Y 南北、Z 高度）。"},
{"Use your current player position for this marker.", "使用你当前角色位置作为该标记位置。"},
{"Updated when you use Set Trigger Location.", "使用「设置触发器位置」后更新。"},
{"Snap trigger to your current position and update the map ID.", "将触发器对齐到你当前位置并更新地图 ID。"},

// --- Real-Time API import ---
{"Import active squad markers", "导入当前小队标记"},

// --- Bottom action bar ---
{"Save", "保存"},
{"Cancel", "取消"},
{"Close", "关闭"},
{"Export", "导出"},
{"Copy a BlishHUD-compatible Base64 marker set share code to the clipboard.",
 "复制一个 BlishHUD 兼容的 Base64 标记组分享码到剪贴板。"},

// --- Import dialog ---
{"Import Marker Set", "导入标记组"},
{"Paste a marker set share code from BlishHUD or this module.", "粘贴来自 BlishHUD 或本模块的标记组分享码。"},
{"Paste Clipboard", "粘贴剪贴板"},

// --- Validation ---
// "Marker set name is required." 与 "Marker set must have a valid map." 归
// strings_library.inc（社区分享校验共用），此处不重复定义。
{"Name is required.", "必须填写名称。"},
{"Map is invalid. Set trigger location while on a map.", "地图无效，请在地图内设置触发器位置。"},
{"Trigger location is required. Use Set Trigger Location.", "必须设置触发器位置，请使用「设置触发器位置」。"},
{"One or more markers have invalid coordinates.", "一个或多个标记的坐标无效。"},
{"At least one marker with a position is required.", "至少需要一个带位置的标记。"},

// --- Name / Description field tooltips ---
{"Name shown on the map when you are near this marker set's trigger.",
 "靠近该标记组触发器时在地图上显示的名称。"},
{"Description shown on the map when you are near this marker set's trigger.",
 "靠近该标记组触发器时在地图上显示的描述。"},

// --- Delete confirmation ---
{"\"%s\" will be removed from your library.", "「%s」将从你的库中移除。"},
{"Delete this marker set?", "删除这个标记组？"},

// --- Unsaved-changes dialog ---
{"You have unsaved changes to this marker set.", "这个标记组有未保存的更改。"},
{"Save your changes, discard them, or keep editing.", "保存更改、放弃更改，或继续编辑。"},

// --- MapPickerUi.cpp ---
{"Showing first %d matches. Refine your search.", "仅显示前 %d 条匹配结果，请细化搜索条件。"},
{"No maps match your search.", "没有匹配的地图。"},
// 源码为 "Unknown map (" + std::to_string(mapId) + ")"，按整句翻译后拼入地图 ID。
{"Unknown map (%s)", "未知地图（%s）"},
