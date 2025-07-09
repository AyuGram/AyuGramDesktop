/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/about_box.h"

#include "base/platform/base_platform_info.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "core/update_checker.h"
#include "lang/lang_keys.h"
#include "ui/boxes/confirm_box.h"
#include "ui/text/text_utilities.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>

#include "window/window_session_controller.h"
#include "window/window_session_controller_link_info.h"

namespace {

rpl::producer<TextWithEntities> Text() {
	return tr::lng_about_text2(
		lt_gpl_link,
		rpl::single(Ui::Text::Link(
			"GNU GPL",
			"https://github.com/AyuGram/AyuGramDesktop/blob/master/LICENSE")),
		lt_github_link,
		rpl::single(Ui::Text::Link(
			"GitHub",
			"https://github.com/AyuGram/AyuGramDesktop")),
		Ui::Text::WithEntities);
}

} // namespace

void AboutBox(not_null<Ui::GenericBox*> box, Window::SessionController* controller) {
	box->setTitle(rpl::single(u"AyuGram Desktop"_q));

	auto layout = box->verticalLayout();

	const auto version = layout->add(
		object_ptr<Ui::LinkButton>(
			box,
			tr::lng_about_version(
				tr::now,
				lt_version,
				currentVersionText()),
			st::aboutVersionLink),
		QMargins(
			st::boxRowPadding.left(),
			-st::lineWidth * 3,
			st::boxRowPadding.right(),
			st::boxRowPadding.bottom()));
	version->setClickedCallback([=] {
		File::OpenUrl(Core::App().changelogLink());
	});

	Ui::AddSkip(layout, st::aboutTopSkip);

	const auto addText = [&](rpl::producer<TextWithEntities> text) {
		const auto label = layout->add(
			object_ptr<Ui::FlatLabel>(box, std::move(text), st::aboutLabel),
			st::boxRowPadding);
		label->setLinksTrusted();
		Ui::AddSkip(layout, st::aboutSkip);
	};

	addText(Text());

	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	box->addLeftButton(
		rpl::single(QString("@AyuGramReleases")),
		[box, controller]
		{
			box->closeBox();
			controller->showPeerByLink(Window::PeerByLinkInfo{
				.usernameOrId = QString("ayugramreleases"),
			});
		});

	box->setWidth(st::aboutWidth);
}

QString currentVersionText() {
	auto result = QString::fromLatin1(AppVersionStr);
	if (cAlphaVersion()) {
		result += u" alpha %1"_q.arg(cAlphaVersion() % 1000);
	} else if (AppBetaVersion) {
		result += " beta";
	}
	if (Platform::IsWindows64Bit()) {
		result += " x64";
	} else if (Platform::IsWindowsARM64()) {
		result += " arm64";
	}
#ifdef _DEBUG
	result += " DEBUG";
#endif
	return result;
}
