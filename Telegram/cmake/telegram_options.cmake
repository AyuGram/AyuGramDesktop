# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

set(TDESKTOP_API_ID "2040" CACHE STRING "Provide 'api_id' for the Telegram API access.")
set(TDESKTOP_API_HASH "b18441a1ff607e10a989891a5462e627" CACHE STRING "Provide 'api_hash' for the Telegram API access.")

if (TDESKTOP_API_TEST)
    set(TDESKTOP_API_ID 17349)
    set(TDESKTOP_API_HASH 344583e45741c457fe1862106095a5eb)
endif()

if (TDESKTOP_API_ID STREQUAL "0" OR TDESKTOP_API_HASH STREQUAL "")
    set(TDESKTOP_API_ID 2040)
    set(TDESKTOP_API_HASH b18441a1ff607e10a989891a5462e627)
endif()

if (DESKTOP_APP_DISABLE_AUTOUPDATE)
    target_compile_definitions(Telegram PRIVATE TDESKTOP_DISABLE_AUTOUPDATE)
endif()

if (DESKTOP_APP_DISABLE_CRASH_REPORTS)
    target_compile_definitions(Telegram PRIVATE TDESKTOP_DISABLE_CRASH_REPORTS)
endif()

if (DESKTOP_APP_USE_PACKAGED)
    target_compile_definitions(Telegram PRIVATE TDESKTOP_USE_PACKAGED)
endif()

if (DESKTOP_APP_SPECIAL_TARGET)
    target_compile_definitions(Telegram PRIVATE TDESKTOP_ALLOW_CLOSED_ALPHA)
endif()

option(DESKTOP_APP_DISABLE_SWIFT6 "Disable local on-device translation (build without Swift 6 on macOS)." OFF)
if (DESKTOP_APP_DISABLE_SWIFT6)
    target_compile_definitions(Telegram PRIVATE TDESKTOP_DISABLE_SWIFT6)
endif()
