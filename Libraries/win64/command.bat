@echo OFF
if exist libjxl rmdir /Q /S libjxl
if %errorlevel% neq 0 exit /b %errorlevel%
if exist libjxl exit /b 1
if %errorlevel% neq 0 exit /b %errorlevel%
call git clone -b v0.11.1 --recursive --shallow-submodules https://github.com/libjxl/libjxl.git
if %errorlevel% neq 0 exit /b %errorlevel%
call cd libjxl
if %errorlevel% neq 0 exit /b %errorlevel%
call SET "cmake_defines=-DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DJPEGXL_ENABLE_FUZZERS=OFF -DJPEGXL_ENABLE_DEVTOOLS=OFF -DJPEGXL_ENABLE_TOOLS=OFF -DJPEGXL_ENABLE_DOXYGEN=OFF -DJPEGXL_ENABLE_MANPAGES=OFF -DJPEGXL_ENABLE_EXAMPLES=OFF -DJPEGXL_ENABLE_JNI=OFF -DJPEGXL_ENABLE_JPEGLI_LIBJPEG=OFF -DJPEGXL_ENABLE_SJPEG=OFF -DJPEGXL_ENABLE_OPENEXR=OFF -DJPEGXL_ENABLE_SKCMS=ON -DJPEGXL_ENABLE_VIEWERS=OFF -DJPEGXL_ENABLE_TCMALLOC=OFF -DJPEGXL_ENABLE_PLUGINS=OFF -DJPEGXL_ENABLE_COVERAGE=OFF -DJPEGXL_WARNINGS_AS_ERRORS=OFF"
if %errorlevel% neq 0 exit /b %errorlevel%
call cmake . ^-A %WIN32X64% ^-DCMAKE_INSTALL_PREFIX=%LIBS_DIR%/local ^-DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>" ^-DCMAKE_C_FLAGS="/DJXL_STATIC_DEFINE /DJXL_THREADS_STATIC_DEFINE /DJXL_CMS_STATIC_DEFINE" ^-DCMAKE_CXX_FLAGS="/DJXL_STATIC_DEFINE /DJXL_THREADS_STATIC_DEFINE /DJXL_CMS_STATIC_DEFINE" ^%cmake_defines%
if %errorlevel% neq 0 exit /b %errorlevel%
call cmake --build . --config Debug --parallel
if %errorlevel% neq 0 exit /b %errorlevel%
call cmake --install . --config Debug
if %errorlevel% neq 0 exit /b %errorlevel%
call cmake --build . --config Release --parallel
if %errorlevel% neq 0 exit /b %errorlevel%
call cmake --install . --config Release
if %errorlevel% neq 0 exit /b %errorlevel%
call 
if %errorlevel% neq 0 exit /b %errorlevel%
