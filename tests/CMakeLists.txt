# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
set(CMAKE_AUTOMOC ON)

function(add_swipe_test name)
    add_executable(${name}_test ${name}_test.cpp)
    target_link_libraries(${name}_test PRIVATE Qt6::Test ${ARGN})
    add_test(NAME ${name} COMMAND ${name}_test)
endfunction()

add_swipe_test(panel_policy panel_policy)
add_swipe_test(config panel_policy)
add_swipe_test(window_adapter panel_policy)
add_swipe_test(panel_picker panel_policy)
add_swipe_test(swipe_tracker swipe_tracker)
add_swipe_test(swipe_session swipe_tracker)
add_swipe_test(settling_swipe swipe_tracker)
add_swipe_test(overscroll swipe_tracker)
add_swipe_test(gesture_settings swipe_tracker)

if(BUILD_CONFIGURATOR)
    add_swipe_test(configurator_layout configurator_ui)
    set_tests_properties(configurator_layout PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_swipe_test(gesture_configurator configurator_ui)
    set_tests_properties(gesture_configurator PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(BUILD_KWIN_EFFECT)
    add_swipe_test(plugin_metadata KWin::kwin)
    target_compile_definitions(plugin_metadata_test PRIVATE
        EFFECT_PATH="$<TARGET_FILE:kwin_natural_slide>"
        HAS_SETTINGS_MODULE=$<BOOL:$<TARGET_EXISTS:kwin_natural_slide_config>>)
    add_dependencies(plugin_metadata_test kwin_natural_slide)
endif()

if(TARGET kwin_natural_slide_config)
    add_swipe_test(settings_module KF6::KCMUtils KF6::CoreAddons)
    target_compile_definitions(settings_module_test PRIVATE
        SETTINGS_MODULE_PATH="$<TARGET_FILE:kwin_natural_slide_config>")
    add_dependencies(settings_module_test kwin_natural_slide_config)
    set_tests_properties(settings_module PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET kwin_natural_swipe_probe)
    add_swipe_test(probe_metadata)
    target_compile_definitions(probe_metadata_test PRIVATE
        PROBE_PATH="$<TARGET_FILE:kwin_natural_swipe_probe>")
    add_dependencies(probe_metadata_test kwin_natural_swipe_probe)
endif()

if(TARGET kwin_natural_swipe_controller)
    add_swipe_test(controller_metadata)
    target_compile_definitions(controller_metadata_test PRIVATE
        CONTROLLER_PATH="$<TARGET_FILE:kwin_natural_swipe_controller>"
        CONTROLLER_ID="${SWIPE_CONTROLLER_ID}")
    add_dependencies(controller_metadata_test kwin_natural_swipe_controller)
endif()

if(TARGET kwin_natural_slide_preview)
    add_swipe_test(overscroll_metadata)
    target_compile_definitions(overscroll_metadata_test PRIVATE
        OVERSCROLL_PATH="$<TARGET_FILE:kwin_natural_slide_preview>"
        OVERSCROLL_ID="${OVERSCROLL_RENDERER_ID}")
    add_dependencies(overscroll_metadata_test kwin_natural_slide_preview)
endif()

if(BUILD_DIAGNOSTIC)
    add_swipe_test(diagnostic)
    target_compile_definitions(diagnostic_test PRIVATE
        DIAGNOSTIC_PATH="$<TARGET_FILE:natural-swipe-diagnostic>"
        EXAMPLES_PATH="${PROJECT_SOURCE_DIR}/examples")
    add_dependencies(diagnostic_test natural-swipe-diagnostic)
endif()

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME package_metadata COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/package_metadata_test.py")
    add_test(NAME startup_session_handoff COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/startup_session_test.py")
    add_test(NAME session_effect_rollback COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/session_effect_test.py")
    add_test(NAME settings_atomic_install COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/settings_install_test.py")
endif()
