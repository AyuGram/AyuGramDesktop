# Build instructions for Windows 64-bit

- [Prepare folder](#prepare-folder)
- [Install third party software](#install-third-party-software)
- [Clone source code and prepare libraries](#clone-source-code-and-prepare-libraries)
- [Build the project](#build-the-project)
- [Qt Visual Studio Tools](#qt-visual-studio-tools)

## Prepare folder

The build is done in **Visual Studio 2022** with **10.0.22000.0** SDK version.

Choose an empty folder for the future build, for example **D:\\TBuild**. It will be named ***BuildPath*** in the rest of this document. Create two folders there, ***BuildPath*\\ThirdParty** and ***BuildPath*\\Libraries**.

All commands (if not stated otherwise) will be launched from **x64 Native Tools Command Prompt for VS 2022.bat** (should be in **Start Menu > Visual Studio 2022** menu folder). Pay attention not to use any other Command Prompt.

## Install third party software

* Download **Python 3.10** installer from [https://www.python.org/downloads/](https://www.python.org/downloads/) and install it with adding to PATH.
* Download **CMake 3.21 or later** installer from [https://cmake.org/download/](https://cmake.org/download/) and install it.
* Download **Git** installer from [https://git-scm.com/download/win](https://git-scm.com/download/win) and install it.

## Clone source code and prepare libraries

Open **x64 Native Tools Command Prompt for VS 2022.bat**, go to ***BuildPath*** and run

    git clone --recursive https://github.com/AyuGram/AyuGramDesktop.git tdesktop
    tdesktop\Telegram\build\prepare\win.bat

## Build the project

Go to ***BuildPath*\\tdesktop\\Telegram** and run

    configure.bat x64 -D TDESKTOP_API_ID=2040 -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627

* Open ***BuildPath*\\tdesktop\\out\\Telegram.sln** in Visual Studio 2022
* Select Telegram project and press Build > Build Telegram (Debug and Release configurations)
* The result AyuGram.exe will be located in **D:\TBuild\tdesktop\out\Debug** (and **Release**)

### Qt Visual Studio Tools

For better debugging you may want to install Qt Visual Studio Tools:

* Open **Extensions** -> **Manage Extensions**
* Go to **Online** tab
* Search for **Qt**
* Install **Qt Visual Studio Tools** extension
