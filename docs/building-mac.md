# Build instructions for macOS

Use a recursive checkout of the repository and branch you intend to build.
Its parent directory holds `Libraries` and `ThirdParty`; keep application
profiles outside those build directories. Allow at least 55 GB for a cold
universal build.

Install Xcode and Homebrew first. CI uses Xcode 16.4 on `macos-15`; the `.icns`
fallback does not require Xcode 26's Icon Composer.

```bash
brew install git automake libtool cmake pkg-config ninja nasm meson ccache
```

Select the installed Xcode using `xcode-select` if necessary. Obtain your own
`TDESKTOP_API_ID` and `TDESKTOP_API_HASH` as described in
[Telegram's application registration instructions](https://core.telegram.org/api/obtaining_api_id)
and export them in the build shell. Do not commit credentials.

## Release preview

From the checkout root, build a universal package:

```bash
./Telegram/build/prepare/mac.sh silent qt-release-only
cd Telegram
./configure.sh -G Ninja \
  -D CMAKE_BUILD_TYPE=Release \
  -D CMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -D 'CMAKE_OSX_ARCHITECTURES=x86_64;arm64' \
  -D DESKTOP_APP_MAC_ARCH= \
  -D CMAKE_C_COMPILER_LAUNCHER=ccache \
  -D CMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -D "TDESKTOP_API_ID=$TDESKTOP_API_ID" \
  -D "TDESKTOP_API_HASH=$TDESKTOP_API_HASH" \
  -D DESKTOP_APP_DISABLE_AUTOUPDATE=ON
python3 build/check_macos_release.py ../out \
  --architectures x86_64 arm64 --disable-autoupdate
cmake --build ../out --parallel 3 --target Telegram
```

The result is `out/AyuGram.app`. CI packages use ad-hoc signing for testing,
not a Developer ID signature or Apple notarization. Public distribution needs
maintainer-controlled signing, notarization and a verified update channel.

For Apple Silicon only, append `mac-arm64` to the preparation command and set
both `CMAKE_OSX_ARCHITECTURES` and `DESKTOP_APP_MAC_ARCH` to `arm64`. Pass only
`arm64` to the checker. For Intel only, use `mac-x86_64` and `x86_64` instead.
Other dependencies retain their upstream universal build recipes.

## Debug and Xcode development

Without `qt-release-only`, preparation retains the upstream defaults: universal
Qt with Debug and Release configurations. `skip-release` builds Debug Qt only;
it cannot be combined with `qt-release-only`. Do not reuse a Release-only Qt
installation for a Debug application without rerunning preparation.

Omit `-G Ninja` to use Xcode. Open `out/Telegram.xcodeproj` and choose the
configuration there. Use Release for daily use and distribution.

## CI and updates

The `AyuGram macOS` workflow accepts the target architecture on manual dispatch.
It checks the generated compiler commands, Qt linkage, bundle identity, icon
and packaged architectures, then produces a ZIP with checksum and source SHA.
Credentials come from repository configuration, not from a particular owner
or branch name. The full build is not triggered on every push.

Automatic updates remain disabled for these preview packages until the
distribution has a verified signing and update channel. This does not change
the source default for maintainers building official releases.
