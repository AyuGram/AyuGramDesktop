// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "ayu/data/entities.h"
#include "settings/settings_common.h"
#include "settings/settings_common_session.h"
#include "ui/layers/box_content.h"
#include "ui/widgets/popup_menu.h"

class BoxContent;

namespace Window {
class Controller;
class SessionController;
} // namespace Window

namespace Settings {

class AyuFiltersList : public Section<AyuFiltersList>
{
public:
	AyuFiltersList(QWidget *parent, not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

	void checkBeforeClose(Fn<void()> close) override;

private:
	void setupContent(not_null<Window::SessionController*> controller);
	void initializeSharedFilters(not_null<Ui::VerticalLayout*> container);

	void addNewFilter(const RegexFilter &filter, bool exclusion = false);

	not_null<Window::SessionController*> _controller;
	not_null<Ui::VerticalLayout*> _content;

	std::vector<RegexFilter> filters;
	std::vector<RegexFilter> exclusions;

	Ui::FlatLabel *filtersTitle = nullptr;
	Ui::FlatLabel *excludedTitle = nullptr;

	std::optional<long long> dialogId;
};

} // namespace Settings