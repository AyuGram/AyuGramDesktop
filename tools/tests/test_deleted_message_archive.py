#!/usr/bin/env python3
"""Exercise the production archive queries and pagination without building Telegram."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "Telegram/SourceFiles/ayu/data"


def function(source, signature):
    return signature + source.split(signature, 1)[1].split("\n}", 1)[0] + "\n}\n"


class DeletedMessageArchiveTest(unittest.TestCase):
    def test_storage_and_pagination(self):
        database = (DATA / "ayu_database.cpp").read_text(encoding="utf-8-sig")
        messages = (DATA / "messages_storage.cpp").read_text(encoding="utf-8-sig")
        entities = (DATA / "entities.h").read_text()
        entities = entities[entities.index("using ID ="):]
        start = entities.index("\t[[nodiscard]] QJsonObject toJson()")
        end = entities.index("\n\t}\n", start) + len("\n\t}\n")
        entities = entities[:start] + entities[end:]
        storage = "auto storage = make_storage(" + database.split(
            "auto storage = make_storage(", 1
        )[1].split("\n);", 1)[0] + "\n);\n"
        storage = storage.replace('"./tdata/ayudata.db"', '"archive.db"')
        legacy = storage.replace("auto storage =", "auto legacy =")
        legacy = legacy.replace(
            'make_column("mimeType", &DeletedMessage::mimeType),\n'
            '\t\tmake_column("snapshot", &DeletedMessage::snapshot,\n'
            '\t\t\tdefault_value(std::vector<char>()))',
            'make_column("mimeType", &DeletedMessage::mimeType)',
        )
        prefix = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "ayu/libs/sqlite/sqlite_orm.h"
#define LOG(...)
using namespace sqlite_orm;
struct SchemaVersion { int id; int version; };
std::mutex databaseMutex;
'''
        queries = function(database, "bool addDeletedMessage(") + function(
            database, "std::vector<DeletedMessage> getDeletedMessagesSlice("
        )
        pagination = function(messages, "std::vector<not_null<HistoryItem*>> restoreDeletedMessages(")
        eviction = function(messages, "bool canUnloadDeletedMessage(")
        history = (ROOT / "Telegram/SourceFiles/history/history.cpp").read_text()
        unloading = function(history, "void History::unloadDeletedMessagesLater(")
        unloading += function(history, "void History::unloadDeletedMessages(")
        filters = (ROOT / "Telegram/SourceFiles/ayu/features/filters/filters_cache_controller.cpp").read_text()
        invalidation = function(filters, "void invalidateSingle(")
        mocks = r'''
namespace ranges = std::ranges;
namespace Core {
bool quitting = false;
bool Quitting() { return quitting; }
}
template <typename T> using not_null = T;
template <typename T> using QVector = std::vector<T>;
struct MsgId {
    ID bare = 0;
    MsgId(ID value = 0) : bare(value) {}
    explicit operator bool() const { return bare != 0; }
    auto operator<=>(const MsgId&) const = default;
};
const auto ServerMaxMsgId = MsgId(ID(1) << 56);
bool IsServerMsgId(MsgId id) { return id.bare > 0 && id.bare < ServerMaxMsgId.bare; }
struct MTPMessage { ID value; };
MsgId IdFromMessage(MTPMessage message) { return message.value; }
namespace Data { enum class ViewRemovalReason { Detached }; }
struct PeerId { static constexpr auto kChatTypeMask = ~uint64_t(0); ID value = 1; };
struct PeerData { PeerId id; };
struct FullMsgId {
    PeerId peer;
    MsgId msg;
    bool operator==(const FullMsgId &other) const {
        return peer.value == other.peer.value && msg == other.msg;
    }
};
ID getDialogIdFromPeer(PeerData *peer) { return peer->id.value; }
namespace AyuSettings {
struct Settings { bool saveDeletedMessages() const { return true; } };
Settings getInstance() { return {}; }
}
struct DocumentData {
    bool busy = false;
    bool loading() const { return busy; }
};
struct PhotoData {
    bool busy = false;
    bool loading() const { return busy; }
};
struct FakeMedia {
    DocumentData *file = nullptr;
    PhotoData *image = nullptr;
    DocumentData *document() const { return file; }
    PhotoData *photo() const { return image; }
};
struct AudioMsgId {
    enum class Type { Song, Voice };
    DocumentData *file = nullptr;
    FullMsgId context;
    DocumentData *audio() const { return file; }
    FullMsgId contextId() const { return context; }
};
namespace Media::Player {
struct Instance {
    AudioMsgId song, voice;
    AudioMsgId current(AudioMsgId::Type type) const {
        return type == AudioMsgId::Type::Song ? song : voice;
    }
};
Instance *instance() {
    static auto result = Instance();
    return &result;
}
}
struct History;
struct HistoryItem {
    MsgId id;
    bool saved = false;
    History *parent = nullptr;
    bool visible = false;
    FakeMedia *attachment = nullptr;
    void removeMainView(Data::ViewRemovalReason) {}
    void setDeleted(bool value) { saved = value; }
    bool isDeletedSaved() const { return saved; }
    void markDeletedAnimated() {}
    History *history() const { return parent; }
    FakeMedia *media() const { return attachment; }
    FullMsgId fullId() const;
    void destroy();
};
struct Session {
    struct User { ID bare = 1; };
    History *parent = nullptr;
    bool alive = true;
    User userId() const { return {}; }
    Session &data() { return *this; }
    History *historyLoaded(PeerId) { return parent; }
};
namespace crl {
std::vector<std::function<void()>> pending;
void on_main(Session *context, std::function<void()> callback) {
    pending.push_back([=] { if (context->alive) callback(); });
}
void drain() {
    const auto callbacks = std::exchange(pending, {});
    for (const auto &callback : callbacks) callback();
}
}
struct Delegate {
    bool active = false;
    bool hasCurrent() const { return active; }
};
struct History {
    enum class Flag : unsigned { UnloadDeletedMessagesScheduled = 1 };
    enum class ClearType { Unload };
    unsigned _flags = 0;
    Delegate delegate;
    Delegate *_delegateMixin = &delegate;
    PeerData peerData;
    PeerData *peer = &peerData;
    Session sessionData{ this };
    MsgId low, high;
    bool bottom = false;
    HistoryItem *last = nullptr;
    std::vector<std::unique_ptr<HistoryItem>> _items;
    std::vector<ID> destroyed;
    int clears = 0;
    Session &session() { return sessionData; }
    History &owner() { return *this; }
    MsgId minMsgId() const { return low; }
    MsgId maxMsgId() const { return high; }
    bool loadedAtBottom() const { return bottom; }
    HistoryItem *lastMessage() const { return last; }
    HistoryItem *message(PeerData*, ID id) {
        for (const auto &item : _items) {
            if (item->id.bare == id) return item.get();
        }
        return nullptr;
    }
    HistoryItem *message(FullMsgId id) { return message(peer, id.msg.bare); }
    HistoryItem *make(ID id, bool saved) {
        if (const auto item = message(peer, id)) return item;
        _items.push_back(std::make_unique<HistoryItem>(HistoryItem{ MsgId(id), saved, this }));
        return _items.back().get();
    }
    std::vector<HistoryItem*> createItems(const QVector<MTPMessage> &slice) {
        auto result = std::vector<HistoryItem*>();
        for (const auto entry : slice) result.push_back(make(entry.value, false));
        return result;
    }
    bool queryItemVisibility(HistoryItem *item) const { return item->visible; }
    void clear(ClearType) { ++clears; low = high = 0; bottom = false; }
    void unloadDeletedMessagesLater();
    void unloadDeletedMessages();
};
unsigned operator&(unsigned flags, History::Flag flag) { return flags & unsigned(flag); }
unsigned operator~(History::Flag flag) { return ~unsigned(flag); }
unsigned &operator|=(unsigned &flags, History::Flag flag) { return flags |= unsigned(flag); }
FullMsgId HistoryItem::fullId() const { return { parent->peer->id, id }; }
void HistoryItem::destroy() {
    const auto owner = parent;
    owner->destroyed.push_back(id.bare);
    std::erase_if(owner->_items, [=](const auto &item) { return item.get() == this; });
}
HistoryItem *RestoreSnapshot(History *history, const DeletedMessage &message) {
    return history->make(message.messageId, true);
}
constexpr auto kDeletedMessagesPerPage = 100;
'''
        checks = r'''
DeletedMessage row(ID id, ID user = 1, ID dialog = 1) {
    auto result = DeletedMessage();
    result.messageId = int(id);
    result.userId = user;
    result.dialogId = dialog;
    result.text = "original";
    return result;
}
int main() {
    {
        LEGACY
        legacy.sync_schema(true);
        legacy.insert(row(10));
        legacy.insert(row(10));
    }
    storage.open_forever();
    storage.sync_schema(true);
    auto old = AyuDatabase::getDeletedMessagesSlice(1, 1, 0, 100, 100, false);
    assert(old.size() == 1 && old[0].text == "original" && old[0].snapshot.empty());
    auto replacement = row(10);
    replacement.text = "updated";
    replacement.snapshot = { 'a', 'b' };
    assert(AyuDatabase::addDeletedMessage(replacement));
    auto found = AyuDatabase::getDeletedMessagesSlice(1, 1, 0, 100, 100, false);
    assert(found.size() == 1 && found[0].text == "updated");
    assert(found[0].snapshot == replacement.snapshot);
    assert(storage.count<DeletedMessage>() == 1);
    {
        REOPENED
        const auto persisted = reopened.get_all<DeletedMessage>();
        assert(persisted.size() == 1 && persisted[0].snapshot == replacement.snapshot);
    }
    assert(AyuDatabase::addDeletedMessage(row(10, 2, 1)));
    assert(AyuDatabase::addDeletedMessage(row(10, 1, 2)));
    assert(AyuDatabase::getDeletedMessagesSlice(1, 1, 10, 100, 100, false).empty());
    assert(AyuDatabase::getDeletedMessagesSlice(1, 1, 0, 10, 100, false).empty());
    sqlite3 *connection = nullptr;
    assert(sqlite3_open("archive.db", &connection) == SQLITE_OK);
    assert(sqlite3_exec(connection, "CREATE TRIGGER fail_save BEFORE INSERT ON DeletedMessage "
        "WHEN NEW.text = 'fail' BEGIN SELECT RAISE(ABORT, 'test failure'); END", nullptr, nullptr, nullptr) == SQLITE_OK);
    auto failed = replacement;
    failed.text = "fail";
    assert(!AyuDatabase::addDeletedMessage(failed));
    found = AyuDatabase::getDeletedMessagesSlice(1, 1, 0, 100, 100, false);
    assert(found.size() == 1 && found[0].text == "updated");
    sqlite3_close(connection);
    for (const auto newer : { false, true }) {
        for (const auto onlyDeleted : { false, true }) {
            storage.remove_all<DeletedMessage>();
            auto live = std::vector<ID>();
            for (auto id = ID(1); id <= 2000; ++id) {
                if (onlyDeleted || id % 17 != 0) {
                    assert(AyuDatabase::addDeletedMessage(row(id)));
                } else {
                    live.push_back(id);
                }
            }
            auto history = History();
            history.bottom = !newer;
            if (!live.empty()) history.last = history.make(live.back(), false);
            auto visited = std::set<ID>();
            for (auto page = 0; page < 100; ++page) {
                auto source = QVector<MTPMessage>();
                const auto edge = newer ? history.high.bare : history.low.bare;
                for (const auto id : live) {
                    if (newer ? id > edge : (!edge || id < edge)) source.push_back({ id });
                }
                if (source.size() > 10) {
                    if (newer) source.resize(10);
                    else source.erase(source.begin(), source.end() - 10);
                }
                auto result = restoreDeletedMessages(&history, source, newer);
                assert(result.size() <= 110);
                if (result.empty()) break;
                auto previous = ID(0);
                for (const auto item : result) {
                    assert(item->id.bare > previous);
                    assert(visited.insert(item->id.bare).second);
                    previous = item->id.bare;
                }
                history.low = result.front()->id;
                history.high = result.back()->id;
            }
            assert(visited.size() == 2000);
        }
    }
    storage.remove_all<DeletedMessage>();
    assert(AyuDatabase::addDeletedMessage(row(50)));
    auto history = History();
    history.bottom = true;
    const auto overlap = restoreDeletedMessages(&history, {{ 50 }}, false);
    assert(overlap.size() == 1 && overlap.front()->id.bare == 50);
    storage.remove_all<DeletedMessage>();
    assert(AyuDatabase::addDeletedMessage(row(1)));
    assert(AyuDatabase::addDeletedMessage(row(50)));
    assert(AyuDatabase::addDeletedMessage(row(100)));
    auto around = History();
    const auto middle = restoreDeletedMessages(&around, {{ 40 }, { 60 }}, false);
    assert(middle.size() == 3);
    assert(middle.front()->id.bare == 40 && middle.back()->id.bare == 60);
    auto cache = History();
    cache.delegate.active = true;
    cache.make(1, true);
    cache.unloadDeletedMessagesLater();
    cache.unloadDeletedMessagesLater();
    assert(crl::pending.size() == 1);
    crl::drain();
    assert(cache._items.size() == 1 && cache.clears == 0);
    cache.delegate.active = false;
    cache.make(2, false);
    const auto visible = cache.make(3, true);
    visible->visible = true;
    auto document = DocumentData();
    auto media = FakeMedia{ &document };
    const auto playing = cache.make(4, true);
    playing->attachment = &media;
    Media::Player::instance()->song = { &document, playing->fullId() };
    cache.unloadDeletedMessagesLater();
    crl::drain();
    assert(cache.destroyed == std::vector<ID>{ 1 });
    assert(cache.message(cache.peer, 2) && cache.message(cache.peer, 3));
    assert(cache.message(cache.peer, 4));
    Media::Player::instance()->song = {};
    document.busy = true;
    cache.unloadDeletedMessagesLater();
    crl::drain();
    assert(cache.destroyed.size() == 1);
    visible->visible = false;
    document.busy = false;
    cache.unloadDeletedMessagesLater();
    crl::drain();
    assert(cache.destroyed == (std::vector<ID>{ 1, 3, 4 }));
    assert(cache._items.size() == 1 && !cache._items.front()->saved);
    cache.make(5, true);
    cache.unloadDeletedMessagesLater();
    cache.sessionData.alive = false;
    crl::drain();
    assert(cache.message(cache.peer, 5));
    cache.sessionData.alive = true;
    cache._flags = 0;
    cache.unloadDeletedMessagesLater();
    Core::quitting = true;
    crl::drain();
    assert(cache.message(cache.peer, 5));
    Core::quitting = false;
    auto filteredHistory = History();
    const auto filtered = filteredHistory.make(50, true);
    FiltersCacheController::filteredMessages[1][50] = false;
    FiltersCacheController::filteredMessages[2][50] = true;
    FiltersCacheController::invalidateSingle(filtered);
    assert(!FiltersCacheController::filteredMessages.contains(1));
    assert(FiltersCacheController::filteredMessages[2][50]);
    FiltersCacheController::invalidateSingle(filtered);
    std::cout << "PASS: schema upgrade, deduplication, snapshots, account isolation, "
        "reopen, strict bounds, rollback, overlap, jump range, "
        "complete bounded pagination, coalesced cleanup, active/visible/playing "
        "protection, session lifetime, and filter cache cleanup\n";
}
'''.replace("LEGACY", legacy).replace(
            "REOPENED", storage.replace("auto storage =", "auto reopened =")
        )
        code = prefix + entities + storage
        code += "namespace AyuDatabase {\n" + queries + "}\n"
        code += "namespace ArchiveTest {\n" + mocks
        code += "namespace AyuMessages {\n" + eviction + "}\n"
        code += "namespace FiltersCacheController {\n"
        code += "std::unordered_map<ID, std::unordered_map<ID, bool>> filteredMessages;\n"
        code += invalidation + "}\n"
        code += pagination + unloading + "}\n"
        code += "using namespace ArchiveTest;\n" + checks
        with tempfile.TemporaryDirectory(prefix="ayu-archive-test-") as directory:
            path = Path(directory)
            (path / "test.cpp").write_text(code)
            subprocess.run(
                [os.environ.get("CXX", "c++"), "-std=c++20", "-O0", "-w",
                 "-I", str(ROOT / "Telegram/SourceFiles"),
                 str(path / "test.cpp"), "-lsqlite3", "-o", str(path / "test")],
                check=True, cwd=path,
            )
            subprocess.run([str(path / "test")], check=True, cwd=path)


if __name__ == "__main__":
    unittest.main()
