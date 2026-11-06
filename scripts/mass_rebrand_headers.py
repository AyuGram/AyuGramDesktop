#!/usr/bin/env python3
"""Mass-replace AyuGram copyright headers under Telegram/SourceFiles/ayu/"""
from pathlib import Path
import sys

OLD = """// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
"""

NEW = """// Drakeygram Desktop
// Fully independent fork. No external tracking.
"""

def main():
    root = Path("Telegram/SourceFiles/ayu")
    if not root.is_dir():
        print("Run from repo root (Telegram/SourceFiles/ayu not found)", file=sys.stderr)
        sys.exit(1)
    n = 0
    for path in root.rglob("*"):
        if path.suffix not in {".cpp", ".h", ".hpp", ".mm", ".c"}:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except Exception:
            continue
        if OLD not in text:
            continue
        path.write_text(text.replace(OLD, NEW, 1), encoding="utf-8", newline="\n")
        print(f"cleaned: {path}")
        n += 1
    print(f"Done. Cleaned {n} files.")

if __name__ == "__main__":
    main()
