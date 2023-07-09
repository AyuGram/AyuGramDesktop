// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2023

#include "telegram_helpers.h"
#include "data/data_peer_id.h"

Main::Session* getSession(long userId)
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

PeerId dialogIdToPeerId(long dialogId)
{
	auto peerId = PeerId();
	return peerId;
}
