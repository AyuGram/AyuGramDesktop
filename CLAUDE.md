# Claude Code Pointer

Read `AGENTS.md` and treat it as the canonical repository-wide instructions.

---

# Work Log

This file documents features that were added to AyuGram Desktop via Claude Code sessions, the design decisions behind them, and the file-level changes. It is a high-level companion to `git log` — read both for context.

## 1. "Remove Media" — selective sweep of a chat's media

A new entry **Remove Media** appears in the 3-dots chat menu (and in the chat-list right-click menu) directly under **Clear History**. It opens a box that lets the user delete every message of a chosen media type from a chat, with live progress.

### UX

1. **Selection phase** — six checkboxes, all unchecked by default:
   - Pictures
   - Videos
   - Voice messages
   - Video messages (round videos)
   - GIFs
   - Stickers
2. **Revoke toggle** (visible only where it has a real effect):
   - 1-on-1 user chat (not Saved Messages) → "Remove for Me and {Name}"
   - Basic legacy group → "Remove for everyone"
   - Saved Messages → toggle hidden (operation always deletes for me, since both sides are me)
   - Megagroup / broadcast channel → toggle hidden (`channels.deleteMessages` has no revoke flag — the server always deletes for all members when permission allows)
3. **Remove** button (red attention style). Clicking with nothing selected shows a toast; otherwise the selection collapses and the progress UI appears in its place.
4. **Searching phase** — label reads "Searching… N found" with N counting up live as each batch is processed.
5. **Deleting phase** — label switches to "X / Y" and a horizontal progress bar fills from 0 to 1 as batches succeed/fail.
6. **Done** — bar at 100%, a toast ("Removed N messages." or "No matching media found."), and the box auto-closes after ~1.2 s.

### Implementation notes

- **Search**: `MTPmessages_Search` paginated in batches of 100 with `offset_id = minIdSeen - 1`. One search stream per checked filter; their result sets are deduplicated via `base::flat_set<MsgId>` before deletion.
- **Sticker filter quirk**: Telegram's API has no `inputMessagesFilterStickers`, and the server treats stickers as a separate class from "files" — `inputMessagesFilterDocument` returns zero stickers. The only correct path is to scan the full chat history with `inputMessagesFilterEmpty` and pick out documents whose attributes include `documentAttributeSticker` on the client. This is documented in a comment in `remove_media_box.cpp` next to the addType call.
- **Delete**: batched in groups of 100 with a 500–1000 ms jittered delay between batches. For channels (megagroups + broadcasts) the call is `MTPchannels_DeleteMessages`; for everything else it's `MTPmessages_DeleteMessages` with the `f_revoke` flag set per the toggle.
- **Fail handling**: on any per-batch API failure (permission denied, network blip, etc.) the operation logs and **skips** that batch rather than retrying. Without this, channels where the user lacks delete-others permission would loop forever on the same batch.
- **Lifetime**: the operation captures a `shared_ptr<State>` so it continues even if the user closes the box mid-run. The final toast fires unconditionally; the box auto-close fires only if the box is still alive.
- **Progress bar widget**: a small inline `Ui::RpWidget` (~25 lines) that paints a rounded background with `windowBgRipple` and a fill rect with `windowBgActive`. No fancy animation.

### Files

- `Telegram/SourceFiles/ayu/ui/boxes/remove_media_box.h` — public entry point.
- `Telegram/SourceFiles/ayu/ui/boxes/remove_media_box.cpp` — box content, the `ProgressBar` widget, and `SearchAndDelete` + `RunDelete` helpers.

## 2. "Delete all my messages" — sweep your own messages from a group/channel

A new entry **Delete all my messages** appears in the 3-dots menu for basic groups, megagroups, and broadcast channels the user is a member of. It deletes every message the user has sent in that chat, with live progress.

This **replaces** the older silent `Delete own messages` action that previously existed in `context_menu.cpp` and was restricted to non-admin members of groups. The new version:

- Has the same intent but a clearer name.
- Works for **all roles** (member, admin, creator).
- Works in **broadcast channels** (the old version was groups-only).
- Has a confirmation step and a progress UI rather than running silently.
- Uses the red attention icon (was the neutral TTL icon).

### UX

1. **Confirm phase** — title "Delete all my messages", body text "Delete every message you've sent in this chat? This cannot be undone.", buttons Delete / Cancel.
2. **Progress phases** — identical to Remove Media: searching count → "X / Y" with bar → done toast → auto-close.

### Implementation notes

- **Search**: `MTPmessages_Search` with `Flag::f_from_id` set and `MTP_inputPeerSelf()` — i.e. messages in this peer from the current user only.
- **Delete**: same batched logic as Remove Media. Channel path uses `MTPchannels_DeleteMessages`; non-channel path uses `MTPmessages_DeleteMessages` with `f_revoke`.
- **Fail handling**: skip-on-fail, same as Remove Media (fixes the same retry-forever bug that the old silent helper had).
- **Hidden in**: user chats, Saved Messages, forum topics. (The forum-topic exclusion matches the old behaviour — topic-scoped sweep is not implemented yet; see Limitations.)

### Files

- `Telegram/SourceFiles/ayu/ui/boxes/delete_my_messages_box.h` — public entry point.
- `Telegram/SourceFiles/ayu/ui/boxes/delete_my_messages_box.cpp` — box content + `SearchOwn` + `RunDelete` helpers.

## Shared cross-cutting changes

### `Telegram/SourceFiles/ayu/ui/context_menu/context_menu.{h,cpp}`

- **Added** `AddRemoveMediaAction(peer, controller, addCallback)` — only inserts the menu item when peer is a user, basic chat, or channel (megagroup/broadcast). Uses `clearAttention` icon, `isAttention: true`.
- **Removed** the inline `DeleteMyMessagesAfterConfirm(peer)` helper (~110 lines). Its search + delete logic now lives in `delete_my_messages_box.cpp` so the box can drive progress reactively.
- **Simplified** `DeleteMyMessagesHandler` to a one-liner that opens `FillDeleteMyMessagesBox` via `controller->show(Box(...))`.
- **Broadened** `AddDeleteOwnMessagesAction` visibility: previously required `!chat->amCreator() && !chat->hasAdminRights()` and was megagroup-only on the channel branch. Now: any `peer->isChat()` or `peer->isChannel()` where `amIn()` is true. Switched icon to `clearAttention`, `isAttention: true`.

### `Telegram/SourceFiles/window/window_peer_menu.cpp`

Inserted `AyuUi::AddRemoveMediaAction(_peer, _controller, _addAction)` right after `addClearHistory()` in two places:

- `Filler::fillChatsListActions()` (~line 1761) — the chat-list right-click menu.
- `Filler::fillHistoryActions()` (~line 1789) — the in-chat 3-dots menu.

`AddDeleteOwnMessagesAction` is still called from those same sites (unchanged callers, broadened callee).

### `Telegram/Resources/langs/lang.strings`

#### Updated

- `ayu_DeleteOwnMessages`: `"Delete own messages"` → `"Delete all my messages"`.
- `ayu_DeleteOwnMessagesConfirmation`: was group-specific, now reads `"Delete every message you've sent in this chat? This cannot be undone."`.

#### Added (Remove Media)

`ayu_RemoveMediaMenu`, `ayu_RemoveMediaTitle`, `ayu_RemoveMediaPhotos`, `ayu_RemoveMediaVideos`, `ayu_RemoveMediaVoice`, `ayu_RemoveMediaVideoMessages`, `ayu_RemoveMediaGifs`, `ayu_RemoveMediaStickers`, `ayu_RemoveMediaRevoke` (with `{user}` placeholder), `ayu_RemoveMediaRevokeSelf`, `ayu_RemoveMediaRevokeGroup`, `ayu_RemoveMediaButton`, `ayu_RemoveMediaNothingSelected`, `ayu_RemoveMediaSearching` (plural, with `{count}`), `ayu_RemoveMediaDeleting`, `ayu_RemoveMediaDone` (plural, with `{count}`), `ayu_RemoveMediaNoneFound`.

#### Added (Delete All My Messages)

`ayu_DeleteOwnMessagesSearching` (plural), `ayu_DeleteOwnMessagesDeleting`, `ayu_DeleteOwnMessagesDone` (plural), `ayu_DeleteOwnMessagesNone`.

### `Telegram/CMakeLists.txt`

Registered the four new files in the AyuGram boxes block:

```
ayu/ui/boxes/remove_media_box.cpp
ayu/ui/boxes/remove_media_box.h
ayu/ui/boxes/delete_my_messages_box.cpp
ayu/ui/boxes/delete_my_messages_box.h
```

## Build

Linux Docker build, as documented in `docs/building-linux.md`:

```
docker run --rm \
    -u $(id -u):$(id -g) \
    -v "$PWD:/usr/src/tdesktop" \
    ghcr.io/telegramdesktop/tdesktop/centos_env:latest \
    /usr/src/tdesktop/Telegram/build/docker/centos_env/build.sh \
    -D TDESKTOP_API_ID=2040 \
    -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627
```

- Clean build (2304 targets) was ~28 min on this machine.
- Incremental rebuild after a `.cpp` change only: ~30 s–2 min (mostly the relink).
- Incremental rebuild after a `lang.strings` change: ~5–10 min — `lang_auto.cpp` is regenerated and recompiled, and it's a widely-included header.

Output binary: `out/Release/AyuGram` (~410 MB, not stripped).

## Limitations / future work

- **Forum topics aren't scoped.** Opening either menu inside a forum topic operates on the entire forum, not just the current topic. To fix: pass the topic through to the box and set `top_msg_id` in `messages.search`, plus restrict the deletion path the same way.
- **Channel non-admins.** If a user opens "Remove Media" in a channel they're a subscriber to (no admin rights), the search succeeds but most batch deletions are silently rejected by the server. We currently still say `"Removed N messages."` using our local progress counter, because `messages.AffectedMessages` doesn't report a per-id success count. Could be improved by pre-checking `canDeleteMessages` and either hiding the entry or showing a warning.
- **Code duplication.** `remove_media_box.cpp` and `delete_my_messages_box.cpp` share ~80 lines of progress UI scaffolding (the `ProgressBar` widget, the `State` struct, the merged-rpl label text, the auto-close on Done). If a third caller is added, factor this into a shared header.
- **Stickers cost.** Because Telegram has no sticker filter, choosing Stickers in Remove Media walks the entire chat history with the empty filter. For very large chats this is the slowest checkbox by far — every other type uses a server-side filter.
