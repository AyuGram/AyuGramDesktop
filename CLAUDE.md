# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

AyuGramDesktop is a fork of Telegram Desktop that adds features such as ghost mode, message history tracking (anti-recall), streamer mode, local Telegram Premium features, a built-in translator, and enhanced appearance options.

## Build System

The project uses CMake (3.25+) with Ninja Multi-Config generator. Builds happen inside a Docker container using a CentOS environment image for Linux.

### Linux (Docker)

```bash
# Release build
docker run --rm -it \
    -u $(id -u) \
    -v "$PWD:/usr/src/tdesktop" \
    ghcr.io/telegramdesktop/tdesktop/centos_env:latest \
    /usr/src/tdesktop/Telegram/build/docker/centos_env/build.sh \
    -D TDESKTOP_API_ID=2040 \
    -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627

# Debug build (add -e CONFIG=Debug)
docker run --rm -it \
    -u $(id -u) \
    -v "$PWD:/usr/src/tdesktop" \
    -e CONFIG=Debug \
    ghcr.io/telegramdesktop/tdesktop/centos_env:latest \
    /usr/src/tdesktop/Telegram/build/docker/centos_env/build.sh \
    -D TDESKTOP_API_ID=2040 \
    -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627
```

Build output goes into the `out/` directory.

### VS Code Dev Container

Add to `.vscode/settings.json`:
```json
{
    "cmake.configureSettings": {
        "TDESKTOP_API_ID": "YOUR_API_ID",
        "TDESKTOP_API_HASH": "YOUR_API_HASH"
    }
}
```

Then reopen in container using the Dev Containers extension.

## Code Architecture

### Directory Structure

- `Telegram/SourceFiles/` — all C++ source code
  - `ayu/` — AyuGram-specific features (see below)
  - `data/` — data models and caching layer
  - `history/` — message history and rendering
  - `api/` — Telegram API wrappers
  - `mtproto/` — MTProto protocol implementation
  - `ui/` — UI framework components
  - `calls/` — voice/video call support
  - `window/` — window/session controller
- `Telegram/Resources/langs/lang.strings` — base English localization strings
- `Telegram/SourceFiles/mtproto/scheme/api.tl` — Telegram API schema (TL language)
- `Telegram/SourceFiles/mtproto/scheme/mtproto.tl` — MTProto protocol schema

### AyuGram-Specific Code (`ayu/`)

- `ayu_settings.h/cpp` — AyuGram settings storage and access
- `ayu_state.h/cpp` — runtime state management
- `ayu_infra.h/cpp` — infrastructure/initialization
- `data/` — `ayu_database`, `messages_storage` (deleted message history storage)
- `features/` — feature implementations: `filters/`, `forward/`, `message_shot/`, `streamer_mode/`, `translator/`
- `ui/` — AyuGram UI components, styles, settings panels, message history panel

### Modular Libraries (git submodules)

The project is split into independently versioned libraries:
- `lib_rpl` — reactive programming library (`rpl::` namespace)
- `lib_base` — base utilities
- `lib_ui` — UI framework
- `lib_tl` — TL schema code generation
- `lib_lottie`, `lib_qr`, `lib_spellcheck`, `lib_storage`, `lib_webrtc`, `lib_webview`, `lib_icu`

### Code Generation

Several subsystems use code generators that run during the build:
- **Localization:** `lang.strings` → C++ `tr::lng_*` accessors
- **API types:** `api.tl` → `MTP*` C++ classes in `MTP::` namespace
- **Styles:** `.style` files → C++ objects in `st::` namespace and structs in `style::` namespace

## Coding Patterns

### Style: Always use `auto`

```cpp
// Correct
auto title = tr::lng_settings_title(tr::now);
const auto &peer = message->peer();

// Avoid
QString title = tr::lng_settings_title(tr::now);
```

### Localization (`tr::`)

Strings are defined in `Telegram/Resources/langs/lang.strings` and accessed via generated `tr::lng_*` functions:

```cpp
// Immediate value
auto text = tr::lng_settings_title(tr::now);

// Reactive producer (updates on language change)
auto producer = tr::lng_settings_title();

// With placeholder
auto msg = tr::lng_confirm_delete_item(tr::now, lt_item_name, itemName);

// Plural
auto filesText = tr::lng_files_selected(tr::now, lt_count, count);
// Reactive plural: use | tr::to_count() to convert rpl::producer<int>
```

### UI Styles (`st::`)

Styles are defined in `.style` files alongside source code. Named colors are in `ui/colors.palette`.

```cpp
#include "styles/style_widgets.h"  // prefix is always "style_"

int height = st::myButton.height;
const style::icon &icon = st::myButton.icon;
p.fillRect(rect(), st::windowBg);
```

Icon paths in `.style` files:
```style
myIcon: icon{{ "gui/icons/search", iconColor }};
```

### Reactive Programming (`rpl::`)

```cpp
// Transform a stream
auto doubled = std::move(intProducer) | rpl::map([](int v) { return v * 2; });

// Subscribe, managed by lifetime
std::move(producer) | rpl::on_next([=](int v) {
    // handle value
}, lifetime);

// Combine multiple producers (lambda args are auto-unpacked from tuple)
rpl::combine(countProducer, textProducer) | rpl::on_next([=](int count, const QString &text) {
    // ...
}, lifetime);
```

Always `std::move` producers when starting a pipeline. Use `rpl::duplicate` if the producer needs to be reused.

### Telegram API Requests

Refer to `Telegram/SourceFiles/mtproto/scheme/api.tl` for method names and parameter types.

```cpp
api().request(MTPmessages_GetHistory(
    MTP_flags(flags),
    MTP_inputPeer(peer),
    // ...
)).done([=](const MTPmessages_Messages &result) {
    result.match([&](const MTPDmessages_messages &data) {
        // handle data.vmessages().v
    });
}).fail([=](const MTP::Error &error) {
    // handle error.type()
}).handleFloodErrors().send();
```

## Code Style

Formatting is enforced via `.clang-format` (LLVM-based):
- **Indentation:** tabs (width 4)
- **Column limit:** 120 characters
- Short `if`/`for`/`case` statements on a single line are allowed
- Brace wrapping: custom (braces after class/struct/enum/union/extern block)

Run clang-format before submitting changes. The dev container includes clangd integration.

## No Automated Tests

There is no test framework in this repository. Verification is done by building successfully and manually testing the application.
