---
name: ayugram-build
description: Orchestrates the compilation and build process of AyuGram Desktop on Windows using MSVC.
---

# AyuGram Build Skill

This skill provides the high-level workflow and references required to compile and build AyuGram Desktop on Windows with maximum performance.

## Prerequisites
- Visual Studio 2022 Build Tools (with C++ VCTools, MFC, ATL, and Windows 11 SDK).
- Git, Python 3.10.

## Build Workflow

1. **Environment Preparation**
   The build MUST be executed within the "x64 Native Tools Command Prompt for VS 2022".
   *Command to launch natively in background (if needed via PowerShell):*
   `cmd.exe /k "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"`

2. **Fetching Dependencies (ThirdParty/Libraries)**
   Navigate to the repository root (`C:\Users\fjuni\AyuGramDesktop`) and run the preparation script:
   ```cmd
   Telegram\build\prepare\win.bat
   ```
   *Note: This script downloads MSYS2, CMake, Ninja, Python packages, and compiles third-party C++ libraries.*

3. **Configuring the Project**
   Go into the `Telegram` directory and generate the `.slnx` solution using the API credentials:
   ```cmd
   cd Telegram
   configure.bat x64 -D TDESKTOP_API_ID=2040 -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627
   ```

4. **Building with Maximum Performance**
   Compile the generated solution using `msbuild` with the `/m` flag (multi-processor) to maximize efficiency:
   ```cmd
   msbuild out\Telegram.slnx /p:Configuration=Release /p:Platform=x64 /m
   ```

## References
- The primary build instructions are located in `docs/building-win-x64.md`. Always consult this file if errors occur during configuration or compilation.
- If you encounter an error like `error C1090: PDB API call failed`, apply the patch detailed at the end of `docs/building-win-x64.md`.
