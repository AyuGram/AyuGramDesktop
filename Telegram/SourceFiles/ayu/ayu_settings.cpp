// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ayu_settings.h"

#include "lang_auto.h"
#include "tray.h"
#include "ayu/ayu_ui_settings.h"
#include "ayu/ayu_worker.h"
#include "ayu/ui/ayu_logo.h"
#include "core/application.h"
#include "features/filters/filters_cache_controller.h"
#include "features/translator/ayu_translator.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "rpl/combine.h"
#include "ui/ayu_userpic.h"
#include "window/window_controller.h"

#include <fstream>
#include <QApplication>

using json = nlohmann::json;

namespace {

std::string getSettingsPath() {
	return (cWorkingDir() + u"tdata/ayu_settings.json"_q).toStdString();
}

void repaintApp() {
	for (QWidget *widget : QApplication::allWidgets()) {
		widget->update();
	}
}

rpl::lifetime lifetime; // idk reactivity dies when placed in `GhostModeAccountSettings` as field

} // namespace

GhostModeAccountSettings::GhostModeAccountSettings() {
	rpl::combine(
		_sendReadMessages.value(),
		_sendReadMessagesLocked.value(),
		_sendReadStories.value(),
		_sendReadStoriesLocked.value(),
		_sendOnlinePackets.value(),
		_sendOnlinePacketsLocked.value(),
		_sendUploadProgress.value(),
		_sendUploadProgressLocked.value(),
		_sendOfflinePacketAfterOnline.value(),
		_sendOfflinePacketAfterOnlineLocked.value()
	) | rpl::on_next([=](
			bool readMsg, bool readMsgLocked,
			bool readStories, bool readStoriesLocked,
			bool online, bool onlineLocked,
			bool upload, bool uploadLocked,
			bool offline, bool offlineLocked) {
		_ghostModeActive = (readMsgLocked || !readMsg)
			&& (readStoriesLocked || !readStories)
			&& (onlineLocked || !online)
			&& (uploadLocked || !upload)
			&& (offlineLocked || offline);
	}, lifetime);
}

void GhostModeAccountSettings::setSendReadMessages(bool val) {
	if (_sendReadMessages.current() == val) return;
	_sendReadMessages = val;
	AyuSettings::save();
}

void GhostModeAccountSettings::setSendReadStories(bool val) {
	if (_sendReadStories.current() == val) return;
	_sendReadStories = val;
	AyuSettings::save();
}

void GhostModeAccountSettings::setSendOnlinePackets(bool val) {
	if (_sendOnlinePackets.current() == val) return;
	_sendOnlinePackets = val;
	AyuSettings::save();
}

void GhostModeAccountSettings::setSendUploadProgress(bool val) {
	if (_sendUploadProgress.current() == val) return;
	_sendUploadProgress = val;
	AyuSettings::save();
}

void GhostModeAccountSettings::setSendOfflinePacketAfterOnline(bool val) {
	if (_sendOfflinePacketAfterOnline.current() == val) return;
	_sendOfflinePacketAfterOnline = val;
	AyuSettings::save();
}

void GhostModeAccountSettings::setMarkReadAfterAction(bool val) {
	if (_markReadAfterAction.current() == val) return;
	_markReadAfterAction = val;
	AyuSettings::save();
}

void GhostModeAccountSettings::setUseScheduledMessages(bool val) {
	if (_useScheduledMessages.current() == val) return;
	_useScheduledMessages = val;
	AyuSettings::save();
}

void GhostModeAccountSettings::setSendWithoutSound(bool val) {
	if (_sendWithoutSound.current() == val) return;
	_sendWithoutSound = val;
	AyuSettings::save();
}

void GhostModeAccountSettings::setGhostModeEnabled(bool val) {
	if (!_sendReadMessagesLocked.current()) _sendReadMessages = !val;
	if (!_sendReadStoriesLocked.current()) _sendReadStories = !val;
	if (!_sendOnlinePacketsLocked.current()) _sendOnlinePackets = !val;
	if (!_sendUploadProgressLocked.current()) _sendUploadProgress = !val;
	if (!_sendOfflinePacketAfterOnlineLocked.current()) _sendOfflinePacketAfterOnline = val;
	AyuSettings::save();

	if (val) {
		if (const auto window = Core::App().activeWindow()) {
			if (const auto session = window->maybeSession()) {
				AyuWorker::markAsOnline(session);
			}
		}
	}
}

void GhostModeAccountSettings::setSendReadMessagesLocked(bool val) {
	if (_sendReadMessagesLocked.current() == val) return;
	_sendReadMessagesLocked = val;
	AyuSettings::save();
}

void GhostModeAccountSettings::setSendReadStoriesLocked(bool val) {
	if (_sendReadStoriesLocked.current() == val) return;
	_sendReadStoriesLocked = val;
	AyuSettings::save();
}

void GhostModeAccountSettings::setSendOnlinePacketsLocked(bool val) {
	if (_sendOnlinePacketsLocked.current() == val) return;
	_sendOnlinePacketsLocked = val;
	AyuSettings::save();
}

void GhostModeAccountSettings::setSendUploadProgressLocked(bool val) {
	if (_sendUploadProgressLocked.current() == val) return;
	_sendUploadProgressLocked = val;
	AyuSettings::save();
}

void GhostModeAccountSettings::setSendOfflinePacketAfterOnlineLocked(bool val) {
	if (_sendOfflinePacketAfterOnlineLocked.current() == val) return;
	_sendOfflinePacketAfterOnlineLocked = val;
	AyuSettings::save();
}

void to_json(nlohmann::json &j, const GhostModeAccountSettings &s) {
	j = nlohmann::json{
		{"sendReadMessages", s._sendReadMessages.current()},
		{"sendReadStories", s._sendReadStories.current()},
		{"sendOnlinePackets", s._sendOnlinePackets.current()},
		{"sendUploadProgress", s._sendUploadProgress.current()},
		{"sendOfflinePacketAfterOnline", s._sendOfflinePacketAfterOnline.current()},
		{"markReadAfterAction", s._markReadAfterAction.current()},
		{"useScheduledMessages", s._useScheduledMessages.current()},
		{"sendWithoutSound", s._sendWithoutSound.current()},
		{"sendReadMessagesLocked", s._sendReadMessagesLocked.current()},
		{"sendReadStoriesLocked", s._sendReadStoriesLocked.current()},
		{"sendOnlinePacketsLocked", s._sendOnlinePacketsLocked.current()},
		{"sendUploadProgressLocked", s._sendUploadProgressLocked.current()},
		{"sendOfflinePacketAfterOnlineLocked", s._sendOfflinePacketAfterOnlineLocked.current()}
	};
}

void from_json(const nlohmann::json &j, GhostModeAccountSettings &s) {
	s._sendReadMessages = j.value("sendReadMessages", true);
	s._sendReadStories = j.value("sendReadStories", true);
	s._sendOnlinePackets = j.value("sendOnlinePackets", true);
	s._sendUploadProgress = j.value("sendUploadProgress", true);
	s._sendOfflinePacketAfterOnline = j.value("sendOfflinePacketAfterOnline", false);
	s._markReadAfterAction = j.value("markReadAfterAction", true);
	s._useScheduledMessages = j.value("useScheduledMessages", false);
	s._sendWithoutSound = j.value("sendWithoutSound", false);
	s._sendReadMessagesLocked = j.value("sendReadMessagesLocked", false);
	s._sendReadStoriesLocked = j.value("sendReadStoriesLocked", false);
	s._sendOnlinePacketsLocked = j.value("sendOnlinePacketsLocked", false);
	s._sendUploadProgressLocked = j.value("sendUploadProgressLocked", false);
	s._sendOfflinePacketAfterOnlineLocked = j.value("sendOfflinePacketAfterOnlineLocked", false);
}

void MessageShotSettings::setShowBackground(bool val) {
	if (_showBackground.current() == val) return;
	_showBackground = val;
	AyuSettings::save();
}

void MessageShotSettings::setShowDate(bool val) {
	if (_showDate.current() == val) return;
	_showDate = val;
	AyuSettings::save();
}

void MessageShotSettings::setShowReactions(bool val) {
	if (_showReactions.current() == val) return;
	_showReactions = val;
	AyuSettings::save();
}

void MessageShotSettings::setShowColorfulReplies(bool val) {
	if (_showColorfulReplies.current() == val) return;
	_showColorfulReplies = val;
	AyuSettings::save();
}

bool MessageShotSettings::isCloudThemeEmpty() const {
	return !_cloudThemeId.current()
		&& !_cloudThemeAccessHash.current()
		&& !_cloudThemeDocumentId.current()
		&& _cloudThemeTitle.current().isEmpty();
}

void MessageShotSettings::clearCloudThemeData() {
	_cloudThemeId = uint64(0);
	_cloudThemeAccessHash = uint64(0);
	_cloudThemeDocumentId = uint64(0);
	_cloudThemeTitle = QString();
	_cloudThemeAccountId = uint64(0);
}

void MessageShotSettings::setEmbeddedTheme(int type, uint32 accentColor) {
	if (_embeddedThemeType.current() == type
		&& _embeddedThemeAccentColor.current() == accentColor
		&& isCloudThemeEmpty()) {
		return;
	}
	_embeddedThemeType = type;
	_embeddedThemeAccentColor = accentColor;
	clearCloudThemeData();
	AyuSettings::save();
}

void MessageShotSettings::setCloudTheme(uint64 accountId, uint64 id, uint64 accessHash, uint64 documentId, const QString &title) {
	if (_embeddedThemeType.current() == -1
		&& _embeddedThemeAccentColor.current() == 0
		&& _cloudThemeAccountId.current() == accountId
		&& _cloudThemeId.current() == id
		&& _cloudThemeAccessHash.current() == accessHash
		&& _cloudThemeDocumentId.current() == documentId
		&& _cloudThemeTitle.current() == title) {
		return;
	}
	_embeddedThemeType = -1;
	_embeddedThemeAccentColor = uint32(0);
	_cloudThemeAccountId = accountId;
	_cloudThemeId = id;
	_cloudThemeAccessHash = accessHash;
	_cloudThemeDocumentId = documentId;
	_cloudThemeTitle = title;
	AyuSettings::save();
}

void MessageShotSettings::clearTheme() {
	if (_embeddedThemeType.current() == -1
		&& _embeddedThemeAccentColor.current() == 0
		&& isCloudThemeEmpty()) {
		return;
	}
	_embeddedThemeType = -1;
	_embeddedThemeAccentColor = uint32(0);
	clearCloudThemeData();
	AyuSettings::save();
}

void to_json(nlohmann::json &j, const MessageShotSettings &s) {
	j = nlohmann::json{
		{"showBackground", s._showBackground.current()},
		{"showDate", s._showDate.current()},
		{"showReactions", s._showReactions.current()},
		{"showColorfulReplies", s._showColorfulReplies.current()},
		{"embeddedThemeType", s._embeddedThemeType.current()},
		{"embeddedThemeAccentColor", s._embeddedThemeAccentColor.current()},
		{"cloudThemeId", s._cloudThemeId.current()},
		{"cloudThemeAccessHash", s._cloudThemeAccessHash.current()},
		{"cloudThemeDocumentId", s._cloudThemeDocumentId.current()},
		{"cloudThemeTitle", s._cloudThemeTitle.current()},
		{"cloudThemeAccountId", s._cloudThemeAccountId.current()},
	};
}

void from_json(const nlohmann::json &j, MessageShotSettings &s) {
	s._showBackground = j.value("showBackground", true);
	s._showDate = j.value("showDate", false);
	s._showReactions = j.value("showReactions", false);
	s._showColorfulReplies = j.value("showColorfulReplies", false);
	s._embeddedThemeType = j.value("embeddedThemeType", j.value("themeType", -1));
	s._embeddedThemeAccentColor = j.value("embeddedThemeAccentColor", j.value("themeAccentColor", uint32(0)));
	s._cloudThemeId = j.value("cloudThemeId", uint64(0));
	s._cloudThemeAccessHash = j.value("cloudThemeAccessHash", uint64(0));
	s._cloudThemeDocumentId = j.value("cloudThemeDocumentId", uint64(0));
	s._cloudThemeTitle = j.value("cloudThemeTitle", QString());
	s._cloudThemeAccountId = j.value("cloudThemeAccountId", uint64(0));
}

AyuSettings::AyuSettings()
: _appIcon(AyuAssets::DEFAULT_ICON)
, _editedMark(Core::IsAppLaunched() ? tr::lng_edited(tr::now) : QString("edited")) {
}

AyuSettings &AyuSettings::getInstance() {
	static AyuSettings instance;
	return instance;
}

void AyuSettings::load() {
	std::ifstream file(getSettingsPath());
	if (!file.good()) {
		return;
	}

	auto &settings = getInstance();

	try {
		json p;
		file >> p;
		file.close();

		if (!p.contains("ghostModeSettings")) {
			p["ghostModeSettings"] = nlohmann::json::object({
				{"0", {
					{"sendReadMessages", p.value("sendReadMessages", true)},
					{"sendReadStories", p.value("sendReadStories", true)},
					{"sendOnlinePackets", p.value("sendOnlinePackets", true)},
					{"sendUploadProgress", p.value("sendUploadProgress", true)},
					{"sendOfflinePacketAfterOnline", p.value("sendOfflinePacketAfterOnline", false)},
					{"markReadAfterAction", p.value("markReadAfterAction", true)},
					{"useScheduledMessages", p.value("useScheduledMessages", false)},
					{"sendWithoutSound", p.value("sendWithoutSound", false)}
				}}
			});
			p["useGlobalGhostMode"] = true;

			LOG(("AyuGramSettings: migrated ghost mode settings to per-account format"));
		}

		try {
			from_json(p, settings);
		} catch (...) {
			LOG(("AyuGramSettings: failed to parse settings file"));
		}
	} catch (...) {
		LOG(("AyuGramSettings: failed to read settings file (not json-like)"));
	}

	if (cGhost()) {
		auto &ghost = AyuSettings::ghost();
		ghost._sendReadMessages = false;
		ghost._sendReadStories = false;
		ghost._sendOnlinePackets = false;
		ghost._sendUploadProgress = false;
		ghost._sendOfflinePacketAfterOnline = true;
	}

	settings.validate();
}

void AyuSettings::save() {
	auto &settings = getInstance();
	json p = settings;

	std::ofstream file;
	file.open(getSettingsPath());
	file << p.dump(4);
	file.close();
}

void AyuSettings::reset() {
	getInstance() = AyuSettings();
	save();
}

GhostModeAccountSettings &AyuSettings::ghost(not_null<Main::Session*> session) {
	return ghost(session->userId().bare);
}

GhostModeAccountSettings &AyuSettings::ghost(uint64 userId) {
	auto &settings = getInstance();
	auto overriddenId = settings.getOverriddenGhostUserId(userId);

	auto it = settings._ghostAccounts.find(overriddenId);
	if (it == settings._ghostAccounts.end()) {
		auto account = std::make_unique<GhostModeAccountSettings>();
		it = settings._ghostAccounts.emplace(overriddenId, std::move(account)).first;
	}

	return *it->second;
}

GhostModeAccountSettings &AyuSettings::ghost() {
	if (const auto window = Core::App().activeWindow()) {
		if (const auto session = window->maybeSession()) {
			return ghost(session);
		}
	}
	return ghost(0);
}

void AyuSettings::setUseGlobalGhostMode(bool val) {
	if (_useGlobalGhostMode.current() == val) return;
	_useGlobalGhostMode = val;
	save();
}

void AyuSettings::addShadowBan(int64 id) {
	if (_shadowBanIds.insert(id).second) {
		FiltersCacheController::rebuildCache();
		FiltersCacheController::fireUpdate();
		save();
	}
}

void AyuSettings::removeShadowBan(int64 id) {
	if (_shadowBanIds.erase(id) > 0) {
		FiltersCacheController::rebuildCache();
		FiltersCacheController::fireUpdate();
		save();
	}
}

void AyuSettings::validate() {
	AyuSettings defaults;
	auto modified = false;

	auto validateRange = [&](auto &var, auto min, auto max, const auto &defaultVar) {
		if (var.current() < min || var.current() > max) {
			var = defaultVar.current();
			modified = true;
		}
	};

	auto validateEnum = [&](auto &var, const auto &defaultVar) {
		auto intVal = static_cast<int>(var.current());
		if (intVal < 0 || intVal > 2) {
			var = defaultVar.current();
			modified = true;
		}
	};

	validateEnum(_showPeerId, defaults._showPeerId);
	validateEnum(_channelBottomButton, defaults._channelBottomButton);
	validateEnum(_showReactionsPanelInContextMenu, defaults._showReactionsPanelInContextMenu);
	validateEnum(_showViewsPanelInContextMenu, defaults._showViewsPanelInContextMenu);
	validateEnum(_showHideMessageInContextMenu, defaults._showHideMessageInContextMenu);
	validateEnum(_showUserMessagesInContextMenu, defaults._showUserMessagesInContextMenu);
	validateEnum(_showMessageDetailsInContextMenu, defaults._showMessageDetailsInContextMenu);
	validateEnum(_showRepeatMessageInContextMenu, defaults._showRepeatMessageInContextMenu);
	validateEnum(_showAddFilterInContextMenu, defaults._showAddFilterInContextMenu);

	validateEnum(_translationProvider, defaults._translationProvider);

	validateRange(_wideMultiplier, 0.5, 4.0, defaults._wideMultiplier);
	validateRange(_recentStickersCount, 1, 200, defaults._recentStickersCount);
	validateRange(_avatarCorners, 0, AyuUiSettings::kMaxAvatarCorners, defaults._avatarCorners);

	const auto embeddedType = _messageShotSettings._embeddedThemeType.current();
	auto embeddedTypeValid = (embeddedType == -1) || (embeddedType >= 0 && embeddedType <= 3); // from Window::Theme::EmbeddedType::DayBlue to Window::Theme::EmbeddedType::NightGreen
	if (!embeddedTypeValid) {
		_messageShotSettings._embeddedThemeType = defaults._messageShotSettings._embeddedThemeType.current();
		_messageShotSettings._embeddedThemeAccentColor = defaults._messageShotSettings._embeddedThemeAccentColor.current();
		modified = true;
	}

	if (modified) {
		save();
	}
}

void AyuSettings::setSaveDeletedMessages(bool val) {
	if (_saveDeletedMessages.current() == val) return;
	_saveDeletedMessages = val;
	save();
}

void AyuSettings::setSaveMessagesHistory(bool val) {
	if (_saveMessagesHistory.current() == val) return;
	_saveMessagesHistory = val;
	save();
}

void AyuSettings::setSaveForBots(bool val) {
	if (_saveForBots.current() == val) return;
	_saveForBots = val;
	save();
}

void AyuSettings::setFiltersEnabled(bool val) {
	if (_filtersEnabled.current() == val) return;
	_filtersEnabled = val;
	save();
}

void AyuSettings::setFiltersEnabledInChats(bool val) {
	if (_filtersEnabledInChats.current() == val) return;
	_filtersEnabledInChats = val;
	save();
}

void AyuSettings::setHideFromBlocked(bool val) {
	if (_hideFromBlocked.current() == val) return;
	_hideFromBlocked = val;
	save();
}

void AyuSettings::setSemiTransparentDeletedMessages(bool val) {
	if (_semiTransparentDeletedMessages.current() == val) return;
	_semiTransparentDeletedMessages = val;
	save();
}

void AyuSettings::setDisableAds(bool val) {
	if (_disableAds.current() == val) return;
	_disableAds = val;
	save();
}

void AyuSettings::setDisableStories(bool val) {
	if (_disableStories.current() == val) return;
	_disableStories = val;
	save();
}

void AyuSettings::setDisableCustomBackgrounds(bool val) {
	if (_disableCustomBackgrounds.current() == val) return;
	_disableCustomBackgrounds = val;
	save();
}

void AyuSettings::setHidePremiumStatuses(bool val) {
	if (_hidePremiumStatuses.current() == val) return;
	_hidePremiumStatuses = val;
	save();
}

void AyuSettings::setShowOnlyAddedEmojisAndStickers(bool val) {
	if (_showOnlyAddedEmojisAndStickers.current() == val) return;
	_showOnlyAddedEmojisAndStickers = val;
	save();
}

void AyuSettings::setCollapseSimilarChannels(bool val) {
	if (_collapseSimilarChannels.current() == val) return;
	_collapseSimilarChannels = val;
	save();
}

void AyuSettings::setHideSimilarChannels(bool val) {
	if (_hideSimilarChannels.current() == val) return;
	_hideSimilarChannels = val;
	save();
}

void AyuSettings::setWideMultiplier(double val) {
	if (_wideMultiplier.current() == val) return;
	_wideMultiplier = val;
	// doesn't work because it should be set before style::StartManager()
	// AyuUiSettings::setWideMultiplier(val);
	// repaintApp();
	save();
}

void AyuSettings::setSpoofWebviewAsAndroid(bool val) {
	if (_spoofWebviewAsAndroid.current() == val) return;
	_spoofWebviewAsAndroid = val;
	save();
}

void AyuSettings::setIncreaseWebviewHeight(bool val) {
	if (_increaseWebviewHeight.current() == val) return;
	_increaseWebviewHeight = val;
	save();
}

void AyuSettings::setIncreaseWebviewWidth(bool val) {
	if (_increaseWebviewWidth.current() == val) return;
	_increaseWebviewWidth = val;
	save();
}

void AyuSettings::setMaterialSwitches(bool val) {
	if (_materialSwitches.current() == val) return;
	_materialSwitches = val;
	AyuUiSettings::setMaterialSwitches(val);
	repaintApp();
	save();
}

void AyuSettings::setRemoveMessageTail(bool val) {
	if (_removeMessageTail.current() == val) return;
	_removeMessageTail = val;
	save();
}

void AyuSettings::setDisableNotificationsDelay(bool val) {
	if (_disableNotificationsDelay.current() == val) return;
	_disableNotificationsDelay = val;
	save();
}

void AyuSettings::setLocalPremium(bool val) {
	if (_localPremium.current() == val) return;
	_localPremium = val;
	save();
}

void AyuSettings::setShowChannelReactions(bool val) {
	if (_showChannelReactions.current() == val) return;
	_showChannelReactions = val;
	save();
}

void AyuSettings::setShowGroupReactions(bool val) {
	if (_showGroupReactions.current() == val) return;
	_showGroupReactions = val;
	save();
}

void AyuSettings::setShowPrivateChatReactions(bool val) {
	if (_showPrivateChatReactions.current() == val) return;
	_showPrivateChatReactions = val;
	save();
}

void AyuSettings::setAppIcon(const QString &val) {
	if (_appIcon.current() == val) return;
	_appIcon = val;
	save();
}

void AyuSettings::setSimpleQuotesAndReplies(bool val) {
	if (_simpleQuotesAndReplies.current() == val) return;
	_simpleQuotesAndReplies = val;
	save();
}

void AyuSettings::setHideFastShare(bool val) {
	if (_hideFastShare.current() == val) return;
	_hideFastShare = val;
	save();
}

void AyuSettings::setReplaceBottomInfoWithIcons(bool val) {
	if (_replaceBottomInfoWithIcons.current() == val) return;
	_replaceBottomInfoWithIcons = val;
	save();
}

void AyuSettings::setDeletedMark(const QString &val) {
	if (_deletedMark.current() == val) return;
	_deletedMark = val;
	save();
}

void AyuSettings::setEditedMark(const QString &val) {
	if (_editedMark.current() == val) return;
	_editedMark = val;
	save();
}

void AyuSettings::setRecentStickersCount(int val) {
	if (_recentStickersCount.current() == val) return;
	_recentStickersCount = val;
	save();
}

void AyuSettings::setShowReactionsPanelInContextMenu(ContextMenuVisibility val) {
	if (_showReactionsPanelInContextMenu.current() == val) return;
	_showReactionsPanelInContextMenu = val;
	save();
}

void AyuSettings::setShowViewsPanelInContextMenu(ContextMenuVisibility val) {
	if (_showViewsPanelInContextMenu.current() == val) return;
	_showViewsPanelInContextMenu = val;
	save();
}

void AyuSettings::setShowHideMessageInContextMenu(ContextMenuVisibility val) {
	if (_showHideMessageInContextMenu.current() == val) return;
	_showHideMessageInContextMenu = val;
	save();
}

void AyuSettings::setShowUserMessagesInContextMenu(ContextMenuVisibility val) {
	if (_showUserMessagesInContextMenu.current() == val) return;
	_showUserMessagesInContextMenu = val;
	save();
}

void AyuSettings::setShowMessageDetailsInContextMenu(ContextMenuVisibility val) {
	if (_showMessageDetailsInContextMenu.current() == val) return;
	_showMessageDetailsInContextMenu = val;
	save();
}

void AyuSettings::setShowRepeatMessageInContextMenu(ContextMenuVisibility val) {
	if (_showRepeatMessageInContextMenu.current() == val) return;
	_showRepeatMessageInContextMenu = val;
	save();
}

void AyuSettings::setShowAddFilterInContextMenu(ContextMenuVisibility val) {
	if (_showAddFilterInContextMenu.current() == val) return;
	_showAddFilterInContextMenu = val;
	save();
}

void AyuSettings::setShowAttachButtonInMessageField(bool val) {
	if (_showAttachButtonInMessageField.current() == val) return;
	_showAttachButtonInMessageField = val;
	save();
}

void AyuSettings::setShowCommandsButtonInMessageField(bool val) {
	if (_showCommandsButtonInMessageField.current() == val) return;
	_showCommandsButtonInMessageField = val;
	save();
}

void AyuSettings::setShowEmojiButtonInMessageField(bool val) {
	if (_showEmojiButtonInMessageField.current() == val) return;
	_showEmojiButtonInMessageField = val;
	save();
}

void AyuSettings::setShowMicrophoneButtonInMessageField(bool val) {
	if (_showMicrophoneButtonInMessageField.current() == val) return;
	_showMicrophoneButtonInMessageField = val;
	save();
}

void AyuSettings::setShowAutoDeleteButtonInMessageField(bool val) {
	if (_showAutoDeleteButtonInMessageField.current() == val) return;
	_showAutoDeleteButtonInMessageField = val;
	save();
}

void AyuSettings::setShowAttachPopup(bool val) {
	if (_showAttachPopup.current() == val) return;
	_showAttachPopup = val;
	save();
}

void AyuSettings::setShowEmojiPopup(bool val) {
	if (_showEmojiPopup.current() == val) return;
	_showEmojiPopup = val;
	save();
}

void AyuSettings::setShowMyProfileInDrawer(bool val) {
	if (_showMyProfileInDrawer.current() == val) return;
	_showMyProfileInDrawer = val;
	save();
}

void AyuSettings::setShowBotsInDrawer(bool val) {
	if (_showBotsInDrawer.current() == val) return;
	_showBotsInDrawer = val;
	save();
}

void AyuSettings::setShowNewGroupInDrawer(bool val) {
	if (_showNewGroupInDrawer.current() == val) return;
	_showNewGroupInDrawer = val;
	save();
}

void AyuSettings::setShowNewChannelInDrawer(bool val) {
	if (_showNewChannelInDrawer.current() == val) return;
	_showNewChannelInDrawer = val;
	save();
}

void AyuSettings::setShowContactsInDrawer(bool val) {
	if (_showContactsInDrawer.current() == val) return;
	_showContactsInDrawer = val;
	save();
}

void AyuSettings::setShowCallsInDrawer(bool val) {
	if (_showCallsInDrawer.current() == val) return;
	_showCallsInDrawer = val;
	save();
}

void AyuSettings::setShowSavedMessagesInDrawer(bool val) {
	if (_showSavedMessagesInDrawer.current() == val) return;
	_showSavedMessagesInDrawer = val;
	save();
}

void AyuSettings::setShowLReadToggleInDrawer(bool val) {
	if (_showLReadToggleInDrawer.current() == val) return;
	_showLReadToggleInDrawer = val;
	save();
}

void AyuSettings::setShowSReadToggleInDrawer(bool val) {
	if (_showSReadToggleInDrawer.current() == val) return;
	_showSReadToggleInDrawer = val;
	save();
}

void AyuSettings::setShowNightModeToggleInDrawer(bool val) {
	if (_showNightModeToggleInDrawer.current() == val) return;
	_showNightModeToggleInDrawer = val;
	save();
}

void AyuSettings::setShowGhostToggleInDrawer(bool val) {
	if (_showGhostToggleInDrawer.current() == val) return;
	_showGhostToggleInDrawer = val;
	save();
}

void AyuSettings::setShowStreamerToggleInDrawer(bool val) {
	if (_showStreamerToggleInDrawer.current() == val) return;
	_showStreamerToggleInDrawer = val;
	save();
}

void AyuSettings::setShowGhostToggleInTray(bool val) {
	if (_showGhostToggleInTray.current() == val) return;
	_showGhostToggleInTray = val;
	save();
}

void AyuSettings::setShowStreamerToggleInTray(bool val) {
	if (_showStreamerToggleInTray.current() == val) return;
	_showStreamerToggleInTray = val;
	save();
}

void AyuSettings::setMonoFont(const QString &val) {
	if (_monoFont.current() == val) return;
	_monoFont = val;
	// doesn't work because `static const auto family = ...`
	// AyuUiSettings::setMonoFont(val);
	// repaintApp();
	save();
}

void AyuSettings::setHideNotificationCounters(bool val) {
	if (_hideNotificationCounters.current() == val) return;
	_hideNotificationCounters = val;
	save();
}

void AyuSettings::setHideNotificationBadge(bool val) {
	if (_hideNotificationBadge.current() == val) return;
	_hideNotificationBadge = val;
	Core::App().refreshApplicationIcon();
	Core::App().tray().updateIconCounters();
	Core::App().domain().notifyUnreadBadgeChanged();
	save();
}

void AyuSettings::setHideAllChatsFolder(bool val) {
	if (_hideAllChatsFolder.current() == val) return;
	_hideAllChatsFolder = val;
	save();
}

void AyuSettings::setChannelBottomButton(ChannelBottomButton val) {
	if (_channelBottomButton.current() == val) return;
	_channelBottomButton = val;
	save();
}

void AyuSettings::setQuickAdminShortcuts(bool val) {
	if (_quickAdminShortcuts.current() == val) return;
	_quickAdminShortcuts = val;
	save();
}

void AyuSettings::setShowPeerId(PeerIdDisplay val) {
	if (_showPeerId.current() == val) return;
	_showPeerId = val;
	save();
}

void AyuSettings::setShowMessageSeconds(bool val) {
	if (_showMessageSeconds.current() == val) return;
	_showMessageSeconds = val;
	save();
}

void AyuSettings::setShowMessageShot(bool val) {
	if (_showMessageShot.current() == val) return;
	_showMessageShot = val;
	save();
}

void AyuSettings::setFilterZalgo(bool val) {
	if (_filterZalgo.current() == val) return;
	_filterZalgo = val;
	save();
}

void AyuSettings::setStickerConfirmation(bool val) {
	if (_stickerConfirmation.current() == val) return;
	_stickerConfirmation = val;
	save();
}

void AyuSettings::setGifConfirmation(bool val) {
	if (_gifConfirmation.current() == val) return;
	_gifConfirmation = val;
	save();
}

void AyuSettings::setVoiceConfirmation(bool val) {
	if (_voiceConfirmation.current() == val) return;
	_voiceConfirmation = val;
	save();
}

void AyuSettings::setTranslationProvider(TranslationProvider val) {
	if (_translationProvider.current() == val) return;
	_translationProvider = val;
	Ayu::Translator::TranslateManager::currentInstance()->resetCache();
	save();
}

void AyuSettings::setAdaptiveCoverColor(bool val) {
	if (_adaptiveCoverColor.current() == val) return;
	_adaptiveCoverColor = val;
	save();
}

void AyuSettings::setImproveLinkPreviews(bool val) {
	if (_improveLinkPreviews.current() == val) return;
	_improveLinkPreviews = val;
	save();
}

void AyuSettings::setCrashReporting(bool val) {
	if (_crashReporting.current() == val) return;
	_crashReporting = val;
	save();
}

void AyuSettings::setAvatarCorners(int val) {
	if (_avatarCorners.current() == val) return;
	_avatarCorners = val;
	AyuUiSettings::setAvatarCorners(val);
	save();
}

void AyuSettings::setSingleCornerRadius(bool val) {
	if (_singleCornerRadius.current() == val) return;
	_singleCornerRadius = val;
	repaintApp();
	save();
}

void to_json(nlohmann::json &j, const AyuSettings &s) {
	std::map<std::string, GhostModeAccountSettings> ghostAccounts;
	for (const auto &[key, value] : s._ghostAccounts) {
		ghostAccounts[std::to_string(key)] = std::move(*value);
	}

	j = nlohmann::json{
		{"ghostModeSettings", ghostAccounts},
		{"useGlobalGhostMode", s._useGlobalGhostMode.current()},
		{"saveDeletedMessages", s._saveDeletedMessages.current()},
		{"saveMessagesHistory", s._saveMessagesHistory.current()},
		{"saveForBots", s._saveForBots.current()},
		{"shadowBanIds", s._shadowBanIds},
		{"filtersEnabled", s._filtersEnabled.current()},
		{"filtersEnabledInChats", s._filtersEnabledInChats.current()},
		{"hideFromBlocked", s._hideFromBlocked.current()},
		{"semiTransparentDeletedMessages", s._semiTransparentDeletedMessages.current()},
		{"disableAds", s._disableAds.current()},
		{"disableStories", s._disableStories.current()},
		{"disableCustomBackgrounds", s._disableCustomBackgrounds.current()},
		{"showOnlyAddedEmojisAndStickers", s._showOnlyAddedEmojisAndStickers.current()},
		{"collapseSimilarChannels", s._collapseSimilarChannels.current()},
		{"hideSimilarChannels", s._hideSimilarChannels.current()},
		{"wideMultiplier", s._wideMultiplier.current()},
		{"spoofWebviewAsAndroid", s._spoofWebviewAsAndroid.current()},
		{"increaseWebviewHeight", s._increaseWebviewHeight.current()},
		{"increaseWebviewWidth", s._increaseWebviewWidth.current()},
		{"materialSwitches", s._materialSwitches.current()},
		{"removeMessageTail", s._removeMessageTail.current()},
		{"disableNotificationsDelay", s._disableNotificationsDelay.current()},
		{"localPremium", s._localPremium.current()},
		{"showChannelReactions", s._showChannelReactions.current()},
		{"showGroupReactions", s._showGroupReactions.current()},
		{"showPrivateChatReactions", s._showPrivateChatReactions.current()},
		{"appIcon", s._appIcon.current()},
		{"simpleQuotesAndReplies", s._simpleQuotesAndReplies.current()},
		{"hideFastShare", s._hideFastShare.current()},
		{"replaceBottomInfoWithIcons", s._replaceBottomInfoWithIcons.current()},
		{"deletedMark", s._deletedMark.current()},
		{"editedMark", s._editedMark.current()},
		{"recentStickersCount", s._recentStickersCount.current()},
		{"showReactionsPanelInContextMenu", s._showReactionsPanelInContextMenu.current()},
		{"showViewsPanelInContextMenu", s._showViewsPanelInContextMenu.current()},
		{"showHideMessageInContextMenu", s._showHideMessageInContextMenu.current()},
		{"showUserMessagesInContextMenu", s._showUserMessagesInContextMenu.current()},
		{"showMessageDetailsInContextMenu", s._showMessageDetailsInContextMenu.current()},
		{"showRepeatMessageInContextMenu", s._showRepeatMessageInContextMenu.current()},
		{"showAddFilterInContextMenu", s._showAddFilterInContextMenu.current()},
		{"showAttachButtonInMessageField", s._showAttachButtonInMessageField.current()},
		{"showCommandsButtonInMessageField", s._showCommandsButtonInMessageField.current()},
		{"showEmojiButtonInMessageField", s._showEmojiButtonInMessageField.current()},
		{"showMicrophoneButtonInMessageField", s._showMicrophoneButtonInMessageField.current()},
		{"showAutoDeleteButtonInMessageField", s._showAutoDeleteButtonInMessageField.current()},
		{"showAttachPopup", s._showAttachPopup.current()},
		{"showEmojiPopup", s._showEmojiPopup.current()},
		{"showMyProfileInDrawer", s._showMyProfileInDrawer.current()},
		{"showBotsInDrawer", s._showBotsInDrawer.current()},
		{"showNewGroupInDrawer", s._showNewGroupInDrawer.current()},
		{"showNewChannelInDrawer", s._showNewChannelInDrawer.current()},
		{"showContactsInDrawer", s._showContactsInDrawer.current()},
		{"showCallsInDrawer", s._showCallsInDrawer.current()},
		{"showSavedMessagesInDrawer", s._showSavedMessagesInDrawer.current()},
		{"showLReadToggleInDrawer", s._showLReadToggleInDrawer.current()},
		{"showSReadToggleInDrawer", s._showSReadToggleInDrawer.current()},
		{"showNightModeToggleInDrawer", s._showNightModeToggleInDrawer.current()},
		{"showGhostToggleInDrawer", s._showGhostToggleInDrawer.current()},
		{"showStreamerToggleInDrawer", s._showStreamerToggleInDrawer.current()},
		{"showGhostToggleInTray", s._showGhostToggleInTray.current()},
		{"showStreamerToggleInTray", s._showStreamerToggleInTray.current()},
		{"monoFont", s._monoFont.current()},
		{"hideNotificationCounters", s._hideNotificationCounters.current()},
		{"hideNotificationBadge", s._hideNotificationBadge.current()},
		{"hideAllChatsFolder", s._hideAllChatsFolder.current()},
		{"channelBottomButton", s._channelBottomButton.current()},
		{"quickAdminShortcuts", s._quickAdminShortcuts.current()},
		{"showPeerId", s._showPeerId.current()},
		{"showMessageSeconds", s._showMessageSeconds.current()},
		{"showMessageShot", s._showMessageShot.current()},
		{"filterZalgo", s._filterZalgo.current()},
		{"stickerConfirmation", s._stickerConfirmation.current()},
		{"gifConfirmation", s._gifConfirmation.current()},
		{"voiceConfirmation", s._voiceConfirmation.current()},
		{"translationProvider", s._translationProvider.current()},
		{"adaptiveCoverColor", s._adaptiveCoverColor.current()},
		{"improveLinkPreviews", s._improveLinkPreviews.current()},
		{"crashReporting", s._crashReporting.current()},
		{"avatarCorners", s._avatarCorners.current()},
		{"singleCornerRadius", s._singleCornerRadius.current()},
		{"messageShotSettings", s._messageShotSettings}
	};
}

void from_json(const nlohmann::json &j, AyuSettings &s) {
	AyuSettings defaults;

	if (j.contains("ghostModeSettings") && j["ghostModeSettings"].is_object()) {
		s._ghostAccounts.clear();
		for (auto &[key, value] : j["ghostModeSettings"].items()) {
			auto account = std::make_unique<GhostModeAccountSettings>();
			value.get_to(*account);
			s._ghostAccounts[std::stoull(key)] = std::move(account);
		}
	}

	s._useGlobalGhostMode = j.value("useGlobalGhostMode", defaults._useGlobalGhostMode.current());
	s._saveDeletedMessages = j.value("saveDeletedMessages", defaults._saveDeletedMessages.current());
	s._saveMessagesHistory = j.value("saveMessagesHistory", defaults._saveMessagesHistory.current());
	s._saveForBots = j.value("saveForBots", defaults._saveForBots.current());
	s._shadowBanIds = j.value("shadowBanIds", defaults._shadowBanIds);
	s._filtersEnabled = j.value("filtersEnabled", defaults._filtersEnabled.current());
	s._filtersEnabledInChats = j.value("filtersEnabledInChats", defaults._filtersEnabledInChats.current());
	s._hideFromBlocked = j.value("hideFromBlocked", defaults._hideFromBlocked.current());
	s._semiTransparentDeletedMessages = j.value("semiTransparentDeletedMessages", defaults._semiTransparentDeletedMessages.current());
	s._disableAds = j.value("disableAds", defaults._disableAds.current());
	s._disableStories = j.value("disableStories", defaults._disableStories.current());
	s._disableCustomBackgrounds = j.value("disableCustomBackgrounds", defaults._disableCustomBackgrounds.current());
	s._showOnlyAddedEmojisAndStickers = j.value("showOnlyAddedEmojisAndStickers", defaults._showOnlyAddedEmojisAndStickers.current());
	s._collapseSimilarChannels = j.value("collapseSimilarChannels", defaults._collapseSimilarChannels.current());
	s._hideSimilarChannels = j.value("hideSimilarChannels", defaults._hideSimilarChannels.current());
	s._wideMultiplier = j.value("wideMultiplier", defaults._wideMultiplier.current());
	s._spoofWebviewAsAndroid = j.value("spoofWebviewAsAndroid", defaults._spoofWebviewAsAndroid.current());
	s._increaseWebviewHeight = j.value("increaseWebviewHeight", defaults._increaseWebviewHeight.current());
	s._increaseWebviewWidth = j.value("increaseWebviewWidth", defaults._increaseWebviewWidth.current());
	s._materialSwitches = j.value("materialSwitches", defaults._materialSwitches.current());
	s._removeMessageTail = j.value("removeMessageTail", defaults._removeMessageTail.current());
	s._disableNotificationsDelay = j.value("disableNotificationsDelay", defaults._disableNotificationsDelay.current());
	s._localPremium = j.value("localPremium", defaults._localPremium.current());
	s._showChannelReactions = j.value("showChannelReactions", defaults._showChannelReactions.current());
	s._showGroupReactions = j.value("showGroupReactions", defaults._showGroupReactions.current());
	s._showPrivateChatReactions = j.value("showPrivateChatReactions", defaults._showPrivateChatReactions.current());
	s._appIcon = j.value("appIcon", defaults._appIcon.current());
	s._simpleQuotesAndReplies = j.value("simpleQuotesAndReplies", defaults._simpleQuotesAndReplies.current());
	s._hideFastShare = j.value("hideFastShare", defaults._hideFastShare.current());
	s._replaceBottomInfoWithIcons = j.value("replaceBottomInfoWithIcons", defaults._replaceBottomInfoWithIcons.current());
	s._deletedMark = j.value("deletedMark", defaults._deletedMark.current());
	s._editedMark = j.value("editedMark", defaults._editedMark.current());
	s._recentStickersCount = j.value("recentStickersCount", defaults._recentStickersCount.current());
	s._showReactionsPanelInContextMenu = j.value("showReactionsPanelInContextMenu", defaults._showReactionsPanelInContextMenu.current());
	s._showViewsPanelInContextMenu = j.value("showViewsPanelInContextMenu", defaults._showViewsPanelInContextMenu.current());
	s._showHideMessageInContextMenu = j.value("showHideMessageInContextMenu", defaults._showHideMessageInContextMenu.current());
	s._showUserMessagesInContextMenu = j.value("showUserMessagesInContextMenu", defaults._showUserMessagesInContextMenu.current());
	s._showMessageDetailsInContextMenu = j.value("showMessageDetailsInContextMenu", defaults._showMessageDetailsInContextMenu.current());
	s._showRepeatMessageInContextMenu = j.value("showRepeatMessageInContextMenu", defaults._showRepeatMessageInContextMenu.current());
	s._showAddFilterInContextMenu = j.value("showAddFilterInContextMenu", defaults._showAddFilterInContextMenu.current());
	s._showAttachButtonInMessageField = j.value("showAttachButtonInMessageField", defaults._showAttachButtonInMessageField.current());
	s._showCommandsButtonInMessageField = j.value("showCommandsButtonInMessageField", defaults._showCommandsButtonInMessageField.current());
	s._showEmojiButtonInMessageField = j.value("showEmojiButtonInMessageField", defaults._showEmojiButtonInMessageField.current());
	s._showMicrophoneButtonInMessageField = j.value("showMicrophoneButtonInMessageField", defaults._showMicrophoneButtonInMessageField.current());
	s._showAutoDeleteButtonInMessageField = j.value("showAutoDeleteButtonInMessageField", defaults._showAutoDeleteButtonInMessageField.current());
	s._showAttachPopup = j.value("showAttachPopup", defaults._showAttachPopup.current());
	s._showEmojiPopup = j.value("showEmojiPopup", defaults._showEmojiPopup.current());
	s._showMyProfileInDrawer = j.value("showMyProfileInDrawer", defaults._showMyProfileInDrawer.current());
	s._showBotsInDrawer = j.value("showBotsInDrawer", defaults._showBotsInDrawer.current());
	s._showNewGroupInDrawer = j.value("showNewGroupInDrawer", defaults._showNewGroupInDrawer.current());
	s._showNewChannelInDrawer = j.value("showNewChannelInDrawer", defaults._showNewChannelInDrawer.current());
	s._showContactsInDrawer = j.value("showContactsInDrawer", defaults._showContactsInDrawer.current());
	s._showCallsInDrawer = j.value("showCallsInDrawer", defaults._showCallsInDrawer.current());
	s._showSavedMessagesInDrawer = j.value("showSavedMessagesInDrawer", defaults._showSavedMessagesInDrawer.current());
	s._showLReadToggleInDrawer = j.value("showLReadToggleInDrawer", defaults._showLReadToggleInDrawer.current());
	s._showSReadToggleInDrawer = j.value("showSReadToggleInDrawer", defaults._showSReadToggleInDrawer.current());
	s._showNightModeToggleInDrawer = j.value("showNightModeToggleInDrawer", defaults._showNightModeToggleInDrawer.current());
	s._showGhostToggleInDrawer = j.value("showGhostToggleInDrawer", defaults._showGhostToggleInDrawer.current());
	s._showStreamerToggleInDrawer = j.value("showStreamerToggleInDrawer", defaults._showStreamerToggleInDrawer.current());
	s._showGhostToggleInTray = j.value("showGhostToggleInTray", defaults._showGhostToggleInTray.current());
	s._showStreamerToggleInTray = j.value("showStreamerToggleInTray", defaults._showStreamerToggleInTray.current());
	s._monoFont = j.value("monoFont", defaults._monoFont.current());
	s._hideNotificationCounters = j.value("hideNotificationCounters", defaults._hideNotificationCounters.current());
	s._hideNotificationBadge = j.value("hideNotificationBadge", defaults._hideNotificationBadge.current());
	s._hideAllChatsFolder = j.value("hideAllChatsFolder", defaults._hideAllChatsFolder.current());
	s._channelBottomButton = j.value("channelBottomButton", defaults._channelBottomButton.current());
	s._quickAdminShortcuts = j.value("quickAdminShortcuts", defaults._quickAdminShortcuts.current());
	s._showPeerId = j.value("showPeerId", defaults._showPeerId.current());
	s._showMessageSeconds = j.value("showMessageSeconds", defaults._showMessageSeconds.current());
	s._showMessageShot = j.value("showMessageShot", defaults._showMessageShot.current());
	s._filterZalgo = j.value("filterZalgo", defaults._filterZalgo.current());
	s._stickerConfirmation = j.value("stickerConfirmation", defaults._stickerConfirmation.current());
	s._gifConfirmation = j.value("gifConfirmation", defaults._gifConfirmation.current());
	s._voiceConfirmation = j.value("voiceConfirmation", defaults._voiceConfirmation.current());
	s._translationProvider = j.value("translationProvider", defaults._translationProvider.current());
	s._adaptiveCoverColor = j.value("adaptiveCoverColor", defaults._adaptiveCoverColor.current());
	s._improveLinkPreviews = j.value("improveLinkPreviews", defaults._improveLinkPreviews.current());
	s._crashReporting = j.value("crashReporting", defaults._crashReporting.current());
	s._avatarCorners = j.value("avatarCorners", defaults._avatarCorners.current());
	s._singleCornerRadius = j.value("singleCornerRadius", defaults._singleCornerRadius.current());

	if (j.contains("messageShotSettings") && j["messageShotSettings"].is_object()) {
		j["messageShotSettings"].get_to(s._messageShotSettings);
	}
}
