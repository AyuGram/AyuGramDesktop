# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

add_executable(test_text WIN32)
init_target(test_text "(tests)")

target_include_directories(test_text PRIVATE ${src_loc})

nice_target_sources(test_text ${src_loc}
PRIVATE
    tests/test_main.cpp
    tests/test_main.h
    tests/test_text.cpp
)

nice_target_sources(test_text ${res_loc}
PRIVATE
    qrc/emoji_1.qrc
    qrc/emoji_2.qrc
    qrc/emoji_3.qrc
    qrc/emoji_4.qrc
    qrc/emoji_5.qrc
    qrc/emoji_6.qrc
    qrc/emoji_7.qrc
    qrc/emoji_8.qrc
)

target_link_libraries(test_text
PRIVATE
    desktop-app::lib_base
    desktop-app::lib_crl
    desktop-app::lib_ui
    desktop-app::external_qt
    desktop-app::external_qt_static_plugins
)

set_target_properties(test_text PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_dependencies(Telegram test_text)

target_prepare_qrc(test_text)

add_executable(test_data_unsent_read_generation)
init_target(test_data_unsent_read_generation "(tests)")

target_include_directories(test_data_unsent_read_generation PRIVATE ${src_loc})

nice_target_sources(test_data_unsent_read_generation ${src_loc}
PRIVATE
    tests/data_unsent_read_generation_test.cpp
)

set_target_properties(test_data_unsent_read_generation PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_dependencies(Telegram test_data_unsent_read_generation)

add_executable(test_data_unsent_read_till)
init_target(test_data_unsent_read_till "(tests)")

target_include_directories(test_data_unsent_read_till PRIVATE ${src_loc})

nice_target_sources(test_data_unsent_read_till ${src_loc}
PRIVATE
    tests/data_unsent_read_till_test.cpp
)

set_target_properties(test_data_unsent_read_till PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_dependencies(Telegram test_data_unsent_read_till)

add_executable(test_ayu_ghost_mode_peer_exceptions)
init_target(test_ayu_ghost_mode_peer_exceptions "(tests)")

target_include_directories(test_ayu_ghost_mode_peer_exceptions PRIVATE ${src_loc})

nice_target_sources(test_ayu_ghost_mode_peer_exceptions ${src_loc}
PRIVATE
    tests/ayu_ghost_mode_peer_exceptions_test.cpp
)

set_target_properties(test_ayu_ghost_mode_peer_exceptions PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_dependencies(Telegram test_ayu_ghost_mode_peer_exceptions)
