// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "settings_main.h"

#include <QDesktopServices>
#include <styles/style_ayu_icons.h>

#include "lang_auto.h"
#include "settings_appearance.h"
#include "settings_ayu.h"
#include "settings_chats.h"
#include "settings_general.h"
#include "settings_other.h"

#include "ayu/ayu_settings.h"
#include "ayu/ui/ayu_logo.h"
#include "core/version.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_settings.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/painter.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "window/window_session_controller_link_info.h"

namespace Settings {

rpl::producer<QString> AyuMain::title() {
	return rpl::single(QString(""));
}

AyuMain::AyuMain(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
	: Section(parent) {
	setupContent(controller);
}

void SetupAppLogo(not_null<Ui::VerticalLayout*> container) {
	const auto logo = container->add(
		object_ptr<Ui::CenterWrap<Ui::RpWidget>>(
			container,
			object_ptr<Ui::RpWidget>(container)));

	const auto widget = logo->entity();
	widget->resize(QSize(st::settingsCloudPasswordIconSize, st::settingsCloudPasswordIconSize));

	widget->paintRequest(
	) | rpl::start_with_next([=](QRect clip)
							 {
								 auto p = QPainter(widget);
								 const auto image = AyuAssets::currentAppLogoNoMargin(); // todo: svg renderer
								 if (!image.isNull()) {
									 const auto size = st::settingsCloudPasswordIconSize;
									 const auto scaled = image.scaled(
										 size * style::DevicePixelRatio(),
										 size * style::DevicePixelRatio(),
										 Qt::KeepAspectRatio,
										 Qt::SmoothTransformation);
									 p.drawImage(
										 QRect(0, 0, size, size),
										 scaled);
								 }
							 },
							 widget->lifetime());
}

void SetupCategories(
	not_null<Ui::VerticalLayout*> container,
	not_null<Window::SessionController*> controller,
	Fn<void(Type)> showOther) {
	struct CategoryInfo
	{
		QString name;
		const style::icon *icon;
		std::function<void()> handler;
	};

	const auto categories = std::vector<CategoryInfo>{
		{QString("AyuGram"), &st::menuIconGroupReactions, [=] { showOther(AyuGhost::Id()); }},
		// {QString("Filters"), &st::menuIconTagFilter, [=] { showOther(AyuFilters::Id()); }},
		{tr::ayu_CategoryGeneral(tr::now), &st::menuIconShowAll, [=] { showOther(AyuGeneral::Id()); }},
		{tr::ayu_CategoryAppearance(tr::now), &st::menuIconPalette, [=] { showOther(AyuAppearance::Id()); }},
		{tr::ayu_CategoryChats(tr::now), &st::menuIconChatBubble, [=] { showOther(AyuChats::Id()); }},
		{tr::ayu_CategoryOther(tr::now), &st::menuIconFave, [=] { showOther(AyuOther::Id()); }},
	};

	for (const auto &category : categories) {
		AddButtonWithIcon(
			container,
			rpl::single(category.name),
			st::settingsButton,
			{category.icon}
		)->setClickedCallback([=]
		{
			if (category.handler) {
				category.handler();
			}
		});
	}
}

void SetupLinks(
	not_null<Ui::VerticalLayout*> container,
	not_null<Window::SessionController*> controller) {
	struct LinkInfo
	{
		QString name;
		QString value;
		const style::icon *icon;
		std::function<void()> handler;
	};

	const auto links = std::vector<LinkInfo>{
		{
			tr::ayu_LinksChannel(tr::now),
			QString("@ayugram"),
			&st::menuIconChannel,
			[=]
			{
				controller->showPeerByLink(Window::PeerByLinkInfo{
					.usernameOrId = QString("ayugram"),
				});
			}
		},
		{
			tr::ayu_LinksChats(tr::now),
			QString("@ayugramchat"),
			&st::menuIconChats,
			[=]
			{
				controller->showPeerByLink(Window::PeerByLinkInfo{
					.usernameOrId = QString("ayugramchat"),
				});
			}
		},
		{
			tr::ayu_LinksTranslate(tr::now),
			QString("Crowdin"),
			&st::menuIconTranslate,
			[=]
			{
				QDesktopServices::openUrl(QString("https://translate.ayugram.one"));
			}
		},
		{
			tr::ayu_LinksDocumentation(tr::now),
			QString("docs.ayugram.one"),
			&st::menuIconIpAddress,
			[=]
			{
				QDesktopServices::openUrl(QString("https://docs.ayugram.one"));
			}
		},
	};

	for (const auto &link : links) {
		AddButtonWithLabel(
			container,
			rpl::single(link.name),
			rpl::single(link.value),
			st::settingsButton,
			{link.icon}
		)->setClickedCallback(link.handler);
	}
}

void AyuMain::setupContent(not_null<Window::SessionController*> controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	SetupAppLogo(content);

	AddSkip(content);

	content->add(
		object_ptr<Ui::CenterWrap<Ui::FlatLabel>>(
			content,
			object_ptr<Ui::FlatLabel>(
				content,
				rpl::single(QString("AyuGram Desktop v") + QString::fromLatin1(AppVersionStr)),
				st::boxTitle)));

	AddSkip(content);

	content->add(
		object_ptr<Ui::CenterWrap<Ui::FlatLabel>>(
			content,
			object_ptr<Ui::FlatLabel>(
				content,
				tr::ayu_SettingsDescription(),
				st::centeredBoxLabel)));

	AddSkip(content);
	AddSkip(content);
	AddSkip(content);
	AddSkip(content);
	AddDivider(content);
	AddSkip(content);

	AddSubsectionTitle(content, tr::ayu_CategoriesHeader());
	SetupCategories(content, controller, showOtherMethod());

	AddSkip(content);
	AddDivider(content);
	AddSkip(content);

	AddSubsectionTitle(content, tr::ayu_LinksHeader());
	SetupLinks(content, controller);

	AddSkip(content);

	ResizeFitChild(this, content);
}

} // namespace Settings