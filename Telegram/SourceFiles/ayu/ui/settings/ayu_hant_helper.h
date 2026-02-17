#pragma once

#include "lang/lang_keys.h"
#include <QtCore/QString>
#include <QtCore/QMap>

namespace {

inline QString AyuHantHelper(const QString &key, const QString &fallback) {
	static const QMap<QString, QString> dict = {
		// Ayu Settings
		{"ayu_GhostEssentialsHeader", "幽靈模式核心設定"},
		{"ayu_DontReadMessages", "讀取訊息時不標記已讀"},
		{"ayu_DontReadStories", "觀看限時動態時不標記已讀"},
		{"ayu_DontSendOnlinePackets", "不發送上線狀態"},
		{"ayu_DontSendUploadProgress", "隱藏正在上傳狀態"},
		{"ayu_SendOfflinePacketAfterOnline", "上線後立即發送離線封包"},
		{"ayu_GhostModeToggle", "幽靈模式"},
		{"ayu_MarkReadAfterAction", "互動後自動標記已讀"},
		{"ayu_MarkReadAfterActionDescription", "當您發送新訊息或進行貼圖回應時，自動將該對話標記為已讀。"},
		{"ayu_UseScheduledMessages", "改用排程訊息"},
		{"ayu_UseScheduledMessagesDescription", "自動將送出的訊息轉為排程發送（預設延遲 12 秒）。使用此功能時，您不會顯示為上線狀態。請避免在不穩定的網路環境下使用。"},
		{"ayu_SendWithoutSoundByDefault", "預設無聲發送"},
		{"ayu_SendWithoutSoundByDefaultDescription", "自動以無聲方式發送所有訊息。"},
		{"ayu_SpyEssentialsHeader", "間諜模式核心設定"},
		{"ayu_SaveDeletedMessages", "保留已刪除訊息"},
		{"ayu_SaveMessagesHistory", "保留訊息編輯紀錄"},
		{"ayu_MessageSavingSaveForBots", "在機器人對話中儲存"},
		{"ayu_MessageSavingExcludeBotsInGroups", "排除群組中的機器人"},
		{"ayu_MessageSavingOtherHeader", "其他選項"},
		{"ayu_LocalPremium", "本地解鎖 Premium"},
		{"ayu_DisableAds", "停用廣告"},
		{"ayu_ImproveDC5Connection", "優化 DC5 連線路徑"},

		// General Settings
		{"ayu_CategoryGeneral", "一般設定"},
		{"ayu_TranslationProvider", "翻譯服務提供者"},
		{"ayu_SettingsShowID", "顯示 ID"},
		{"ayu_SettingsShowID_Hide", "隱藏"},
		{"ayu_DisableStories", "停用限時動態"},
		{"ayu_DisableOpenLinkWarning", "停用外部連結警告"},
		{"ayu_DisableSimilarChannels", "停用相似頻道推薦"},
		{"ayu_CollapseSimilarChannels", "摺疊相似頻道"},
		{"ayu_HideSimilarChannelsTab", "隱藏相似頻道分頁"},
		{"ayu_DisableNotificationsDelay", "停用通知延遲"},
		{"ayu_SettingsShowMessageSeconds", "顯示訊息精確秒數"},
		{"ayu_SettingsSpoofWebviewAsAndroid", "將 Webview 模擬為 Android 裝置"},
		{"ayu_SettingsBiggerWindow", "加大 Webview 視窗"},
		{"ayu_SettingsIncreaseWebviewHeight", "增加高度"},
		{"ayu_SettingsIncreaseWebviewWidth", "增加寬度"},
		{"ayu_ConfirmationsTitle", "操作確認"},
		{"ayu_StickerConfirmation", "發送貼圖前確認"},
		{"ayu_GIFConfirmation", "發送 GIF 前確認"},
		{"ayu_VoiceConfirmation", "發送語音訊息前確認"},

		// Appearance Settings
		{"ayu_CategoryAppearance", "外觀設定"},
		{"ayu_AppIconHeader", "應用程式圖示"},
		{"ayu_HideNotificationBadge", "隱藏通知標記"},
		{"ayu_HideNotificationBadgeDescription", "隱藏工作列圖示上的未讀計數標記。"},
		{"ayu_MaterialSwitches", "使用 Material Design 風格開關"},
		{"ayu_RemoveMessageTail", "移除對話氣泡尾巴"},
		{"ayu_DisableCustomBackgrounds", "停用自訂對話背景"},
		{"ayu_MonospaceFont", "等寬字型"},
		{"ayu_FontDefault", "系統預設"},
		{"ayu_ChatFoldersHeader", "對話分頁設定"},
		{"ayu_HideNotificationCounters", "隱藏通知計數"},
		{"ayu_HideAllChats", "隱藏「所有對話」分頁"},
		{"ayu_DrawerElementsHeader", "側選單項目"},
		{"ayu_LReadMessages", "本地標記已讀 (不影響伺服器)"},
		{"ayu_SReadMessages", "伺服器同步已讀"},
		{"ayu_StreamerModeToggle", "實況主模式"},
		{"ayu_TrayElementsHeader", "系統匣項目"},
		{"ayu_EnableGhostModeTray", "在系統匣顯示幽靈模式開關"},
		{"ayu_EnableStreamerModeTray", "在系統匣顯示實況主模式開關"},

		// Chats Settings
		{"ayu_CategoryChats", "對話設定"},
		{"ayu_ShowOnlyAddedEmojisAndStickers", "僅顯示已新增的表情符號與貼圖"},
		{"ayu_HideReactions", "隱藏回應反應 (Reactions)"},
		{"ayu_HideReactionsInChannels", "在頻道中隱藏"},
		{"ayu_HideReactionsInGroups", "在群組中隱藏"},
		{"ayu_SettingsRecentStickersCount", "常用貼圖顯示數量"},
		{"ayu_ChannelBottomButton", "頻道底部按鈕行為"},
		{"ayu_ChannelBottomButtonHide", "隱藏"},
		{"ayu_ChannelBottomButtonMute", "靜音 / 取消靜音"},
		{"ayu_ChannelBottomButtonDiscuss", "前往討論群組"},
		{"ayu_QuickAdminShortcuts", "管理員快速操作捷徑"},
		{"ayu_SettingsShowMessageShot", "顯示「訊息截圖」按鈕"},
		{"ayu_SettingsShowMessageShotDescription", "在訊息操作選單中加入截圖功能。"},
		{"ayu_DeletedMarkText", "已刪除訊息標記文字"},
		{"ayu_EditedMarkText", "已編輯訊息標記文字"},
		{"ayu_ReplaceMarksWithIcons", "將標記替換為圖示"},
		{"ayu_HideShareButton", "隱藏快速分享按鈕"},
		{"ayu_SimpleQuotesAndReplies", "簡約引用與回覆風格"},
		{"ayu_SettingsWideMultiplier", "對話寬度比例"},
		{"ayu_SettingsWideMultiplierDescription", "調整對話氣泡的最大寬度比例。修改後需重啟程式。"},
		{"ayu_ContextMenuElementsHeader", "右鍵選單項目"},
		{"ayu_SettingsContextMenuItemHidden", "隱藏"},
		{"ayu_SettingsContextMenuItemShown", "顯示"},
		{"ayu_SettingsContextMenuItemExtended", "展開"},
		{"ayu_SettingsContextMenuReactionsPanel", "反應面板 (Reactions)"},
		{"ayu_SettingsContextMenuViewsPanel", "瀏覽數面板 (Views)"},
		{"ayu_SettingsContextMenuTitle", "右鍵選單設定"},
		{"ayu_ContextHideMessage", "隱藏此訊息"},
		{"ayu_UserMessagesMenuText", "查看此用戶訊息"},
		{"ayu_MessageDetailsPC", "訊息詳細資訊"},
		{"ayu_RegexFilterQuickAdd", "快速加入過濾器"},
		{"ayu_SettingsContextMenuDescription", "自訂右鍵點選訊息時顯示的項目。"},
		{"ayu_MessageFieldElementsHeader", "輸入欄位圖示"},
		{"ayu_MessageFieldElementAttach", "附件按鈕"},
		{"ayu_MessageFieldElementCommands", "指令按鈕 (/)"},
		{"ayu_MessageFieldElementTTL", "自動刪除按鈕"},
		{"ayu_MessageFieldElementEmoji", "表情符號按鈕"},
		{"ayu_MessageFieldElementVoice", "語音 / 影片訊息按鈕"},
		{"ayu_MessageFieldPopupsHeader", "輸入欄位彈出視窗"},

		// Filters Settings
		{"ayu_CategoryFilters", "過濾器設定"},
		{"ayu_FiltersMenuSelectChat", "選擇對話"},
		{"ayu_FiltersMenuImport", "匯入過濾器"},
		{"ayu_FiltersMenuExport", "匯出過濾器"},
		{"ayu_FiltersMenuClear", "清空過濾器"},
		{"ayu_FiltersClearPopupText", "確定要清空所有的過濾器與排除名單嗎？"},
		{"ayu_FiltersClearPopupActionText", "立即清空"},
		{"ayu_RegexFilters", "正則表達式過濾器"},
		{"ayu_RegexFiltersEnable", "啟用過濾器功能"},
		{"ayu_RegexFiltersEnableSharedInChats", "在對話列表中套用過濾"},
		{"ayu_FiltersHideFromBlocked", "隱藏來自封鎖對象的訊息"},
		{"ayu_RegexFiltersShared", "全域通用過濾器"},
		{"ayu_FiltersShadowBan", "影子封鎖清單 (Shadow Ban)"},

		// Other
		{"ayu_CategoryOther", "其他設定"},
		{"ayu_SupportHeader", "支持開發者"},
		{"ayu_SupportDescription1", "AyuGram 支援服務"},
		{"ayu_SupportDescription2", "如果您喜歡這個應用程式，可以贊助支持開發，或聯絡 {item}。"},
		{"ayu_CrashReporting", "傳送錯誤報告"},
		{"ayu_CrashReportingDescription", "當程式發生非預期錯誤時，自動傳送匿名報告以協助維護。"},
		{"ayu_RegisterURLScheme", "註冊 URL 協議 (URL Scheme)"},
		{"ayu_ResetSettings", "重置所有設定"},
		{"ayu_ResetSettingsConfirmation", "確定要將所有 AyuGram 設定還原為預設值嗎？"},

		// Specific to Sidebar
		{"ayu_CategoryAyuGram", "AyuGram 功能設定"},
		{"ayu_CategoriesHeader", "分類清單"},
		{"ayu_LinksHeader", "相關連結"},
		{"ayu_WebviewHeader", "內建網頁設定 (Webview)"},
	};

	const auto lang = Lang::Id().toLower();
	// Only apply Traditional Chinese (Taiwan/HK) translations.
	// We check for "hant" (Traditional) or "tw/hk" specifically to avoid affecting "zh-hans" (Simplified).
	if (lang.contains(qsl("hant")) || lang.contains(qsl("tw")) || lang.contains(qsl("hk"))) {
		if (dict.contains(key)) {
			return dict.value(key);
		}
	}
	return fallback;
}

} // namespace

#define AYU_T(key) (tr::key() | rpl::map([=](const QString &v) { return AyuHantHelper(qsl(#key), v); }))
#define AYU_S(key) AyuHantHelper(qsl(#key), tr::key(tr::now))
