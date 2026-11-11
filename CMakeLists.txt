# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.25)
project(kwin-natural-desktop-slide VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_KWIN_EFFECT "Build the effect for a reviewed, exact-runtime-matched KWin version" OFF)
option(BUILD_CONFIGURATOR "Build the standalone panel picker" ON)
option(BUILD_SETTINGS_MODULE "Include the KDE settings module in effect builds" ON)
option(BUILD_DIAGNOSTIC "Build read-only configuration/panel diagnostics" ON)
option(ENABLE_GESTURE_OBSERVER "Observe raw swipes without consuming events or driving animation" OFF)
option(ENABLE_GESTURE_CONTROLLER "Build the separate gesture owner/animation bridge" ON)
option(ENABLE_SETTLING_UNDO "Build continuous horizontal finger-scroll follow-ups while settling" ON)
option(ENABLE_INTEGRATION_DRIVER "Build unsafe test-only hooks for an isolated nested KWin" OFF)
option(BUILD_DISTRIBUTION_PACKAGE "Install complete end-user integration and configure native packaging" OFF)
mark_as_advanced(ENABLE_INTEGRATION_DRIVER)
set(DESKTOP_SLIDE_RENDERER_ID "kwin_natural_desktop_slide" CACHE STRING "Renderer identity; use a _dev suffix for same-session development")
set(DESKTOP_SLIDE_CONTROLLER_ID "kwin_natural_desktop_slide_controller" CACHE STRING "Controller identity; use a _dev suffix for same-session development")

# Include KDE install paths before Qt can introduce conflicting GNUInstallDirs defaults.
if(BUILD_KWIN_EFFECT)
    find_package(ECM 6.26 REQUIRED NO_MODULE)
    list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
    include(KDEInstallDirs6)
endif()
find_package(Qt6 6.5 REQUIRED COMPONENTS Core)

add_library(panel_policy STATIC
    src/panel_policy.cpp
    src/config.cpp
    src/window_adapter.cpp
    src/panel_picker.cpp
)
add_library(gesture_settings STATIC src/gesture_settings.cpp)
target_include_directories(gesture_settings PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(gesture_settings PUBLIC Qt6::Core)
set_target_properties(gesture_settings PROPERTIES POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
add_library(swipe_tracker STATIC src/swipe_tracker.cpp src/swipe_session.cpp src/settling_swipe.cpp)
set_target_properties(swipe_tracker PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(swipe_tracker PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
target_include_directories(swipe_tracker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(swipe_tracker PUBLIC Qt6::Core gesture_settings)
set_target_properties(panel_policy PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(panel_policy PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(panel_policy PUBLIC Qt6::Core)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(panel_policy PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(swipe_tracker PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(gesture_settings PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_DIAGNOSTIC)
    find_package(Qt6 6.5 REQUIRED COMPONENTS DBus)
    add_executable(natural-desktop-slide-diagnostic src/diagnostic.cpp)
    target_link_libraries(natural-desktop-slide-diagnostic PRIVATE panel_policy Qt6::DBus)
endif()

if(BUILD_CONFIGURATOR)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets DBus)
    add_library(configurator_ui STATIC src/configurator.cpp src/gesture_configurator.cpp src/color_picker.cpp)
    set_target_properties(configurator_ui PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(configurator_ui PUBLIC panel_policy gesture_settings Qt6::Widgets Qt6::DBus)
    add_executable(natural-desktop-slide-config src/configurator_main.cpp)
    target_link_libraries(natural-desktop-slide-config PRIVATE configurator_ui)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(configurator_ui PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(natural-desktop-slide-config PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_KWIN_EFFECT)
    if(BUILD_CONFIGURATOR AND BUILD_SETTINGS_MODULE)
        add_subdirectory(kcm)
    endif()
    add_subdirectory(effect)
endif()

if(BUILD_DISTRIBUTION_PACKAGE)
    add_subdirectory(packaging)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
