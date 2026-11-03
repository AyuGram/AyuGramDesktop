## Build instructions for macOS

### Prepare folder

Choose a folder for the future build, for example **/Users/user/TBuild**. It will be named ***BuildPath*** in the rest of this document. All commands will be launched from Terminal.

**Note about disk space:** The full build process will require approximately **55 GB** of free space. This includes:
- **~35 GB** for libraries (when building for both x64 and arm64 architectures)
- **~20 GB** for the compiled Telegram app (in the `out` folder)

### Clone source code and prepare libraries

Go to ***BuildPath*** and run

    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    brew install git automake libtool cmake wget pkg-config gnu-tar ninja nasm meson

    sudo xcode-select -s /Applications/Xcode.app/Contents/Developer

    git clone --recursive https://github.com/AyuGram/AyuGramDesktop.git tdesktop
    ./tdesktop/Telegram/build/prepare/mac.sh

### Building the project

Go to ***BuildPath*/tdesktop/Telegram** and run

    ./configure.sh -D TDESKTOP_API_ID=2040 -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627

Then launch Xcode, open ***BuildPath*/tdesktop/out/Telegram.xcodeproj** and build for Debug / Release.

### Repeated microphone or camera permission prompts

The official 7.2.9 macOS package has no complete application bundle signature:
`codesign --verify --deep --strict --all-architectures /Applications/AyuGram.app`
fails with `code object is not signed at all`. The arm64 executable's linker
signature alone does not bind `Info.plist` or seal the bundle resources. macOS
TCC can repeatedly report `Failed to match existing code requirement` for
`one.ayugram.AyuGramDesktop` when recording voice or video messages. This is
also reported in [issue #273](https://github.com/AyuGram/AyuGramDesktop/issues/273)
and [issue #464](https://github.com/AyuGram/AyuGramDesktop/issues/464).

For an existing installation from the official release, quit AyuGram with
**Command-Q**, keep a backup of `AyuGram.app`, then apply a local ad hoc bundle
signature:

```bash
codesign --force --sign - \
    --identifier one.ayugram.AyuGramDesktop /Applications/AyuGram.app
codesign --verify --deep --strict --all-architectures /Applications/AyuGram.app
```

Sign only the outer app bundle, preserving the existing Apple signatures on
the bundled Swift libraries. `--deep` is used for verification, not signing.

Reopen AyuGram, grant microphone/camera access when requested, then check
several recordings and another launch. Local signing does not provide an
Apple Developer ID or notarization; repeat the repair after installing an
unsigned update. A distributable release should be signed with the
maintainer's Developer ID, notarized, and verified after its final packaging
steps. Do not modify bundle contents after signing.

The permission wrapper also needs to return after registering the asynchronous
AVFoundation request. Otherwise it immediately reports `Granted` and later
calls the continuation again with the system's actual answer. This affects
calls and the microphone test in settings; voice-message capture uses a
separate path, so that source fix alone does not repair the unsigned package.

Run the isolated callback regression tests on macOS with Xcode Command Line
Tools installed:

```bash
python3 tools/tests/test_mac_permissions.py
```

They compile only a small Objective-C++ harness around the actual permission
function, without building AyuGram or requesting device access.
