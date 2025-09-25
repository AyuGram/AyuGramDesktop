// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "ayu/data/entities.h"
#include "boxes/peer_list_box.h"
#include "boxes/peer_list_controllers.h"
#include "data/data_peer.h"
#include "history/history.h"

class RegexFilterGlobalExclusion;

namespace Main {
class Session;
} // namespace Main

namespace Settings {

class PerDialogFiltersListRow final : public PeerListRow
{
public:
	explicit PerDialogFiltersListRow(PeerId peer);
	QString generateName() override;
	PaintRoundImageCallback generatePaintUserpicCallback(bool forceRound) override;

private:
	PeerId peerId;
};

class PerDialogFiltersListController final : public PeerListController
{
public:
	explicit PerDialogFiltersListController(not_null<Main::Session*> session,
										   not_null<Window::SessionController*> controller);

	[[nodiscard]] Main::Session &session() const override;

	void prepare() override;

	void rowClicked(not_null<PeerListRow*> row) override;

private:
	struct FilterCounts
	{
		int filters = 0;
		int exclusions = 0;
	};
	std::unordered_map<ID, FilterCounts> countsByDialogIds;

	const not_null<Main::Session*> _session;
	not_null<Window::SessionController*> _controller;
};

class SelectChatBoxController
	: public ChatsListBoxController
	  , public base::has_weak_ptr
{
public:
	explicit SelectChatBoxController(
		not_null<Window::SessionController*> controller,
		Fn<void(not_null<PeerData*>)> onSelectedCallback
	);

	Main::Session &session() const override;
	void rowClicked(not_null<PeerListRow*> row) override;

protected:
	std::unique_ptr<Row> createRow(not_null<History*> history) override;
	void prepareViewHook() override;

private:
	not_null<Window::SessionController*> _controller;
	Fn<void(not_null<PeerData*>)> _onSelectedCallback;
};
} // namespace Settings