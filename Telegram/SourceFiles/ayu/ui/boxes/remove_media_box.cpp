// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/remove_media_box.h"

#include "apiwrap.h"
#include "lang_auto.h"
#include "base/call_delayed.h"
#include "base/random.h"
#include "data/data_channel.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace AyuUi {

namespace {

constexpr auto kBatchLimit = 100;
constexpr auto kBatchDelayMin = crl::time(500);
constexpr auto kBatchDelayJitter = 500;
constexpr auto kDoneCloseDelay = crl::time(1200);
constexpr auto kProgressBarHeight = 6;

enum class Phase {
	Selecting,
	Searching,
	Deleting,
	Done,
};

struct State {
	rpl::variable<Phase> phase = Phase::Selecting;
	rpl::variable<int> found = 0;
	rpl::variable<int> deleted = 0;
	rpl::variable<int> total = 0;
};

struct Selector {
	MTPMessagesFilter filter;
	bool stickersOnly = false;
};

bool MessageIsSticker(const MTPDmessage &data) {
	const auto media = data.vmedia();
	if (!media) {
		return false;
	}
	auto isSticker = false;
	media->match([&](const MTPDmessageMediaDocument &d) {
		if (const auto doc = d.vdocument()) {
			doc->match([&](const MTPDdocument &dd) {
				for (const auto &attr : dd.vattributes().v) {
					if (attr.type() == mtpc_documentAttributeSticker) {
						isSticker = true;
						return;
					}
				}
			}, [](const MTPDdocumentEmpty &) {
			});
		}
	}, [](const auto &) {
	});
	return isSticker;
}

void WalkMessageIds(
		const MTPmessages_Messages &response,
		bool stickersOnly,
		Fn<void(MsgId)> consume) {
	const auto handle = [&](const QVector<MTPMessage> &messages) {
		for (const auto &msg : messages) {
			msg.match([&](const MTPDmessage &data) {
				if (stickersOnly && !MessageIsSticker(data)) {
					return;
				}
				consume(MsgId(data.vid().v));
			}, [](const auto &) {
			});
		}
	};
	response.match(
		[&](const MTPDmessages_messages &d) { handle(d.vmessages().v); },
		[&](const MTPDmessages_messagesSlice &d) { handle(d.vmessages().v); },
		[&](const MTPDmessages_channelMessages &d) { handle(d.vmessages().v); },
		[&](const MTPDmessages_messagesNotModified &) {});
}

int RawMessagesCount(const MTPmessages_Messages &response) {
	auto count = 0;
	response.match(
		[&](const MTPDmessages_messages &d) { count = d.vmessages().v.size(); },
		[&](const MTPDmessages_messagesSlice &d) { count = d.vmessages().v.size(); },
		[&](const MTPDmessages_channelMessages &d) { count = d.vmessages().v.size(); },
		[&](const MTPDmessages_messagesNotModified &) {});
	return count;
}

class ProgressBar final : public Ui::RpWidget {
public:
	explicit ProgressBar(QWidget *parent) : RpWidget(parent) {
		resize(0, kProgressBarHeight);
	}
	void setValue(float64 value) {
		const auto clamped = std::clamp(value, 0., 1.);
		if (!qFuzzyCompare(_value, clamped)) {
			_value = clamped;
			update();
		}
	}

protected:
	void paintEvent(QPaintEvent *) override {
		auto p = QPainter(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto radius = height() / 2.;
		p.setPen(Qt::NoPen);
		p.setBrush(st::windowBgRipple);
		p.drawRoundedRect(rect(), radius, radius);
		const auto filled = int(width() * _value);
		if (filled > 0) {
			p.setBrush(st::windowBgActive);
			p.drawRoundedRect(QRect(0, 0, filled, height()), radius, radius);
		}
	}

private:
	float64 _value = 0.;
};

void RunDelete(
		not_null<PeerData*> peer,
		std::shared_ptr<base::flat_set<MsgId>> collected,
		bool revoke,
		std::shared_ptr<State> state) {
	const auto total = int(collected->size());
	state->total = total;
	state->deleted = 0;
	if (!total) {
		state->phase = Phase::Done;
		Ui::Toast::Show(tr::ayu_RemoveMediaNoneFound(tr::now));
		return;
	}
	state->phase = Phase::Deleting;
	const auto ordered = std::make_shared<std::vector<MsgId>>(
		collected->begin(),
		collected->end());
	const auto session = &peer->session();

	const auto step = std::make_shared<Fn<void(int)>>();
	*step = [=](int index) {
		if (index >= int(ordered->size())) {
			state->phase = Phase::Done;
			Ui::Toast::Show(tr::ayu_RemoveMediaDone(
				tr::now,
				lt_count,
				int(ordered->size())));
			return;
		}
		QVector<MTPint> ids;
		const auto take = std::min<int>(kBatchLimit, ordered->size() - index);
		ids.reserve(take);
		for (auto i = 0; i < take; ++i) {
			ids.push_back(MTP_int((*ordered)[index + i].bare));
		}

		const auto done = [=](const MTPmessages_AffectedMessages &result) {
			session->api().applyAffectedMessages(peer, result);
			if (peer->isChannel()) {
				session->data().processMessagesDeleted(peer->id, ids);
			} else {
				session->data().processNonChannelMessagesDeleted(ids);
			}
			const auto newDeleted = index + ids.size();
			state->deleted = newDeleted;
			const auto delay = kBatchDelayMin
				+ crl::time(base::RandomValue<int>() % kBatchDelayJitter);
			base::call_delayed(delay, [=] { (*step)(newDeleted); });
		};
		const auto fail = [=](const MTP::Error &error) {
			DEBUG_LOG(("RemoveMedia: delete batch failed: %1").arg(error.type()));
			const auto skipped = index + ids.size();
			state->deleted = skipped;
			base::call_delayed(crl::time(500), [=] { (*step)(skipped); });
		};

		if (const auto channel = peer->asChannel()) {
			session->api()
				.request(MTPchannels_DeleteMessages(
					channel->inputChannel(),
					MTP_vector<MTPint>(ids)))
				.done(done)
				.fail(fail)
				.handleFloodErrors()
				.send();
		} else {
			using Flag = MTPmessages_DeleteMessages::Flag;
			session->api()
				.request(MTPmessages_DeleteMessages(
					MTP_flags(revoke ? Flag::f_revoke : Flag(0)),
					MTP_vector<MTPint>(ids)))
				.done(done)
				.fail(fail)
				.handleFloodErrors()
				.send();
		}
	};
	(*step)(0);
}

void SearchAndDelete(
		not_null<PeerData*> peer,
		std::vector<Selector> selectors,
		bool revoke,
		std::shared_ptr<State> state) {
	const auto session = &peer->session();
	const auto collected = std::make_shared<base::flat_set<MsgId>>();
	const auto remaining = std::make_shared<int>(int(selectors.size()));

	state->phase = Phase::Searching;
	state->found = 0;

	const auto finishOne = [=] {
		if (--(*remaining) == 0) {
			RunDelete(peer, collected, revoke, state);
		}
	};

	for (const auto &selector : selectors) {
		const auto filter = selector.filter;
		const auto stickersOnly = selector.stickersOnly;
		const auto walker = std::make_shared<Fn<void(MsgId)>>();
		*walker = [=](MsgId offsetId) {
			session->api()
				.request(MTPmessages_Search(
					MTP_flags(0),
					peer->input(),
					MTP_string(),
					MTPInputPeer(),
					MTPInputPeer(),
					MTPVector<MTPReaction>(),
					MTP_int(0),
					filter,
					MTP_int(0),
					MTP_int(0),
					MTP_int(offsetId.bare),
					MTP_int(0),
					MTP_int(kBatchLimit),
					MTP_int(0),
					MTP_int(0),
					MTP_long(0)))
				.done([=](const MTPmessages_Messages &result) {
					MsgId minId;
					WalkMessageIds(result, stickersOnly, [&](MsgId id) {
						if (!id) {
							return;
						}
						if (!minId || id < minId) {
							minId = id;
						}
						if (collected->insert(id).second) {
							state->found = state->found.current() + 1;
						}
					});
					const auto rawCount = RawMessagesCount(result);
					if (rawCount == kBatchLimit && minId) {
						(*walker)(minId - MsgId(1));
					} else {
						finishOne();
					}
				})
				.fail([=](const MTP::Error &error) {
					DEBUG_LOG(("RemoveMedia: search failed: %1").arg(error.type()));
					finishOne();
				})
				.send();
		};
		(*walker)(MsgId(0));
	}
}

} // namespace

void FillRemoveMediaBox(
		not_null<Ui::GenericBox*> box,
		not_null<PeerData*> peer,
		not_null<Window::SessionController*> controller) {
	box->setTitle(tr::ayu_RemoveMediaTitle());

	const auto state = std::make_shared<State>();

	struct Entry {
		Ui::Checkbox *checkbox = nullptr;
		MTPMessagesFilter filter;
		bool stickersOnly = false;
	};
	const auto entries = box->lifetime().make_state<std::vector<Entry>>();

	const auto outer = box->verticalLayout();
	const auto selection = outer->add(object_ptr<Ui::VerticalLayout>(outer));

	const auto addType = [&](
			MTPMessagesFilter filter,
			bool stickersOnly,
			rpl::producer<QString> label) {
		const auto checkbox = selection->add(
			object_ptr<Ui::Checkbox>(
				selection,
				std::move(label),
				false,
				st::defaultBoxCheckbox),
			st::boxRowPadding + QMargins(0, st::boxLittleSkip / 2, 0, st::boxLittleSkip / 2));
		entries->push_back({ checkbox, filter, stickersOnly });
	};

	addType(MTP_inputMessagesFilterPhotos(), false, tr::ayu_RemoveMediaPhotos());
	addType(MTP_inputMessagesFilterVideo(), false, tr::ayu_RemoveMediaVideos());
	addType(MTP_inputMessagesFilterVoice(), false, tr::ayu_RemoveMediaVoice());
	addType(MTP_inputMessagesFilterRoundVideo(), false, tr::ayu_RemoveMediaVideoMessages());
	addType(MTP_inputMessagesFilterGif(), false, tr::ayu_RemoveMediaGifs());

	// Telegram's API exposes no inputMessagesFilterStickers, and the
	// server classifies stickers separately from files, so inputMessagesFilterDocument
	// returns zero stickers. The only correct path is to scan the entire
	// chat history with the empty filter and pick out documents whose
	// attributes include documentAttributeSticker on the client side.
	addType(MTP_inputMessagesFilterEmpty(), true, tr::ayu_RemoveMediaStickers());

	const auto user = peer->asUser();
	const auto isSelf = user && user->isSelf();
	const auto isBasicChat = peer->isChat();
	const auto showRevokeToggle = (user && !isSelf) || isBasicChat;

	Ui::Checkbox *revoke = nullptr;
	if (showRevokeToggle) {
		Ui::AddSkip(selection, st::boxLittleSkip);
		Ui::AddDivider(selection);
		Ui::AddSkip(selection, st::boxLittleSkip);

		auto revokeLabel = isBasicChat
			? tr::ayu_RemoveMediaRevokeGroup()
			: tr::ayu_RemoveMediaRevoke(lt_user, rpl::single(peer->shortName()));
		revoke = selection->add(
			object_ptr<Ui::Checkbox>(
				selection,
				std::move(revokeLabel),
				false,
				st::defaultBoxCheckbox),
			st::boxRowPadding + QMargins(0, st::boxLittleSkip / 2, 0, st::boxLittleSkip / 2));
	}

	const auto progress = outer->add(object_ptr<Ui::VerticalLayout>(outer));
	progress->hide();

	auto searchingText = rpl::combine(
		state->phase.value(),
		state->found.value()
	) | rpl::filter([](Phase phase, int) {
		return phase == Phase::Searching;
	}) | rpl::map([](Phase, int found) {
		return tr::ayu_RemoveMediaSearching(tr::now, lt_count, found);
	});

	auto deletingText = rpl::combine(
		state->phase.value(),
		state->deleted.value(),
		state->total.value()
	) | rpl::filter([](Phase phase, int, int) {
		return phase == Phase::Deleting || phase == Phase::Done;
	}) | rpl::map([](Phase, int deleted, int total) {
		return QString("%1 / %2").arg(deleted).arg(total);
	});

	progress->add(
		object_ptr<Ui::FlatLabel>(
			progress,
			rpl::merge(
				std::move(searchingText),
				std::move(deletingText)),
			st::boxLabel),
		st::boxRowPadding + QMargins(0, st::boxLittleSkip, 0, st::boxLittleSkip));

	const auto bar = progress->add(
		object_ptr<ProgressBar>(progress),
		st::boxRowPadding + QMargins(0, 0, 0, st::boxLittleSkip));

	rpl::combine(
		state->phase.value(),
		state->deleted.value(),
		state->total.value()
	) | rpl::on_next([=](Phase phase, int deleted, int total) {
		if (phase == Phase::Deleting && total > 0) {
			bar->setValue(float64(deleted) / float64(total));
		} else if (phase == Phase::Done) {
			bar->setValue(1.);
		} else {
			bar->setValue(0.);
		}
	}, box->lifetime());

	const auto removeButton = box->addButton(
		tr::ayu_RemoveMediaButton(),
		[=] {
			std::vector<Selector> selected;
			selected.reserve(entries->size());
			for (const auto &entry : *entries) {
				if (entry.checkbox->checked()) {
					selected.push_back({ entry.filter, entry.stickersOnly });
				}
			}
			if (selected.empty()) {
				Ui::Toast::Show(tr::ayu_RemoveMediaNothingSelected(tr::now));
				return;
			}
			const auto revokeChecked = revoke
				? revoke->checked()
				: !isSelf;
			selection->hide();
			progress->show();
			SearchAndDelete(peer, std::move(selected), revokeChecked, state);
		},
		st::attentionBoxButton);
	const auto cancelButton = box->addButton(tr::lng_cancel(), [=] {
		box->closeBox();
	});

	state->phase.value() | rpl::on_next([=](Phase phase) {
		const auto selecting = (phase == Phase::Selecting);
		if (removeButton) {
			removeButton->setVisible(selecting);
		}
	}, box->lifetime());

	state->phase.value() | rpl::filter([](Phase phase) {
		return phase == Phase::Done;
	}) | rpl::on_next([weak = base::make_weak(box.get())] {
		base::call_delayed(kDoneCloseDelay, [=] {
			if (const auto strong = weak.get()) {
				strong->closeBox();
			}
		});
	}, box->lifetime());
}

} // namespace AyuUi
