# Agent Guide for Drakeygram Desktop

This guide defines repository-wide instructions for coding agents working with the Drakeygram Desktop (Telegram Desktop fork) codebase.

Avoid building the project.

If you're asked to create a Pull Request, then clearly state in PR description that it was AI generated.

## Important Note

This is a fully independent fork. All remote tracking, AyuGram network endpoints, and external language pack fetches have been disabled. Do not re-introduce any connections to ayugram.one, exteragram.app, or related services.

## Build System Structure

The build system expects this directory layout:

```text
L:\Telegram\                    # BuildPath
L:\Telegram\tdesktop\           # Repository (you work here)
L:\Telegram\Libraries\          # 32-bit dependencies (Linux/macOS)
L:\Telegram\win64\Libraries\    # 64-bit dependencies (Windows)
L:\Telegram\ThirdParty\         # Build tools (NuGet, Python, etc.)
```

Dependencies are located relative to the repository: `../Libraries`, `../win64/Libraries`, or `../ThirdParty`.

## Build Configuration

### Build Commands

**From repository root, run:**

```bash
cmake --build out --config Debug --target Telegram
```

That's it. The `out/` directory is already configured. The executable will be at `out/Debug/Telegram.exe`.

**Never build Release** - it's extremely heavy and not needed for testing changes.

## Key Files

- **`Telegram/build/version`** - Version information
- **`out/`** - Build output directory

## Best Practices

1. **Always use Debug builds** - Release builds are extremely heavy
2. **Don't build Release configuration** - it's too heavy for testing
3. **Do not re-add any AyuGram network endpoints or tracking**

## Commits

- Subject: one concise, plain-language line summarizing the change, ~50-60 characters.
- Never add a `Co-Authored-By:` line or any tool/assistant attribution trailer.
