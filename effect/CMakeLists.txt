# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
find_package(Qt6 REQUIRED COMPONENTS DBus Quick Widgets)
find_package(KWin 6.8.2 EXACT REQUIRED GLOBAL)

# KWin's target requires C++23; the standalone policy remains C++20.
set(CMAKE_AUTOMOC ON)
list(APPEND CMAKE_AUTOMOC_MACRO_NAMES KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json" effect_metadata)
if(TARGET kwin_natural_slide_config)
    string(JSON effect_metadata SET "${effect_metadata}" X-KDE-ConfigModule
        "\"kwin_natural_slide_config\"")
endif()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/natural_slide_metadata.json"
    CONTENT "${effect_metadata}\n")
add_library(kwin_natural_slide MODULE main.cpp natural_slide.cpp natural_slide.h)
target_include_directories(kwin_natural_slide PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(kwin_natural_slide PRIVATE panel_policy gesture_settings KWin::kwin)
target_compile_options(kwin_natural_slide PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(kwin_natural_slide PRIVATE -Wl,--no-undefined)
set_target_properties(kwin_natural_slide PROPERTIES PREFIX "")
install(TARGETS kwin_natural_slide
    DESTINATION "${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins" COMPONENT effect)

if(ENABLE_OVERSCROLL_RENDERER)
    if(NOT OVERSCROLL_RENDERER_ID MATCHES "^kwin_natural_slide_preview[0-9]+$")
        message(FATAL_ERROR "Invalid OVERSCROLL_RENDERER_ID")
    endif()
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json" preview_metadata)
    string(JSON preview_metadata SET "${preview_metadata}" KPlugin Id "\"${OVERSCROLL_RENDERER_ID}\"")
    string(JSON preview_metadata SET "${preview_metadata}" KPlugin Name "\"Selective Panel Slide (Overscroll Preview)\"")
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/overscroll_metadata.json" CONTENT "${preview_metadata}\n")
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/overscroll-renderer-plugin-id.txt" CONTENT "${OVERSCROLL_RENDERER_ID}\n")
    add_library(kwin_natural_slide_preview MODULE main.cpp natural_slide.cpp natural_slide.h)
    target_include_directories(kwin_natural_slide_preview PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(kwin_natural_slide_preview PRIVATE panel_policy gesture_settings KWin::kwin)
    target_compile_definitions(kwin_natural_slide_preview PRIVATE NATURAL_SWIPE_OVERSCROLL_PREVIEW=1)
    target_compile_options(kwin_natural_slide_preview PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(kwin_natural_slide_preview PRIVATE -Wl,--no-undefined)
    set_target_properties(kwin_natural_slide_preview PROPERTIES PREFIX "" OUTPUT_NAME "${OVERSCROLL_RENDERER_ID}"
        CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
    install(TARGETS kwin_natural_slide_preview
        DESTINATION "${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins" COMPONENT overscroll)
endif()

if(ENABLE_GESTURE_OBSERVER)
    add_library(kwin_natural_swipe_probe MODULE swipe_probe.cpp swipe_observer.cpp)
    target_link_libraries(kwin_natural_swipe_probe PRIVATE swipe_tracker KWin::kwin)
    target_compile_options(kwin_natural_swipe_probe PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(kwin_natural_swipe_probe PRIVATE -Wl,--no-undefined)
    set_target_properties(kwin_natural_swipe_probe PROPERTIES PREFIX "")
    install(TARGETS kwin_natural_swipe_probe
        DESTINATION "${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins" COMPONENT probe)
endif()

if(ENABLE_GESTURE_CONTROLLER)
    if(NOT SWIPE_CONTROLLER_ID MATCHES "^kwin_natural_swipe_controller(_[a-zA-Z0-9]+)?$")
        message(FATAL_ERROR "Invalid SWIPE_CONTROLLER_ID")
    endif()
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/controller_metadata.json" controller_metadata)
    string(JSON controller_metadata SET "${controller_metadata}" KPlugin Id "\"${SWIPE_CONTROLLER_ID}\"")
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/natural_swipe_controller_metadata.json" CONTENT "${controller_metadata}\n")
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/controller-plugin-id.txt" CONTENT "${SWIPE_CONTROLLER_ID}\n")
    add_library(kwin_natural_swipe_controller MODULE swipe_controller.cpp)
    target_include_directories(kwin_natural_swipe_controller PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(kwin_natural_swipe_controller PRIVATE swipe_tracker KWin::kwin)
    target_compile_definitions(kwin_natural_swipe_controller PRIVATE
        NATURAL_SWIPE_SETTLING_UNDO=$<BOOL:${ENABLE_SETTLING_UNDO}>)
    target_compile_options(kwin_natural_swipe_controller PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(kwin_natural_swipe_controller PRIVATE -Wl,--no-undefined)
    set_target_properties(kwin_natural_swipe_controller PROPERTIES PREFIX "" OUTPUT_NAME "${SWIPE_CONTROLLER_ID}"
        CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
    install(TARGETS kwin_natural_swipe_controller
        DESTINATION "${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins" COMPONENT controller)
endif()

# Portable installer input: KDE/Qt decide lib vs lib64 vs multiarch paths.
# Target expressions also support non-default build directories and generators.
set(install_artifacts "{\"role\":\"renderer\",\"source\":\"$<TARGET_FILE:kwin_natural_slide>\",\"name\":\"kwin_natural_slide.so\"}")
if(TARGET kwin_natural_slide_config)
    string(APPEND install_artifacts ",{\"role\":\"settings\",\"source\":\"$<TARGET_FILE:kwin_natural_slide_config>\",\"name\":\"kwin_natural_slide_config.so\"}")
endif()
if(TARGET kwin_natural_swipe_controller)
    string(APPEND install_artifacts ",{\"role\":\"controller\",\"source\":\"$<TARGET_FILE:kwin_natural_swipe_controller>\",\"name\":\"${SWIPE_CONTROLLER_ID}.so\"}")
endif()
if(TARGET kwin_natural_slide_preview)
    string(APPEND install_artifacts ",{\"role\":\"overscroll\",\"source\":\"$<TARGET_FILE:kwin_natural_slide_preview>\",\"name\":\"${OVERSCROLL_RENDERER_ID}.so\"}")
endif()
if(TARGET kwin_natural_swipe_probe)
    string(APPEND install_artifacts ",{\"role\":\"probe\",\"source\":\"$<TARGET_FILE:kwin_natural_swipe_probe>\",\"name\":\"kwin_natural_swipe_probe.so\"}")
endif()
file(GENERATE OUTPUT "${PROJECT_BINARY_DIR}/install-layout-$<CONFIG>.json" CONTENT
    "{\"schemaVersion\":1,\"kwinVersion\":\"6.8.2\",\"pluginDirectory\":\"${KDE_INSTALL_FULL_PLUGINDIR}/kwin/effects\",\"artifacts\":[${install_artifacts}]}\n")
