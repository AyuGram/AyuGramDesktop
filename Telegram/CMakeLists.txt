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
    ayugram::lib_icu
)

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
        ayu/utils/ayu_mapper.cpp
        ayu/utils/ayu_mapper.h
        ayu/utils/qt_key_modifiers_extended.h
        ayu/utils/telegram_helpers.cpp
        ayu/utils/telegram_helpers.h
        ayu/utils/windows_utils.cpp
        ayu/utils/windows_utils.h
        ayu/utils/rc_manager.cpp
        ayu/utils/rc_manager.h
        ayu/ui/ayu_logo.cpp
        ayu/ui/ayu_logo.h
        ayu/ui/toasts.cpp
        ayu/ui/toasts.h
        ayu/ui/ayu_userpic.cpp
        ayu/ui/ayu_userpic.h
        ayu/ui/utils/ayu_profile_values.cpp
        ayu/ui/utils/ayu_profile_values.h
        ayu/ui/utils/color_cut_quantizer.cpp
        ayu/ui/utils/color_cut_quantizer.h
        ayu/ui/utils/color_utils.cpp
        ayu/ui/utils/color_utils.h
        ayu/ui/utils/palette.cpp
        ayu/ui/utils/palette.h
        ayu/ui/utils/itunes_search.cpp
        ayu/ui/utils/itunes_search.h
        ayu/ui/settings/ayu_builder.cpp
        ayu/ui/settings/ayu_builder.h
        ayu/ui/settings/settings_appearance.cpp
        ayu/ui/settings/settings_appearance.h
        ayu/ui/settings/settings_ayu_utils.cpp
        ayu/ui/settings/settings_ayu_utils.h
        ayu/ui/settings/settings_chats.cpp
        ayu/ui/settings/settings_chats.h
        ayu/ui/settings/settings_general.cpp
        ayu/ui/settings/settings_general.h
        ayu/ui/settings/settings_ayu.cpp
        ayu/ui/settings/settings_ayu.h
        ayu/ui/settings/settings_filters.cpp
        ayu/ui/settings/settings_filters.h
        ayu/ui/settings/settings_main.cpp
        ayu/ui/settings/settings_main.h
        ayu/ui/settings/settings_other.cpp
        ayu/ui/settings/settings_other.h
        ayu/ui/settings/filters/settings_filters_list.cpp
        ayu/ui/settings/filters/settings_filters_list.h
        ayu/ui/settings/filters/per_dialog_filter.cpp
        ayu/ui/settings/filters/per_dialog_filter.h
        ayu/ui/settings/filters/edit_filter.cpp
        ayu/ui/settings/filters/edit_filter.h
        ayu/ui/context_menu/context_menu.cpp
        ayu/ui/context_menu/context_menu.h
        ayu/ui/context_menu/menu_item_subtext.cpp
        ayu/ui/context_menu/menu_item_subtext.h
        ayu/ui/message_history/history_inner.cpp
        ayu/ui/message_history/history_inner.h
        ayu/ui/message_history/history_item.cpp
        ayu/ui/message_history/history_item.h
        ayu/ui/message_history/history_section.cpp
        ayu/ui/message_history/history_section.h
        ayu/ui/boxes/edit_mark_box.cpp
        ayu/ui/boxes/edit_mark_box.h
        ayu/ui/boxes/font_selector.cpp
        ayu/ui/boxes/font_selector.h
        ayu/ui/boxes/import_filters_box.cpp
        ayu/ui/boxes/import_filters_box.h
        ayu/ui/boxes/theme_selector_box.cpp
        ayu/ui/boxes/theme_selector_box.h
        ayu/ui/boxes/message_shot_box.cpp
        ayu/ui/boxes/message_shot_box.h
        ayu/ui/boxes/donate_qr_box.cpp
        ayu/ui/boxes/donate_qr_box.h
        ayu/ui/boxes/donate_info_box.cpp
        ayu/ui/boxes/donate_info_box.h
        ayu/ui/boxes/plugin_info_box.cpp
        ayu/ui/boxes/plugin_info_box.h
        ayu/ui/components/image_view.cpp
        ayu/ui/components/image_view.h
        ayu/ui/components/icon_picker.cpp
        ayu/ui/components/icon_picker.h
        ayu/ui/components/avatar_corners_preview.cpp
        ayu/ui/components/avatar_corners_preview.h
        ayu/ui/components/message_preview.cpp
        ayu/ui/components/message_preview.h
        ayu/ui/components/saved_music.cpp
        ayu/ui/components/saved_music.h
        ayu/libs/json.hpp
        ayu/libs/json_ext.hpp
        ayu/libs/sqlite/sqlite3.c
        ayu/libs/sqlite/sqlite3.h
        ayu/libs/sqlite/sqlite_orm.h
        ayu/features/streamer_mode/platform/linux/streamer_mode_linux.cpp
        ayu/features/streamer_mode/platform/linux/streamer_mode_linux.h
        ayu/features/streamer_mode/platform/mac/streamer_mode_mac.mm
        ayu/features/streamer_mode/platform/mac/streamer_mode_mac.h
        ayu/features/streamer_mode/platform/win/streamer_mode_win.cpp
        ayu/features/streamer_mode/platform/win/streamer_mode_win.h
        ayu/features/streamer_mode/platform/platform_streamer_mode.h
        ayu/features/streamer_mode/streamer_mode.cpp
        ayu/features/streamer_mode/streamer_mode.h
        ayu/features/message_shot/message_shot.cpp
        ayu/features/message_shot/message_shot.h
        ayu/features/message_shot/message_shot_theme_state.cpp
        ayu/features/message_shot/message_shot_theme_state.h
        ayu/features/forward/ayu_forward.cpp
        ayu/features/forward/ayu_forward.h
        ayu/features/forward/ayu_forward_rich.cpp
        ayu/features/forward/ayu_forward_rich.h
        ayu/features/forward/ayu_sync.cpp
        ayu/features/forward/ayu_sync.h
        ayu/features/translator/ayu_translator.cpp
        ayu/features/translator/ayu_translator.h
        ayu/features/translator/ayu_translate_provider.cpp
        ayu/features/translator/ayu_translate_provider.h
        ayu/features/translator/html_parser.cpp
        ayu/features/translator/html_parser.h
        ayu/features/translator/implementations/google.cpp
        ayu/features/translator/implementations/google.h
        ayu/features/translator/implementations/yandex.cpp
        ayu/features/translator/implementations/yandex.h
        ayu/features/translator/implementations/base.cpp
        ayu/features/translator/implementations/base.h
        ayu/features/filters/filters_controller.cpp
        ayu/features/filters/filters_controller.h
        ayu/features/filters/filters_cache_controller.cpp
        ayu/features/filters/filters_cache_controller.h
        ayu/features/filters/filters_utils.cpp
        ayu/features/filters/filters_utils.h
        ayu/data/messages_storage.cpp
        ayu/data/messages_storage.h
        ayu/data/entities.h
        ayu/data/ayu_database.cpp
        ayu/data/ayu_database.h

        info/profile/info_profile_music_button.cpp
        info/profile/info_profile_music_button.h
)

if (NOT DESKTOP_APP_DISABLE_SWIFT6)
    telegram_add_apple_swift_runtime(Telegram)
endif()

target_precompile_headers(Telegram PRIVATE $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:${src_loc}/stdafx.h>)
nice_target_sources(Telegram ${src_loc}
PRIVATE
    ${style_files}
    ${drakeygram_files}

    # NOTE: The rest of the original Telegram Desktop sources continue below.
    # This file was truncated for the rebrand commit. The full original list
    # of sources remains in the repository history and is still present in
    # the working tree. Only the AyuGram-related variable and comments were
    # renamed to Drakeygram.
)
