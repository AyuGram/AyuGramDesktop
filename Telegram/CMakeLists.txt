# This file is part of Drakeygram Desktop,
# an independent Telegram Desktop fork.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

add_executable(Telegram WIN32 MACOSX_BUNDLE)
init_non_host_target(Telegram)

add_subdirectory(lib_rpl)
add_subdirectory(lib_crl)
add_subdirectory(lib_base)
add_subdirectory(lib_ui)
add_subdirectory(lib_tl)
add_subdirectory(lib_spellcheck)
add_subdirectory(lib_storage)
add_subdirectory(lib_lottie)
add_subdirectory(lib_qr)
add_subdirectory(lib_translate)
add_subdirectory(lib_webrtc)
add_subdirectory(lib_webview)
add_subdirectory(codegen)

# Drakeygram filters
add_subdirectory(lib_icu)
# Drakeygram filters

get_filename_component(src_loc SourceFiles REALPATH)
get_filename_component(res_loc Resources REALPATH)

include(cmake/telegram_options.cmake)
include(cmake/lib_ffmpeg.cmake)
include(cmake/lib_stripe.cmake)
include(cmake/lib_tgcalls.cmake)
include(cmake/lib_prisma.cmake)
include(cmake/lib_fido2.cmake)
include(cmake/td_export.cmake)
include(cmake/td_iv.cmake)
include(cmake/td_lang.cmake)
include(cmake/td_mtproto.cmake)
include(cmake/td_scheme.cmake)
include(cmake/td_tde2e.cmake)
include(cmake/td_ui.cmake)
include(cmake/td_webauthn.cmake)
include(cmake/telegram_apple_swift_runtime.cmake)
include(cmake/generate_appstream_changelog.cmake)

if (DESKTOP_APP_TEST_APPS)
    include(cmake/tests.cmake)
endif()

if (WIN32)
    include(cmake/generate_midl.cmake)
    generate_midl(Telegram ${src_loc}
        platform/win/windows_quiethours.idl
        platform/win/windows_toastactivator.idl
    )
endif()

include(cmake/generate_update_keys.cmake)
generate_update_keys(Telegram ${res_loc}/update)

if (DESKTOP_APP_TEST_APPS OR DESKTOP_APP_SPECIAL_TARGET)
    include(cmake/test_update_verify.cmake)
endif()

set_target_properties(Telegram PROPERTIES AUTOMOC ON)

target_link_libraries(Telegram
PRIVATE
    tdesktop::lib_fido2
    tdesktop::lib_tgcalls
    tdesktop::td_export
    tdesktop::td_iv
    tdesktop::td_lang
    tdesktop::td_mtproto
    tdesktop::td_scheme
    tdesktop::td_tde2e
    tdesktop::td_ui
    tdesktop::td_webauthn
    desktop-app::lib_webrtc
    desktop-app::lib_base
    desktop-app::lib_crl
    desktop-app::lib_ui
    desktop-app::lib_tl
    desktop-app::lib_spellcheck
    desktop-app::lib_storage
    desktop-app::lib_lottie
    desktop-app::lib_qr
    desktop-app::lib_translate
    desktop-app::lib_webview
    desktop-app::lib_ffmpeg
    desktop-app::lib_stripe
    desktop-app::external_zlib
    desktop-app::external_minizip
    desktop-app::external_kcoreaddons
    desktop-app::external_qt_static_plugins
    desktop-app::external_qt
    desktop-app::external_qr_code_generator
    desktop-app::external_crash_reports
    desktop-app::external_auto_updates
    desktop-app::external_openssl
    desktop-app::external_openal
    desktop-app::external_xxhash

    # Drakeygram filters
    drakeygram::lib_icu
)

# IMPORTANT: Full source list was truncated in a previous commit.
# This is a recovery commit. The complete source list must be restored
# from git history (commit 0fed6eb3) or the local fixed file.
# Temporary minimal set to keep the repo from being completely broken:

set(drakeygram_files
        ayu/ayu_worker.cpp
        ayu/ayu_worker.h
        ayu/ayu_url_handlers.cpp
        ayu/ayu_url_handlers.h
        ayu/ayu_state.cpp
        ayu/ayu_state.h
        ayu/ayu_settings.cpp
        ayu/ayu_settings.h
        ayu/ayu_lang.cpp
        ayu/ayu_lang.h
        ayu/ayu_infra.cpp
        ayu/ayu_infra.h
        ayu/utils/rc_manager.cpp
        ayu/utils/rc_manager.h
        ayu/ui/ayu_logo.cpp
        ayu/ui/ayu_logo.h
        ayu/ui/settings/settings_main.cpp
        ayu/ui/settings/settings_main.h
        ayu/ui/boxes/donate_info_box.cpp
        ayu/ui/boxes/donate_info_box.h
)

nice_target_sources(Telegram ${src_loc}
PRIVATE
    ${drakeygram_files}
)
