// Library editor + community library (src/ui/LibraryEditorUi.cpp, src/ui/CommunityLibraryUi.cpp)

// --- LibraryEditorUi.cpp: headings / subtext / filters ---
{"AutoMarker Library", "自动标记库"},
{"Manage saved marker sets. Imported community sets are read-only — use Personalize to create your own editable copy.",
  "管理已保存的标记组。从社区导入的标记组为只读——用「个性化」创建属于你的可编辑副本。"},
{"Current map", "当前地图"},
{"Only show marker sets for your current map.", "只显示当前地图的标记组。"},
{"Mine", "我的"},
{"Hide marker sets imported from the community library.", "隐藏从社区库导入的标记组。"},

// --- LibraryEditorUi.cpp: toolbar ---
{"Add New", "新建"},
{"Reset Library To Default", "重置库为默认"},
{"Hold Ctrl+Shift to reset the library to defaults.", "按住 Ctrl+Shift 将库重置为默认设置。"},
{"Search", "搜索"},
{"Filter by marker set name, description, or map name.", "按标记组名称、描述或地图名筛选。"},

// --- LibraryEditorUi.cpp: empty state ---
{"No marker sets match the current filters.", "没有符合当前筛选条件的标记组。"},

// --- LibraryEditorUi.cpp: row details ---
{"Map: %s | Markers: %zu", "地图：%s | 标记：%zu"},
{"Author: %s", "作者：%s"},

// --- LibraryEditorUi.cpp: row action buttons ---
{"Edit", "编辑"},
{"Personalize", "个性化"},
{"Open the editor with this set as a template. Save to add your personalized copy.",
  "以此标记组为模板打开编辑器。保存后即可加入你的个性化副本。"},
{"Delete", "删除"},
{"Remove this imported set from your library", "把这个导入的标记组从你的库中移除"},
{"Hover to preview markers on the map", "悬停可在地图上预览标记"},
{"Place", "放置"},
{"Place squad markers for this set.", "放置这个标记组的小队标记。"},

// --- LibraryEditorUi.cpp: status toggle ---
{"Active", "已激活"},
{"Disabled", "已禁用"},
{"Click to disable this marker set", "点击禁用此标记组"},
{"Click to enable this marker set", "点击启用此标记组"},

// --- CommunityLibraryUi.cpp: headings / subtext / gate ---
{"Community Library", "社区库"},
{"Browse and import marker sets from the GeoGuesser community library.",
  "浏览并从 GeoGuesser 社区库导入标记组。"},
{"Community library not loaded yet.", "社区库尚未加载。"},
{"Download Community Library", "下载社区库"},
{"Redownload", "重新下载"},
{"Hold Ctrl+Shift to force a community library redownload.", "按住 Ctrl+Shift 强制重新下载社区库。"},
{"Category", "分类"},
{"Filter to current map", "只显示当前地图"},
{"Only Show available", "只显示可导入"},
{"Hide marker sets already in your library.", "隐藏已在库中的标记组。"},

// --- CommunityLibraryUi.cpp: row ---
{"Map: %s", "地图：%s"},
{"Import", "导入"},
{"Import this community marker set into your library.", "把这个社区标记组导入到你的库。"},
{"Already in your library.", "已在你的库中。"},
{"Preview", "预览"},
{"Stop", "停止"},

// --- CommunityLibraryUi.cpp: category filter ---
{"All categories", "全部分类"},

// --- CommunityLibraryUi.cpp: share section ---
{"Share with community", "分享到社区"},
{"Register a GW2 API key to share your marker sets with the community library.",
  "请先注册 GW2 API 密钥，才能把你的标记组分享到社区库。"},
{"Share your custom marker sets with the community library.", "把你的自定义标记组分享到社区库。"},
{"No custom marker sets available to share.", "没有可分享的自定义标记组。"},
{"Custom...", "自定义..."},
{"Pick an existing category or choose Custom to type your own.", "选择已有分类，或选择「自定义」自行输入。"},
{"Type category name", "输入分类名称"},
{"Enter a category name.", "请输入分类名称。"},
{"Marker set name is required.", "必须填写标记组名称。"},
{"Marker set must have a valid map.", "标记组必须绑定有效地图。"},
{"Register a GW2 API key below first.", "请先在下方注册 GW2 API 密钥。"},
{"Failed to obtain share token from server.", "无法从服务器获取分享令牌。"},
{"Share failed", "分享失败"},
// 源码为 fallback + " (" + statusCode + ")."，按整句翻译后拼入。
{"%s (%d).", "%s（状态码 %d）。"},
{"Sent for moderator review.", "已提交，等待管理员审核。"},
{"Sharing...", "分享中..."},
{"Uploading marker set for moderator review.", "正在上传标记组，等待管理员审核。"},

// --- CommunityLibraryUi.cpp: import notifications (GUI_SendAlert) ---
{"Imported community marker set into your library.", "已将社区标记组导入到你的库。"},
{"Imported \"%s\" into your library.", "已将「%s」导入到你的库。"},
