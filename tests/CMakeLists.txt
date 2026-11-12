# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
set(CMAKE_AUTOMOC ON)
if(ENABLE_INTEGRATION_DRIVER AND BUILD_KWIN_EFFECT)
    add_executable(nested_test_client nested_client.cpp)
    target_link_libraries(nested_test_client PRIVATE Qt6::Widgets)
endif()

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
add_swipe_test(gesture_handover swipe_tracker)
add_swipe_test(output_context swipe_tracker)
add_swipe_test(spring_motion swipe_tracker)
add_swipe_test(api_compat swipe_tracker)
add_swipe_test(touchpad_portability swipe_tracker)

if(BUILD_CONFIGURATOR)
    add_swipe_test(settings_feedback configurator_ui)
    set_tests_properties(settings_feedback PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_swipe_test(configurator_layout configurator_ui)
    set_tests_properties(configurator_layout PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_swipe_test(gesture_configurator configurator_ui)
    set_tests_properties(gesture_configurator PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(BUILD_KWIN_EFFECT)
    add_swipe_test(plugin_metadata KWin::kwin)
    target_compile_definitions(plugin_metadata_test PRIVATE
        EFFECT_PATH="$<TARGET_FILE:kwin_natural_desktop_slide>"
        RENDERER_ID="${DESKTOP_SLIDE_RENDERER_ID}"
        NDS_KWIN_BUILD_VERSION="${NDS_KWIN_VERSION}"
        HAS_SETTINGS_MODULE=$<BOOL:$<TARGET_EXISTS:kwin_natural_desktop_slide_config>>)
    add_dependencies(plugin_metadata_test kwin_natural_desktop_slide)
endif()

if(TARGET kwin_natural_desktop_slide_config)
    add_swipe_test(settings_module KF6::KCMUtils KF6::CoreAddons)
    target_compile_definitions(settings_module_test PRIVATE
        SETTINGS_MODULE_PATH="$<TARGET_FILE:kwin_natural_desktop_slide_config>")
    add_dependencies(settings_module_test kwin_natural_desktop_slide_config)
    set_tests_properties(settings_module PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(TARGET kwin_natural_desktop_slide_probe)
    add_swipe_test(probe_metadata)
    target_compile_definitions(probe_metadata_test PRIVATE
        PROBE_PATH="$<TARGET_FILE:kwin_natural_desktop_slide_probe>")
    add_dependencies(probe_metadata_test kwin_natural_desktop_slide_probe)
endif()

if(TARGET kwin_natural_desktop_slide_controller)
    add_swipe_test(controller_metadata)
    target_compile_definitions(controller_metadata_test PRIVATE
        CONTROLLER_PATH="$<TARGET_FILE:kwin_natural_desktop_slide_controller>"
        CONTROLLER_ID="${DESKTOP_SLIDE_CONTROLLER_ID}")
    add_dependencies(controller_metadata_test kwin_natural_desktop_slide_controller)
endif()

if(BUILD_DIAGNOSTIC)
    add_swipe_test(diagnostic)
    # Minimal distro containers may default to the non-UTF-8 C locale. Qt's
    # startup warning would otherwise pollute the child-process stderr checks.
    set_tests_properties(diagnostic PROPERTIES ENVIRONMENT "LANG=C.UTF-8;LC_ALL=C.UTF-8")
    target_compile_definitions(diagnostic_test PRIVATE
        DIAGNOSTIC_PATH="$<TARGET_FILE:natural-desktop-slide-diagnostic>"
        EXAMPLES_PATH="${PROJECT_SOURCE_DIR}/examples")
    add_dependencies(diagnostic_test natural-desktop-slide-diagnostic)
endif()

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    if(TARGET kwin_natural_desktop_slide_config AND NOT ENABLE_INTEGRATION_DRIVER)
        add_test(NAME settings_entry_points COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/settings_entry_points_test.py"
            --build-dir "${PROJECT_BINARY_DIR}" --config "$<CONFIG>")
    endif()
    if(BUILD_DISTRIBUTION_PACKAGE)
        add_test(NAME package_payload COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/package_payload_test.py"
            --build-dir "${PROJECT_BINARY_DIR}")
    endif()
    add_test(NAME release_tools COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/release_tools_test.py")
    add_test(NAME package_metadata COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/package_metadata_test.py")
    add_test(NAME startup_session_handoff COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/startup_session_test.py")
    add_test(NAME session_effect_rollback COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/session_effect_test.py")
    add_test(NAME settings_atomic_install COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/settings_install_test.py")
endif()
