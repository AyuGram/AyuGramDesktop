<div align="center">

# Drakeygram

**Independent Telegram Desktop client — privacy, control, zero third-party tracking**

[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-blue)](https://github.com/sqdq2/Drakeygram)
[![License](https://img.shields.io/badge/license-GPL--3.0-green)](https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL)
[![Status](https://img.shields.io/badge/status-independent%20fork-success)](https://github.com/sqdq2/Drakeygram)

*Built on Telegram Desktop. Fully independent. No external telemetry.*

</div>

---

## Why Drakeygram?

Drakeygram is a **self-contained** Telegram Desktop fork focused on privacy and power-user features.  
It does **not** phone home to any AyuGram / third-party config servers. All remote config, external language CDNs, and donate tracking endpoints have been removed or disabled.

| | |
|---|---|
| **Identity** | Drakeygram — your client, your name |
| **Network** | Telegram servers only (plus optional public translators you choose) |
| **Trackers** | None to former upstream fork services |
| **Code** | Open source on this repository |

---

## Features

### Privacy & Ghost Mode
- **Ghost Mode** — control read receipts, online status, typing, upload progress, story views
- Per-account or global ghost profiles
- Optional mark-as-read after action / scheduled send helpers
- Silent send defaults

### Message control
- **Anti-recall** — keep deleted messages locally
- **Message history / revisions** — edited message history
- Local filters (regex) for content control
- Enhanced context menus and message tools

### Appearance & UX
- Font customization
- Streamer mode (platform-aware)
- Local Telegram Premium visuals (client-side)
- Custom icons / appearance options
- Message shot (screenshot) tools
- Media preview enhancements

### Productivity
- Built-in **translator** (Google / Yandex style providers — optional, user-initiated)
- Smart forward helpers
- Filters with import/export
- macOS force-click style interactions where supported

> Feature set inherits the hardened desktop fork lineage and keeps local-first behavior. Nothing requires AyuGram accounts or servers.

---

## Independence guarantee

This repository has been hard-cleaned for independence:

- Remote config manager (`RCManager`) — **no network fetches**
- External language pack CDN — **disabled**
- Donate / supporter remote lists — **empty / local only**
- Branding, desktop IDs, and binary name — **Drakeygram**
- Docs and settings links — **no AyuGram domains**

If you fork this tree further, keep those guarantees.

---

## Building

Drakeygram follows the same build system as [Telegram Desktop](https://github.com/telegramdesktop/tdesktop).

1. Install the official **tdesktop** dependencies for your OS  
   → https://github.com/telegramdesktop/tdesktop/blob/dev/docs/building-linux.md  
   → https://github.com/telegramdesktop/tdesktop/blob/dev/docs/building-mac.md  
   → https://github.com/telegramdesktop/tdesktop/blob/dev/docs/building-win-x64.md

2. Clone with submodules:

```bash
git clone --recursive https://github.com/sqdq2/Drakeygram.git
cd Drakeygram
```

3. Configure and build with CMake (same flags you use for tdesktop).

The binary output name is **Drakeygram**. Linux desktop ID: `com.drakeygram.desktop`.

---

## Repository layout (relevant bits)

```text
Telegram/SourceFiles/ayu/     # Privacy & power features (local)
lib/xdg/                      # Drakeygram desktop integration
RESTORE_CMAKE.sh              # Emergency CMakeLists recovery helper
scripts/mass_rebrand_headers.py
```

Internal folder name `ayu/` is historical pathing only — **not** a network dependency.

---

## Credits

- [Telegram Desktop](https://github.com/telegramdesktop/tdesktop) — base client  
- Community privacy / customization work that this fork continues independently  

Drakeygram is **not** affiliated with Telegram FZ-LLC or any other third-party client brand.

---

## License

GPL-3.0 (same family as Telegram Desktop). See upstream `LEGAL` and project license files.

---

<div align="center">

**Drakeygram** — independent, private, yours.

</div>
