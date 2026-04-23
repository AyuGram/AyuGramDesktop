/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mtproto/sender.h"
#include "ui/text/text_entity.h"

#include <optional>
#include <vector>

class ApiWrap;

namespace Main {
class Session;
} // namespace Main

namespace Api {

class ComposeWithAi final {
public:
	struct Request {
		TextWithEntities text;
		QString translateToLang;
		QString changeTone;
		bool proofread = false;
		bool emojify = false;
	};

	struct DiffEntity {
		enum class Type {
			Insert,
			Replace,
			Delete,
		};

		Type type = Type::Insert;
		int offset = 0;
		int length = 0;
		QString oldText;
	};

	struct Diff {
		TextWithEntities text;
		std::vector<DiffEntity> entities;
	};

	struct Result {
		TextWithEntities resultText;
		std::optional<Diff> diffText;
	};

	explicit ComposeWithAi(not_null<ApiWrap*> api);

	[[nodiscard]] mtpRequestId request(
		Request request,
		Fn<void(Result &&)> done,
		Fn<void(const MTP::Error &)> fail = nullptr);
	void cancel(mtpRequestId requestId);

private:
	[[nodiscard]] static Diff ParseDiff(
		not_null<Main::Session*> session,
		const MTPTextWithEntities &text);

	const not_null<Main::Session*> _session;
	MTP::Sender _api;

};

} // namespace Api
