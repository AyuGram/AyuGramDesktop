// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#pragma once

#include "mtproto/sender.h"

#include <functional>
#include <list>
#include <unordered_map>
#include <QtCore/QString>

#include "implementations/base.h"

class QNetworkReply;

namespace Main {
class Session;
}

namespace Ayu::Translator {

class TranslateManager
{
public:
    using Request = MTPmessages_TranslateText;
    using Result = Request::ResponseType;

    class Builder
    {
    public:
        Builder(
            TranslateManager &manager,
            Main::Session &session,
            const MTPflags<MTPmessages_translateText::Flags> &flags,
            const MTPInputPeer &peer,
            const MTPVector<MTPint> &id,
            const MTPVector<MTPTextWithEntities> &text,
            const MTPstring &to_lang
        );

        Builder(Builder &&) noexcept = default;
        Builder &operator=(Builder &&) noexcept = default;

        Builder &done(std::function<void(const Result &)> cb);
        Builder &fail(std::function<void(const MTP::Error &)> cb);
        Builder &fail(std::function<void()> cb);

        mtpRequestId send();
        void cancel();

        [[nodiscard]] const auto &session() const { return _session; }
        [[nodiscard]] const auto &flags() const { return _flags; }
        [[nodiscard]] const auto &peer() const { return _peer; }
        [[nodiscard]] const auto &ids() const { return _idList; }
        [[nodiscard]] const auto &texts() const { return _text; }
        [[nodiscard]] const auto &toLang() const { return _toLang; }

    private:
        TranslateManager *_manager = nullptr;
        not_null<Main::Session*> _session;
        mtpRequestId _id = 0;

        MTPflags<MTPmessages_translateText::Flags> _flags;
        MTPInputPeer _peer;
        MTPVector<MTPint> _idList;
        MTPVector<MTPTextWithEntities> _text;
        MTPstring _toLang;

        std::function<void(const Result &)> _done;
        std::function<void(const MTP::Error &)> _fail;

        friend class TranslateManager;
    };

    TranslateManager() = default;
    ~TranslateManager() = default;

    Builder request(
        Main::Session &session,
        const MTPflags<MTPmessages_translateText::Flags> &flags,
        const MTPInputPeer &peer,
        const MTPVector<MTPint> &id,
        const MTPVector<MTPTextWithEntities> &text,
        const MTPstring &to_lang
    );

    [[nodiscard]] mtpRequestId performTranslation(Builder &req);

    bool cancel(mtpRequestId requestId);

    bool triggerDone(mtpRequestId id, const Result &result);
    bool triggerFail(mtpRequestId id);

    void resetCache();

    static TranslateManager *currentInstance();
    static void init();
    static TranslateManager *instance;

private:
    struct CacheEntry
    {
        TextWithEntities originalText;
        TextWithEntities translatedText;
        QString fromLang;
        QString toLang;
    };

    using CacheKey = QString;
    using CacheIterator = std::list<std::pair<CacheKey, CacheEntry>>::iterator;

    std::list<std::pair<CacheKey, CacheEntry>> _cacheList;
    std::unordered_map<CacheKey, CacheIterator> _cacheMap;
    static constexpr size_t MAX_CACHE_SIZE = 500;

    QString generateCacheKey(const QString &text, const QString &fromLang, const QString &toLang) const;
    QString generateMessageCacheKey(PeerId peerId, MsgId msgId, const QString &fromLang, const QString &toLang) const;
    void insertToCache(const QString &key, const CacheEntry &entry);
    std::optional<CacheEntry> getFromCache(const QString &key);
    void removeLeastRecentlyUsed();

    struct Pending
    {
        std::function<void(const Result &)> done;
        std::function<void(const MTP::Error &)> fail;
        CallbackCancel cancel;
    };

    mtpRequestId _nextId = 1;
    std::unordered_map<mtpRequestId, Pending> _pending;
};

} // namespace Ayu::Translator
