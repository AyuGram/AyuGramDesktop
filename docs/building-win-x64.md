# Build instructions for Windows 64-bit

- [Prepare folder](#prepare-folder)
- [Install third party software](#install-third-party-software)
- [Clone source code and prepare libraries](#clone-source-code-and-prepare-libraries)
- [Build the project](#build-the-project)

## Prepare folder

The build is done in **Visual Studio 2022** with **10.0.26100.0** SDK version.

Choose an empty folder for the future build, for example **D:\\TBuild**. It will be named ***BuildPath*** in the rest of this document. Create two folders there, ***BuildPath*\\ThirdParty** and ***BuildPath*\\Libraries**.

All commands (if not stated otherwise) will be launched from **x64 Native Tools Command Prompt for VS 2022.bat** (should be in **Start Menu > Visual Studio 2022** menu folder). Pay attention not to use any other Command Prompt.

## Install third party software

* Download **Python 3.10** installer from [https://www.python.org/downloads/](https://www.python.org/downloads/) and install it with adding to PATH.
* Download **Git** installer from [https://git-scm.com/download/win](https://git-scm.com/download/win) and install it.

## Clone source code and prepare libraries

Open **x64 Native Tools Command Prompt for VS 2022.bat**, go to ***BuildPath*** and run

    git clone --recursive https://github.com/AyuGram/AyuGramDesktop.git tdesktop
    tdesktop\Telegram\build\prepare\win.bat

You may encounter an error saying that your IP is not allowed - simply turn on VPN.

## Build the project

Go to ***BuildPath*\\tdesktop\\Telegram** and run

    configure.bat x64 -D TDESKTOP_API_ID=2040 -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627

* Open ***BuildPath*\\tdesktop\\out\\Telegram.slnx** in Visual Studio 2022
* Select Telegram project and press Build > Build Telegram (Debug and Release configurations)
* The result AyuGram.exe will be located in **D:\TBuild\tdesktop\out\Debug** (and **Release**)

If you encounter issue like `error C1090: PDB API call failed, error code '12'` on Release build, apply the following patch in `tdesktop/cmake` folder (via pwsh or manually):

```diff
@'
diff --git a/options_win.cmake b/options_win.cmake
index c2d66cf..ccceb53 100644
--- a/options_win.cmake
+++ b/options_win.cmake
@@ -32,6 +32,7 @@ if (MSVC)
       /utf-8
       /W4
       /MP     # Enable multi process build.
+        /FS
       /EHsc   # Catch C++ exceptions only, extern C functions never throw a C++ exception.
       /w15038 # wrong initialization order
       /w14265 # class has virtual functions, but destructor is not virtual
@@ -64,7 +65,7 @@ if (MSVC)
   INTERFACE
       $<$<CONFIG:Debug>:/NODEFAULTLIB:LIBCMT>
       $<$<AND:$<CONFIG:Debug>,$<BOOL:${build_win64}>>:/DEBUG:FASTLINK>
-        $<$<NOT:$<AND:$<CONFIG:Debug>,$<BOOL:${build_win64}>>>:$<IF:$<STREQUAL:$<GENEX_EVAL:
$<TARGET_PROPERTY:MSVC_DEBUG_INFORMATION_FORMAT>>,ProgramDatabase>,/DEBUG,/DEBUG:NONE>>
+        $<$<NOT:$<AND:$<CONFIG:Debug>,$<BOOL:${build_win64}>>>:$<IF:$<BOOL:$<GENEX_EVAL:
$<TARGET_PROPERTY:MSVC_DEBUG_INFORMATION_FORMAT>>>,/DEBUG,/DEBUG:NONE>>
       $<$<NOT:$<CONFIG:Debug>>:/OPT:REF>
       /INCREMENTAL:NO
       /DEPENDENTLOADFLAG:0x800
diff --git a/variables.cmake b/variables.cmake
index d6ac6c5..b2f492a 100644
--- a/variables.cmake
+++ b/variables.cmake
@@ -21,7 +21,9 @@ if (DESKTOP_APP_SPECIAL_TARGET STREQUAL ""
endif()

set(CMAKE_CXX_SCAN_FOR_MODULES OFF CACHE BOOL "")
-set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase" CACHE STRING "")
+set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
+    "$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>$<$<CONFIG:Release,MinSizeRel>:Embedded>"
+    CACHE STRING "" FORCE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")
option(DESKTOP_APP_TEST_APPS "Build test apps, development only." OFF)
option(DESKTOP_APP_LOTTIE_DISABLE_RECOLORING "Disable recoloring of lottie animations." OFF)
'@ | git -C cmake apply -
```
