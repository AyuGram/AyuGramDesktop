// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/message_shot/message_shot_theme_state.h"

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/utils/telegram_helpers.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "ui/style/style_palette_colorizer.h"
#include "window/themes/window_theme.h"
#include "window/themes/window_theme_preview.h"

#include <unordered_set>

namespace AyuFeatures::MessageShot {
namespace {

Window::Theme::EmbeddedType defaultSelected = Window::Theme::EmbeddedType(-1);
std::optional<QColor> defaultSelectedColor;
std::optional<Data::CloudTheme> customSelected;

rpl::event_stream<> resetDefaultSelectedStream;
rpl::event_stream<> resetCustomSelectedStream;
rpl::event_stream<Data::CloudTheme> themeChosenStream;
rpl::event_stream<style::palette> paletteChosenStream;

std::shared_ptr<style::palette> persistedPalette;
std::unordered_set<uint64> refreshedThemeAccounts;

std::optional<Data::CloudTheme> findCloudThemeById(
		not_null<Main::Session*> session,
		uint64 id) {
	const auto &chat = session->data().cloudThemes().chatThemes();
	const auto &list = session->data().cloudThemes().list();
	auto it = ranges::find(chat, id, &Data::CloudTheme::id);
	if (it != chat.end()) {
		return *it;
	}
	it = ranges::find(list, id, &Data::CloudTheme::id);
	if (it != list.end()) {
		return *it;
	}
	return std::nullopt;
}

bool tryApplyCloudThemePalette(
		not_null<Main::Session*> session,
		const Data::CloudTheme &theme,
		std::shared_ptr<style::palette> palette) {
	if (!theme.documentId) {
		return false;
	}

	const auto document = session->data().document(theme.documentId);
	const auto documentView = document->createMediaView();
	auto preview = Window::Theme::PreviewFromFile(
		documentView->bytes(),
		document->location().name(),
		theme);
	if (!preview) {
		return false;
	}

	palette->reset();
	palette->load(preview->instance.palette.save());
	setCustomSelected(theme);
	return true;
}

SavedThemeApplyResult tryApplyEmbeddedThemePalette(
		const MessageShotSettings &shot,
		std::shared_ptr<style::palette> palette,
		Fn<void()> onApplied) {
	if (shot.embeddedThemeType() == -1) {
		return SavedThemeApplyResult::Failed;
	}

	const auto type = static_cast<Window::Theme::EmbeddedType>(shot.embeddedThemeType());
	const auto themes = Window::Theme::EmbeddedThemes();
	const auto it = ranges::find(themes, type, &Window::Theme::EmbeddedScheme::type);
	if (it == themes.end()) {
		return SavedThemeApplyResult::Failed;
	}

	const auto accentRgb = shot.embeddedThemeAccentColor();
	if (it->path.isEmpty() && type == Window::Theme::EmbeddedType::Default) {
		style::palette embeddedPalette;
		const auto accent = (accentRgb != 0)
			? std::optional<QColor>(QColor::fromRgb(accentRgb))
			: std::nullopt;
		Window::Theme::PreparePaletteCallback(false, accent)(embeddedPalette);
		palette->reset();
		palette->load(embeddedPalette.save());
		setDefaultSelected(type);
		if (accent) {
			setDefaultSelectedColor(*accent);
		}
		if (onApplied) {
			onApplied();
		}
		return SavedThemeApplyResult::AppliedSync;
	}

	if (accentRgb != 0) {
		const auto color = QColor::fromRgb(accentRgb);
		const auto colorizer = Window::Theme::ColorizerFrom(*it, color);
		auto instance = Window::Theme::Instance();
		if (!Window::Theme::LoadFromFile(it->path, &instance, nullptr, nullptr, colorizer)) {
			return SavedThemeApplyResult::Failed;
		}

		palette->reset();
		palette->load(instance.palette.save());
		setDefaultSelected(type);
		setDefaultSelectedColor(color);
		if (onApplied) {
			onApplied();
		}
		return SavedThemeApplyResult::AppliedSync;
	}

	const Data::CloudTheme cloud;
	const auto preview = Window::Theme::PreviewFromFile(
		QByteArray(),
		it->path,
		cloud);
	if (!preview) {
		return SavedThemeApplyResult::Failed;
	}

	palette->reset();
	palette->load(preview->instance.palette.save());
	setDefaultSelected(type);
	if (onApplied) {
		onApplied();
	}
	return SavedThemeApplyResult::AppliedSync;
}

void applyCloudThemeAsync(
		not_null<Window::SessionController*> controller,
		not_null<Main::Session*> session,
		const Data::CloudTheme &theme,
		std::shared_ptr<style::palette> palette,
		Fn<void()> onApplied) {
	if (!theme.documentId) {
		return;
	}

	const auto weak = base::make_weak(controller);
	const auto document = session->data().document(theme.documentId);
	const auto documentView = document->createMediaView();

	document->save(
		Data::FileOriginTheme(theme.id, theme.accessHash),
		QString());

	const auto apply = [=] {
		if (!weak) {
			return;
		}

		auto preview = Window::Theme::PreviewFromFile(
			documentView->bytes(),
			document->location().name(),
			theme);
		if (!preview) {
			return;
		}

		palette->reset();
		palette->load(preview->instance.palette.save());
		setCustomSelected(theme);
		if (onApplied) {
			onApplied();
		}
	};

	if (documentView->loaded()) {
		apply();
		return;
	}

	session->downloaderTaskFinished(
	) | rpl::filter([=] {
		return documentView->loaded();
	}) | rpl::take(1) | rpl::on_next([=] {
		apply();
	}, controller->lifetime());
}

}

void setDefaultSelected(const Window::Theme::EmbeddedType type) {
	resetCustomSelected();
	defaultSelected = type;
}

Window::Theme::EmbeddedType getSelectedFromDefault() {
	return defaultSelected;
}

void setDefaultSelectedColor(const QColor color) {
	resetCustomSelected();
	defaultSelectedColor = color;
}

std::optional<QColor> getSelectedColorFromDefault() {
	return defaultSelectedColor;
}

void setCustomSelected(const Data::CloudTheme theme) {
	resetDefaultSelected();
	customSelected = theme;
}

std::optional<Data::CloudTheme> getSelectedFromCustom() {
	return customSelected;
}

void resetDefaultSelected() {
	defaultSelected = Window::Theme::EmbeddedType(-1);
	defaultSelectedColor = std::nullopt;
	resetDefaultSelectedStream.fire({});
}

void resetCustomSelected() {
	customSelected = std::nullopt;
	resetCustomSelectedStream.fire({});
}

rpl::producer<> resetDefaultSelectedEvents() {
	return resetDefaultSelectedStream.events();
}

rpl::producer<> resetCustomSelectedEvents() {
	return resetCustomSelectedStream.events();
}

void setTheme(Data::CloudTheme theme) {
	themeChosenStream.fire(std::move(theme));
}

rpl::producer<Data::CloudTheme> themeChosen() {
	return themeChosenStream.events();
}

void setPalette(const style::palette &palette) {
	paletteChosenStream.fire_copy(palette);
}

rpl::producer<style::palette> paletteChosen() {
	return paletteChosenStream.events();
}

void ensureChatThemesRefreshed() {
	const auto accountId = AyuSettings::getInstance().messageShotSettings().cloudThemeAccountId();
	if (!accountId) {
		return;
	}

	if (!refreshedThemeAccounts.emplace(accountId).second) {
		return;
	}

	const auto session = getSession(accountId);
	if (!session) {
		refreshedThemeAccounts.erase(accountId);
		return;
	}

	session->data().cloudThemes().refreshChatThemes();
	session->data().cloudThemes().refresh();
}

// from `std::vector<EmbeddedScheme> EmbeddedThemes()`
QString embeddedThemeDisplayName(Window::Theme::EmbeddedType type) {
	switch (type) {
	case Window::Theme::EmbeddedType::Default:
		return tr::lng_settings_theme_classic(tr::now);
	case Window::Theme::EmbeddedType::DayBlue:
		return tr::lng_settings_theme_day(tr::now);
	case Window::Theme::EmbeddedType::Night:
		return tr::lng_settings_theme_tinted(tr::now);
	case Window::Theme::EmbeddedType::NightGreen:
		return tr::lng_settings_theme_night(tr::now);
	}
	return tr::ayu_MessageShotThemeDefault(tr::now);
}

QString resolveThemeName() {
	const auto &shot = AyuSettings::getInstance().messageShotSettings();
	if (shot.cloudThemeId() != 0) {
		return shot.cloudThemeTitle();
	}
	if (shot.embeddedThemeType() != -1) {
		const auto type = static_cast<Window::Theme::EmbeddedType>(shot.embeddedThemeType());
		return embeddedThemeDisplayName(type);
	}
	return tr::ayu_MessageShotThemeDefault(tr::now);
}

std::shared_ptr<style::palette> getPersistedPalette() {
	return persistedPalette;
}

void setPersistedPalette(std::shared_ptr<style::palette> palette) {
	persistedPalette = std::move(palette);
}

SavedThemeApplyResult applySavedThemePalette(
		std::shared_ptr<style::palette> palette,
		Fn<void()> onApplied) {
	const auto &shot = AyuSettings::getInstance().messageShotSettings();

	if (shot.embeddedThemeType() != -1) {
		return tryApplyEmbeddedThemePalette(shot, std::move(palette), std::move(onApplied));
	}

	if (shot.cloudThemeId() == 0) {
		return SavedThemeApplyResult::Failed;
	}

	const auto session = getSession(shot.cloudThemeAccountId());
	if (!session) {
		return SavedThemeApplyResult::Failed;
	}

	if (const auto fromList = findCloudThemeById(session, shot.cloudThemeId())) {
		if (tryApplyCloudThemePalette(session, *fromList, palette)) {
			if (onApplied) {
				onApplied();
			}
			return SavedThemeApplyResult::AppliedSync;
		}
	}

	Data::CloudTheme saved;
	saved.id = shot.cloudThemeId();
	saved.accessHash = shot.cloudThemeAccessHash();
	saved.documentId = shot.cloudThemeDocumentId();
	saved.title = shot.cloudThemeTitle();
	if (tryApplyCloudThemePalette(session, saved, palette)) {
		if (onApplied) {
			onApplied();
		}
		return SavedThemeApplyResult::AppliedSync;
	}

	return SavedThemeApplyResult::AwaitingAsync;
}

void subscribeToCloudThemeLoad(
		not_null<Window::SessionController*> controller,
		std::shared_ptr<style::palette> palette,
		Fn<void()> onApplied) {
	const auto &shot = AyuSettings::getInstance().messageShotSettings();
	if (shot.cloudThemeId() == 0) {
		return;
	}

	const auto session = getSession(shot.cloudThemeAccountId());
	if (!session) {
		return;
	}

	if (getSelectedFromCustom().has_value()) {
		return;
	}

	const auto id = shot.cloudThemeId();
	const auto weak = base::make_weak(controller);
	const auto tryApplyFromTheme = [=](const Data::CloudTheme &theme) {
		applyCloudThemeAsync(controller, session, theme, palette, onApplied);
	};

	if (const auto theme = findCloudThemeById(session, id)) {
		tryApplyFromTheme(*theme);
		return;
	}

	const auto found = std::make_shared<bool>(false);
	const auto tryFind = [=] {
		if (*found || !weak) {
			return;
		}

		if (const auto theme = findCloudThemeById(session, id)) {
			*found = true;
			tryApplyFromTheme(*theme);
		}
	};

	session->data().cloudThemes().chatThemesUpdated(
	) | rpl::on_next(tryFind, controller->lifetime());
	session->data().cloudThemes().updated(
	) | rpl::on_next(tryFind, controller->lifetime());
}

}
