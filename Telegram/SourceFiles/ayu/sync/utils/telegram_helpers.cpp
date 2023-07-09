// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "telegram_helpers.h"
#include <functional>
#include <QTimer>
#include "data/data_peer_id.h"

#include "ayu/sync/models.h"

#include "data/data_session.h"
#include "history/history.h"


Main::Session* getSession(ID userId)
{
	for (auto& [index, account] : Core::App().domain().accounts())
	{
		if (const auto session = account->maybeSession())
		{
			if (session->userId().bare == userId)
			{
				return session;
			}
		}
	}

	return nullptr;
}

bool accountExists(ID userId)
{
	return userId == 0 || getSession(userId) != nullptr;
}

void dispatchToMainThread(std::function<void()> callback)
{
	auto timer = new QTimer();
	timer->moveToThread(qApp->thread());
	timer->setSingleShot(true);
	QObject::connect(timer, &QTimer::timeout, [=]()
	{
		callback();
		timer->deleteLater();
	});
	QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));
}

not_null<History*> getHistoryFromDialogId(ID dialogId, Main::Session* session)
{
	if (dialogId > 0)
	{
		return session->data().history(peerFromUser(dialogId));
	}

	auto history = session->data().history(peerFromChannel(abs(dialogId)));
	if (history->folderKnown())
	{
		return history;
	}

	return session->data().history(peerFromChat(abs(dialogId)));
}
