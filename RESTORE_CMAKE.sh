#!/usr/bin/env bash
# Restore full Telegram/CMakeLists.txt from last good commit and rebrand to Drakeygram.
# Run from repository root.
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT"

echo "==> Fetching full CMakeLists from commit 0fed6eb3..."
curl -fsSL 'https://raw.githubusercontent.com/sqdq2/Drakeygram/0fed6eb3d9b21739de559286ecc988939a2f21ee/Telegram/CMakeLists.txt' -o Telegram/CMakeLists.txt

echo "==> Applying Drakeygram rebrand..."
python3 - <<'PY'
from pathlib import Path
p = Path("Telegram/CMakeLists.txt")
t = p.read_text(encoding="utf-8")
reps = [
    ("# This file is part of Telegram Desktop,\n# the official desktop application for the Telegram messaging service.",
     "# This file is part of Drakeygram Desktop,\n# an independent Telegram Desktop fork."),
    ("# AyuGram filters", "# Drakeygram filters"),
    ("set(ayugram_files", "set(drakeygram_files"),
    ("${ayugram_files}", "${drakeygram_files}"),
    ("ayugram::lib_icu", "drakeygram::lib_icu"),
    ("set(ayugram_icons", "set(drakeygram_icons"),
    ("${ayugram_icons}", "${drakeygram_icons}"),
    ("one.ayugram.AyuGramDesktop", "one.drakeygram.DrakeygramDesktop"),
    ('set(output_name "AyuGram")', 'set(output_name "Drakeygram")'),
    ('OR NOT "${output_name}" STREQUAL "AyuGram")', 'OR NOT "${output_name}" STREQUAL "Drakeygram")'),
    ("# AyuGram: do not use __declspec(dllimport) when including icu headers",
     "# Drakeygram: do not use __declspec(dllimport) when including icu headers"),
    ("com.ayugram.desktop", "com.drakeygram.desktop"),
]
for a, b in reps:
    t = t.replace(a, b)
p.write_text(t, encoding="utf-8")
print("OK")
print("  ayugram_files remaining:", "ayugram_files" in t)
print("  AyuGram remaining:", "AyuGram" in t)
print("  one.ayugram remaining:", "one.ayugram" in t)
print("  lines:", t.count(chr(10))+1)
PY

chmod +x "$0" 2>/dev/null || true
echo "==> Done. Review with: git diff Telegram/CMakeLists.txt"
echo "==> Then: git add Telegram/CMakeLists.txt && git commit -m 'fix: restore full CMakeLists with Drakeygram rebrand'"
