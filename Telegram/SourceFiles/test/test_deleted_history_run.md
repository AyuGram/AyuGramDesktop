# Disposable inline-restoration GUI test

Run this recipe on a disposable profile. The Debug executable must include `test_deleted_history_rpc.cpp/.h` and the one RPC
observation seam. Temporarily copy `test_deleted_history_scenario.cpp.copy`
onto `test_scenario.cpp` before building Debug, then restore the repository's
no-op scenario and rebuild before using the client normally. Never run the
fixture on a production profile.

The fixture needs two processes because the second must reopen the same
SQLite rows after all first-process RAM caches have gone. Both processes
recreate only synthetic session/peer objects. Phase two never seeds rows.

Set `AYU_GUI_EXE` to the actual newly built Debug executable. Then prepare a
fresh directory once:

```sh
AYU_GUI_EXE=/absolute/path/to/Debug/AyuGram.app/Contents/MacOS/AyuGram
AYU_GUI_TESTS=/absolute/path/to/tdesktop/Telegram/SourceFiles/test
AYU_GUI_ROOT=$(mktemp -d /private/tmp/ayu-deleted-gui.XXXXXX)
chmod 700 "$AYU_GUI_ROOT"
mkdir "$AYU_GUI_ROOT/tdata"
touch "$AYU_GUI_ROOT/testing" "$AYU_GUI_ROOT/ayu-deleted-history-fixture"
```

Launch phase one directly, not through `open`, so the sandbox and environment
apply from process startup:

```sh
env AYU_DELETED_HISTORY_WORKDIR="$AYU_GUI_ROOT" \
  AYU_DELETED_HISTORY_PHASE=1 AYU_DELETED_HISTORY_NO_NETWORK=1 \
  TDESKTOP_TEST_EVIDENCE_DIR="$AYU_GUI_ROOT/evidence-phase1" \
  TDESKTOP_TEST_WATCHDOG=300 \
  /usr/bin/sandbox-exec -f "$AYU_GUI_TESTS/test_deleted_history_no_network.sb" \
  "$AYU_GUI_EXE" -testagent -noupdate -workdir "$AYU_GUI_ROOT"
```

After it exits normally, require `phase1-complete` and the phase-one log's
`SCENARIO_RESULT: PASS` plus `TEST_COMPLETE`. Then use the identical command
with `AYU_DELETED_HISTORY_PHASE=2` and
`TDESKTOP_TEST_EVIDENCE_DIR="$AYU_GUI_ROOT/evidence-phase2"`. Require
`phase2-complete`, PASS, TEST_COMPLETE, zero failures, and inspect the saved
old-prefix, between-anchors, newer-tail and cloud-empty screenshots in both
evidence directories. The log records source-row SHA256 and actual request
IDs/parameters. Keep the fixture and evidence for review.

The network-policy environment marker is a safety prerequisite, not an
egress measurement. The `.sb` profile enforces the restriction: Unix-socket
IPC is allowed for Qt startup, while AF_INET TCP connections are denied.
If an outer sandbox refuses `sandbox_apply`, run the command in an environment
that permits applying this profile; do not remove the network restriction.

Scope: this tests the default legacy `HistoryWidget` renderer, explicitly
selected in the fixture. It does not prove the experimental new chat-view
renderer or Telegram's real server behavior. The cached-placeholder unread
check gives the unavailable service placeholder `ClientSideUnread` to exercise
the decrement branch, then restores and attaches synchronously before UI
read-marking can alter counts; the long mixed/cloud-empty checks consume real
`HistoryWidget` request callbacks and verify the exact visible paint owner.

## Qt-free regression checks on macOS

These standalone tests compile the actual production query/page headers.
They use only synthetic records; they do not launch the app or open its data.
Run the commands sequentially, with temporary executables outside `out/`:

```sh
AYU_TEST_ROOT=/absolute/path/to/tdesktop
AYU_TEST_SOURCES="$AYU_TEST_ROOT/Telegram/SourceFiles"
AYU_TEST_OUT=$(mktemp -d /private/tmp/ayu-history-tests.XXXXXX)

/usr/bin/clang++ -std=c++20 -I "$AYU_TEST_SOURCES" \
  "$AYU_TEST_SOURCES/test/test_history_page.cpp" \
  -o "$AYU_TEST_OUT/history_page"
"$AYU_TEST_OUT/history_page"

/usr/bin/clang++ -std=c++20 -Wno-deprecated-declarations \
  -Wno-deprecated-literal-operator -Wno-c++26-extensions \
  -I "$AYU_TEST_SOURCES" \
  "$AYU_TEST_SOURCES/test/test_deleted_history_query.cpp" \
  -lsqlite3 -o "$AYU_TEST_OUT/deleted_history_query"
"$AYU_TEST_OUT/deleted_history_query"
```

The query header includes the checked-in
`ayu/libs/sqlite/sqlite_orm.h` and its SQLite API header through the source
include root above; `-lsqlite3` links the macOS system runtime. The three
warning switches suppress existing vendor-header diagnostics on Apple Clang 21.
Expected output, with exit status 0 for each executable:

```text
History page tests passed: 475136 exhaustive cases plus boundary and local-only paging cases.
deleted history query and combined persistence tests passed
```

Optional ASan/UBSan run of the query and combined paging/persistence test:

```sh
/usr/bin/clang++ -std=c++20 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -Wno-deprecated-declarations \
  -Wno-deprecated-literal-operator -Wno-c++26-extensions \
  -I "$AYU_TEST_SOURCES" \
  "$AYU_TEST_SOURCES/test/test_deleted_history_query.cpp" \
  -lsqlite3 -o "$AYU_TEST_OUT/deleted_history_query_san"
"$AYU_TEST_OUT/deleted_history_query_san"
```

Require the same query-test success line and no sanitizer report. Passing
these checks establishes the query/page contracts, not GUI behavior.
