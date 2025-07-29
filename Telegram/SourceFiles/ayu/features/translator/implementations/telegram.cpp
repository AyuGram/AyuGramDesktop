// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "telegram.h"

#include <QtNetwork/QNetworkReply>

#include "api/api_text_entities.h"
#include "main/main_session.h"

namespace Ayu::Translator {

TelegramTranslator &TelegramTranslator::instance() {
	static TelegramTranslator inst;
	return inst;
}

TelegramTranslator::TelegramTranslator(QObject *parent)
	: BaseTranslator(parent) {
}

CallbackCancel TelegramTranslator::startTranslation(
	const StartTranslationArgs &args
) {
	const auto requestId = args.session->api().request(MTPmessages_TranslateText(
		args.requestData.flags,
		args.requestData.peer,
		args.requestData.idList,
		args.requestData.text,
		args.requestData.toLang
	)).done([=](const MTPmessages_TranslatedText &result)
	{
		const auto &data = result.data();
		const auto &list = data.vresult().v;
		if (list.isEmpty()) {
			args.onFail();
		} else {
			auto vec = std::vector<TextWithEntities>();
			vec.reserve(list.size());
			for (const auto &item : list) {
				const auto &d = item.data();
				vec.push_back(TextWithEntities{
					.text = qs(d.vtext()),
					.entities = Api::EntitiesFromMTP(
						args.session,
						d.ventities().v)
				});
			}
			args.onSuccess(vec);
		}
	}).fail([=](const MTP::Error &)
	{
		args.onFail();
	}).send();

	return [=]
	{
		args.session->api().request(requestId).cancel();
	};
}

}
