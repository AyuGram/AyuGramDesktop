// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/translator/ayu_translate_provider.h"

#include "api/api_text_entities.h"
#include "ayu/features/translator/ayu_translator.h"
#include "base/weak_ptr.h"
#include "data/data_msg_id.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "main/main_session.h"

#include <memory>

namespace {

class AyuTranslateProvider final
	: public Ui::TranslateProvider
	, public base::has_weak_ptr {
public:
	AyuTranslateProvider(
		not_null<Main::Session*> session,
		TranslationProvider provider)
	: _session(session)
	, _provider(provider) {
		if (!Ayu::Translator::TranslateManager::currentInstance()) {
			Ayu::Translator::TranslateManager::init();
		}
	}

	[[nodiscard]] bool supportsMessageId() const override {
		return true;
	}

	void request(
			Ui::TranslateProviderRequest request,
			LanguageId to,
			Fn<void(Ui::TranslateProviderResult)> done) override {
		requestSingle(
			std::move(request),
			to,
			std::move(done));
	}

	void requestBatch(
			std::vector<Ui::TranslateProviderRequest> requests,
			const LanguageId &to,
			Fn<void(int, Ui::TranslateProviderResult)> doneOne,
			Fn<void()> doneAll) override {
		doneOne = [weak = base::make_weak(this), doneOne = std::move(doneOne)](
				int index,
				Ui::TranslateProviderResult result) {
			if (weak) {
				doneOne(index, std::move(result));
			}
		};
		doneAll = [weak = base::make_weak(this), doneAll = std::move(doneAll)] {
			if (weak) {
				doneAll();
			}
		};
		if (requests.empty()) {
			doneAll();
			return;
		}
		using Flag = MTPmessages_TranslateText::Flag;
		const auto failAll = [=] {
			for (auto i = 0; i != requests.size(); ++i) {
				doneOne(i, Ui::TranslateProviderResult{
					.error = Ui::TranslateProviderError::Unknown,
				});
			}
			doneAll();
		};
		const auto manager = Ayu::Translator::TranslateManager::currentInstance();
		if (!manager || to.twoLetterCode().isEmpty()) {
			failAll();
			return;
		}
		const auto doneFromResult = [=, session = _session](
				const MTPmessages_TranslatedText &result) {
			const auto &list = result.data().vresult().v;
			for (auto i = 0; i != requests.size(); ++i) {
				doneOne(
					i,
					(i < list.size())
						? Ui::TranslateProviderResult{
							.text = Api::ParseTextWithEntities(
								session,
								list[i]),
						}
						: Ui::TranslateProviderResult{
							.error = Ui::TranslateProviderError::Unknown,
						});
			}
			doneAll();
		};
		const auto firstPeer = PeerId(requests.front().peerId);
		auto allWithIds = true;
		for (const auto &request : requests) {
			if ((PeerId(request.peerId) != firstPeer) || (request.msgId == 0)) {
				allWithIds = false;
				break;
			}
		}
		if (allWithIds) {
			const auto peer = _session->data().peerLoaded(firstPeer);
			if (!peer) {
				failAll();
				return;
			}
			auto ids = QVector<MTPint>();
			ids.reserve(requests.size());
			for (const auto &request : requests) {
				ids.push_back(MTP_int(MsgId(request.msgId)));
			}
			manager->request(
				_session,
				MTP_flags(Flag::f_peer | Flag::f_id),
				peer->input(),
				MTP_vector<MTPint>(ids),
				MTPVector<MTPTextWithEntities>(),
				MTP_string(to.twoLetterCode()),
				_provider
			).done(doneFromResult).fail(failAll).send();
			return;
		}
		auto allWithText = true;
		for (const auto &request : requests) {
			if (request.text.text.isEmpty()) {
				allWithText = false;
				break;
			}
		}
		if (allWithText) {
			auto text = QVector<MTPTextWithEntities>();
			text.reserve(requests.size());
			for (const auto &request : requests) {
				text.push_back(MTP_textWithEntities(
					MTP_string(request.text.text),
					Api::EntitiesToMTP(
						_session,
						request.text.entities,
						Api::ConvertOption::SkipLocal)));
			}
			manager->request(
				_session,
				MTP_flags(Flag::f_text),
				MTP_inputPeerEmpty(),
				MTPVector<MTPint>(),
				MTP_vector<MTPTextWithEntities>(text),
				MTP_string(to.twoLetterCode()),
				_provider
			).done(doneFromResult).fail(failAll).send();
			return;
		}
		if (requests.size() == 1) {
			failAll();
			return;
		}
		struct State {
			int remaining = 0;
			Fn<void(int, Ui::TranslateProviderResult)> doneOne;
			Fn<void()> doneAll;
		};
		auto state = std::make_shared<State>(State{
			.remaining = int(requests.size()),
			.doneOne = std::move(doneOne),
			.doneAll = std::move(doneAll),
		});
		for (auto i = 0; i != requests.size(); ++i) {
			requestSingle(
				std::move(requests[i]),
				to,
				[=](Ui::TranslateProviderResult result) mutable {
					state->doneOne(i, std::move(result));
					if (!--state->remaining) {
						state->doneAll();
					}
				});
		}
	}

private:
	void requestSingle(
			Ui::TranslateProviderRequest request,
			LanguageId to,
			Fn<void(Ui::TranslateProviderResult)> done) {
		requestBatch(
			{ std::move(request) },
			to,
			[done = std::move(done)](
					int,
					Ui::TranslateProviderResult result) mutable {
				done(std::move(result));
			},
			[] {});
	}

	const not_null<Main::Session*> _session;
	const TranslationProvider _provider;
};

} // namespace

namespace Ui {

std::unique_ptr<TranslateProvider> CreateAyuTranslateProvider(
		not_null<Main::Session*> session,
		TranslationProvider provider) {
	return std::make_unique<AyuTranslateProvider>(session, provider);
}

} // namespace Ui
