// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/delete_my_messages_box.h"

#include "apiwrap.h"
#include "lang_auto.h"
#include "base/call_delayed.h"
#include "base/random.h"
#include "data/data_channel.h"
#include "data/data_session.h"
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
	Confirm,
	Searching,
	Deleting,
	Done,
};

struct State {
	rpl::variable<Phase> phase = Phase::Confirm;
	rpl::variable<int> found = 0;
	rpl::variable<int> deleted = 0;
	rpl::variable<int> total = 0;
};

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
		std::shared_ptr<std::vector<MsgId>> collected,
		std::shared_ptr<State> state) {
	const auto total = int(collected->size());
	state->total = total;
	state->deleted = 0;
	if (!total) {
		state->phase = Phase::Done;
		Ui::Toast::Show(tr::ayu_DeleteOwnMessagesNone(tr::now));
		return;
	}
	state->phase = Phase::Deleting;
	const auto session = &peer->session();

	const auto step = std::make_shared<Fn<void(int)>>();
	*step = [=](int index) {
		if (index >= int(collected->size())) {
			state->phase = Phase::Done;
			Ui::Toast::Show(tr::ayu_DeleteOwnMessagesDone(
				tr::now,
				lt_count,
				int(collected->size())));
			return;
		}
		QVector<MTPint> ids;
		const auto take = std::min<int>(kBatchLimit, collected->size() - index);
		ids.reserve(take);
		for (auto i = 0; i < take; ++i) {
			ids.push_back(MTP_int((*collected)[index + i].bare));
		}

		const auto advance = [=](int newIndex) {
			state->deleted = newIndex;
			const auto delay = kBatchDelayMin
				+ crl::time(base::RandomValue<int>() % kBatchDelayJitter);
			base::call_delayed(delay, [=] { (*step)(newIndex); });
		};
		const auto done = [=](const MTPmessages_AffectedMessages &result) {
			session->api().applyAffectedMessages(peer, result);
			if (peer->isChannel()) {
				session->data().processMessagesDeleted(peer->id, ids);
			} else {
				session->data().processNonChannelMessagesDeleted(ids);
			}
			advance(index + ids.size());
		};
		const auto fail = [=](const MTP::Error &error) {
			DEBUG_LOG(("DeleteOwnMessages: batch failed: %1").arg(error.type()));
			advance(index + ids.size());
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
					MTP_flags(Flag::f_revoke),
					MTP_vector<MTPint>(ids)))
				.done(done)
				.fail(fail)
				.handleFloodErrors()
				.send();
		}
	};
	(*step)(0);
}

void SearchOwn(
		not_null<PeerData*> peer,
		std::shared_ptr<State> state) {
	const auto session = &peer->session();
	const auto collected = std::make_shared<std::vector<MsgId>>();

	state->phase = Phase::Searching;
	state->found = 0;

	const auto walker = std::make_shared<Fn<void(MsgId)>>();
	*walker = [=](MsgId offsetId) {
		using Flag = MTPmessages_Search::Flag;
		session->api()
			.request(MTPmessages_Search(
				MTP_flags(Flag::f_from_id),
				peer->input(),
				MTP_string(),
				MTP_inputPeerSelf(),
				MTPInputPeer(),
				MTPVector<MTPReaction>(),
				MTP_int(0),
				MTP_inputMessagesFilterEmpty(),
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
				auto batchCount = 0;
				const auto handle = [&](const QVector<MTPMessage> &messages) {
					batchCount = int(messages.size());
					for (const auto &msg : messages) {
						msg.match([&](const MTPDmessage &data) {
							const auto id = MsgId(data.vid().v);
							if (!minId || id < minId) {
								minId = id;
							}
							collected->push_back(id);
							state->found = state->found.current() + 1;
						}, [](const auto &) {
						});
					}
				};
				result.match(
					[&](const MTPDmessages_messages &d) { handle(d.vmessages().v); },
					[&](const MTPDmessages_messagesSlice &d) { handle(d.vmessages().v); },
					[&](const MTPDmessages_channelMessages &d) { handle(d.vmessages().v); },
					[&](const MTPDmessages_messagesNotModified &) {});
				if (batchCount == kBatchLimit && minId) {
					(*walker)(minId - MsgId(1));
				} else {
					RunDelete(peer, collected, state);
				}
			})
			.fail([=](const MTP::Error &error) {
				DEBUG_LOG(("DeleteOwnMessages: search failed: %1").arg(error.type()));
				RunDelete(peer, collected, state);
			})
			.send();
	};
	(*walker)(MsgId(0));
}

} // namespace

void FillDeleteMyMessagesBox(
		not_null<Ui::GenericBox*> box,
		not_null<PeerData*> peer,
		not_null<Window::SessionController*> controller) {
	box->setTitle(tr::ayu_DeleteOwnMessages());

	const auto state = std::make_shared<State>();
	const auto outer = box->verticalLayout();

	const auto confirm = outer->add(object_ptr<Ui::VerticalLayout>(outer));
	confirm->add(
		object_ptr<Ui::FlatLabel>(
			confirm,
			tr::ayu_DeleteOwnMessagesConfirmation(),
			st::boxLabel),
		st::boxRowPadding + QMargins(0, st::boxLittleSkip, 0, st::boxLittleSkip));

	const auto progress = outer->add(object_ptr<Ui::VerticalLayout>(outer));
	progress->hide();

	auto searchingText = rpl::combine(
		state->phase.value(),
		state->found.value()
	) | rpl::filter([](Phase phase, int) {
		return phase == Phase::Searching;
	}) | rpl::map([](Phase, int found) {
		return tr::ayu_DeleteOwnMessagesSearching(tr::now, lt_count, found);
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

	const auto deleteButton = box->addButton(
		tr::lng_box_delete(),
		[=] {
			confirm->hide();
			progress->show();
			SearchOwn(peer, state);
		},
		st::attentionBoxButton);
	const auto cancelButton = box->addButton(tr::lng_cancel(), [=] {
		box->closeBox();
	});

	state->phase.value() | rpl::on_next([=](Phase phase) {
		if (deleteButton) {
			deleteButton->setVisible(phase == Phase::Confirm);
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
