## Build instructions for macOS

### Note

The build has only been tested with Xcode 26.1. Although it may work on lower Xcode versions, this is not guaranteed.

### Prepare folder

Choose a folder for the future build, for example **/Users/user/TBuild**. It will be named ***BuildPath*** in the rest of this document. All commands will be launched from Terminal.

**Note about disk space:** The full build process will require approximately **55 GB** of free space. This includes:
- **~35 GB** for libraries (when building for both x64 and arm64 architectures)
- **~20 GB** for the compiled Telegram app (in the `out` folder)

### Clone source code and prepare libraries

Go to ***BuildPath*** and run

    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    brew install git automake cmake wget pkg-config gnu-tar ninja nasm meson

    sudo xcode-select -s /Applications/Xcode.app/Contents/Developer

    git clone --recursive https://github.com/AyuGram/AyuGramDesktop.git tdesktop
    ./tdesktop/Telegram/build/prepare/mac.sh

### Building the project

Go to ***BuildPath*/tdesktop/Telegram** and run

    ./configure.sh -D TDESKTOP_API_ID=2040 -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627

Then launch Xcode, open ***BuildPath*/tdesktop/out/Telegram.xcodeproj** and build for Debug / Release.

#### Apple Silicon build from Terminal

To build a native Release version on an Apple Silicon Mac without opening Xcode, go to ***BuildPath*/tdesktop** and run

    cmake --build out --config Release --target Telegram -- -arch arm64 -quiet

The application will be created at ***BuildPath*/tdesktop/out/Release/AyuGram.app**. You can verify the executable architecture with

    file out/Release/AyuGram.app/Contents/MacOS/AyuGram

The output should contain `Mach-O 64-bit executable arm64`.
