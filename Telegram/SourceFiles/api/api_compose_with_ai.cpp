/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_compose_with_ai.h"

#include "api/api_text_entities.h"
#include "apiwrap.h"

namespace Api {
namespace {

[[nodiscard]] MTPTextWithEntities Serialize(
		not_null<Main::Session*> session,
		const TextWithEntities &text) {
	return MTP_textWithEntities(
		MTP_string(text.text),
		EntitiesToMTP(session, text.entities, ConvertOption::SkipLocal));
}

} // namespace

ComposeWithAi::ComposeWithAi(not_null<ApiWrap*> api)
: _session(&api->session())
, _api(&api->instance()) {
}

mtpRequestId ComposeWithAi::request(
		Request request,
		Fn<void(Result &&)> done,
		Fn<void(const MTP::Error &)> fail) {
	using Flag = MTPmessages_composeMessageWithAI::Flag;
	auto flags = MTPmessages_composeMessageWithAI::Flags(0);
	if (request.proofread) {
		flags |= Flag::f_proofread;
	}
	if (!request.translateToLang.isEmpty()) {
		flags |= Flag::f_translate_to_lang;
	}
	if (request.tone) {
		flags |= Flag::f_tone;
	}
	if (request.emojify) {
		flags |= Flag::f_emojify;
	}
	const auto session = _session;
	return _api.request(MTPmessages_ComposeMessageWithAI(
		MTP_flags(flags),
		Serialize(session, request.text),
		request.translateToLang.isEmpty()
			? MTPstring()
			: MTP_string(request.translateToLang),
		request.tone
			? (request.tone->id
				? MTP_inputAiComposeToneID(
					MTP_long(request.tone->id),
					MTP_long(request.tone->accessHash))
				: MTP_inputAiComposeToneDefault(
					MTP_string(request.tone->defaultTone)))
			: MTPInputAiComposeTone()
	)).done([=, done = std::move(done)](
			const MTPmessages_ComposedMessageWithAI &result) mutable {
		const auto &data = result.data();
		auto parsed = Result{
			.resultText = ParseTextWithEntities(session, data.vresult_text()),
		};
		if (const auto diff = data.vdiff_text()) {
			parsed.diffText = ParseDiff(session, *diff);
		}
		done(std::move(parsed));
	}).fail([=, fail = std::move(fail)](const MTP::Error &error) mutable {
		if (fail) {
			fail(error);
		}
	}).send();
}

void ComposeWithAi::cancel(mtpRequestId requestId) {
	if (requestId) {
		_api.request(requestId).cancel();
	}
}

ComposeWithAi::Diff ComposeWithAi::ParseDiff(
		not_null<Main::Session*> session,
		const MTPTextWithEntities &text) {
	const auto &data = text.data();
	auto result = Diff{
		.text = ParseTextWithEntities(session, text),
	};
	const auto &entities = data.ventities().v;
	result.entities.reserve(entities.size());
	for (const auto &entity : entities) {
		entity.match([&](const MTPDmessageEntityDiffInsert &data) {
			result.entities.push_back({
				.type = DiffEntity::Type::Insert,
				.offset = data.voffset().v,
				.length = data.vlength().v,
			});
		}, [&](const MTPDmessageEntityDiffReplace &data) {
			result.entities.push_back({
				.type = DiffEntity::Type::Replace,
				.offset = data.voffset().v,
				.length = data.vlength().v,
				.oldText = qs(data.vold_text()),
			});
		}, [&](const MTPDmessageEntityDiffDelete &data) {
			result.entities.push_back({
				.type = DiffEntity::Type::Delete,
				.offset = data.voffset().v,
				.length = data.vlength().v,
			});
		}, [](const auto &) {
		});
	}
	return result;
}

} // namespace Api
