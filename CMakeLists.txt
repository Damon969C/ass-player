cmake_minimum_required(VERSION 3.21)

project(SubtitleSidecarPlayerQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network)

qt_standard_project_setup()

qt_add_executable(subtitle_sidecar_player_qt WIN32
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    src/media_tools.cpp
    src/media_tools.h
    src/mpv_controller.cpp
    src/mpv_controller.h
    resources/app.qrc
    src/subtitle_parser.cpp
    src/subtitle_parser.h
)

target_link_libraries(subtitle_sidecar_player_qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

if(WIN32)
    enable_language(RC)
    target_sources(subtitle_sidecar_player_qt PRIVATE resources/windows/app.rc)
    target_compile_definitions(subtitle_sidecar_player_qt PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    set_property(TARGET subtitle_sidecar_player_qt PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    if(TARGET Qt6::QWindowsIntegrationPlugin)
        qt_import_plugins(subtitle_sidecar_player_qt
            INCLUDE_BY_TYPE platforms Qt6::QWindowsIntegrationPlugin
        )
    endif()
endif()

install(TARGETS subtitle_sidecar_player_qt
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)
