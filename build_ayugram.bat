@echo off
setlocal enabledelayedexpansion

echo ==================================================
echo AyuGram Desktop Auto-Builder
echo ==================================================

:: 1. Find Visual Studio 2022 using vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
)

if not exist "!VSWHERE!" (
    echo [ERROR] Could not find vswhere.exe. Please ensure Visual Studio 2022 is installed.
    pause
    exit /b 1
)

:: Find installation path
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -version [17.0^,18.0^) -property installationPath`) do (
    set "VS_PATH=%%i"
)

if "!VS_PATH!"=="" (
    echo [ERROR] Visual Studio 2022 installation path not found.
    pause
    exit /b 1
)

echo Found Visual Studio 2022 at: !VS_PATH!

:: 2. Load x64 Native Tools Environment
set "VCVARS=!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat"
if not exist "!VCVARS!" (
    set "VCVARS=!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat"
    set "VCVARS_ARGS=x64"
)

if not exist "!VCVARS!" (
    echo [ERROR] Could not find vcvars64.bat or vcvarsall.bat in Visual Studio installation.
    pause
    exit /b 1
)

echo Loading VS Developer Environment...
if "!VCVARS_ARGS!"=="" (
    call "!VCVARS!"
) else (
    call "!VCVARS!" !VCVARS_ARGS!
)

:: 3. Find Python and add to PATH if not already present
where python >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Python not found in system PATH. Searching standard folders...
    for /d %%d in ("%LocalAppData%\Programs\Python\Python*") do (
        if exist "%%d\python.exe" (
            set "PATH=%%d;%%d\Scripts;!PATH!"
            echo Added Python to build PATH: %%d
        )
    )
)

:: Re-verify Python presence
where python >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Python not found. Please install Python and add it to PATH.
    pause
    exit /b 1
)

set "PATH=C:\Users\aver\AppData\Local\Programs\Python\Python313;C:\Users\aver\AppData\Local\Programs\Python\Python313\Scripts;%PATH%"

:: 4. Run Win.bat dependency preparation if Libraries doesn't exist
if exist "..\Libraries\win64" goto skip_prepare

echo ==================================================
echo Preparing dependencies (silent mode)...
echo ==================================================
call Telegram\build\prepare\win.bat silent
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Dependency preparation failed.
    pause
    exit /b %ERRORLEVEL%
)
goto configure

:skip_prepare
echo [INFO] Existing Libraries folder found at ..\Libraries\win64. Skipping dependency preparation.

:configure

:: 5. Configure project
echo ==================================================
echo Configuring AyuGram...
echo ==================================================
call Telegram\configure.bat x64 -D TDESKTOP_API_ID=2040 -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Configuration failed.
    pause
    exit /b %ERRORLEVEL%
)

:: 6. Build project
echo ==================================================
echo Building AyuGram (Release)...
echo ==================================================
cmake --build out --config Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b %ERRORLEVEL%
)

echo ==================================================
echo AyuGram compiled successfully!
echo The executable is located at: out\Release\AyuGram.exe
echo ==================================================
pause
