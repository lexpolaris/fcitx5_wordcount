cmake_minimum_required(VERSION 3.16)
project(fcitx5_wordcount VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets DBus Sql)

set(SOURCES
    src/main.cpp
    src/app_core.cpp
    src/core/char_counter.cpp
    src/core/level_system.cpp
    src/core/speed_tracker.cpp
    src/core/word_equivalence.cpp
    src/core/fcitx5_monitor.cpp
    src/engine/database_storage.cpp
    src/engine/statistics_engine.cpp
    src/ui/detail_panel.cpp
    src/ui/tray_icon_manager.cpp
    src/ui/settings_dialog.cpp
    src/ui/config_manager.cpp
    src/ui/insight_panel.cpp
)

set(HEADERS
    src/app_core.h
    src/core/char_counter.h
    src/core/fcitx5_monitor.h
    src/core/level_system.h
    src/core/speed_tracker.h
    src/core/word_equivalence.h
    src/engine/database_storage.h
    src/engine/statistics_engine.h
    src/ui/detail_panel.h
    src/ui/tray_icon_manager.h
    src/ui/settings_dialog.h
    src/ui/theme_helper.h
    src/ui/config_manager.h
    src/ui/insight_panel.h
)

add_executable(fcitx5_wordcount ${SOURCES} ${HEADERS})

target_include_directories(fcitx5_wordcount PRIVATE src)

target_link_libraries(fcitx5_wordcount
    Qt6::Core
    Qt6::Widgets
    Qt6::DBus
    Qt6::Sql
)

# 安装到系统
install(TARGETS fcitx5_wordcount RUNTIME DESTINATION bin)
