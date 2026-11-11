# SPDX-FileCopyrightText: 2026 Matt Hetherington
# SPDX-License-Identifier: GPL-2.0-or-later
find_package(KF6KCMUtils 6.30 REQUIRED GLOBAL)
find_package(KF6CoreAddons 6.30 REQUIRED GLOBAL)
set(CMAKE_AUTOMOC ON)
add_library(kwin_natural_slide_config MODULE panel_slide_kcm.cpp)
target_link_libraries(kwin_natural_slide_config PRIVATE
    configurator_ui KF6::KCMUtils KF6::CoreAddons)
target_compile_options(kwin_natural_slide_config PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(kwin_natural_slide_config PRIVATE -Wl,--no-undefined)
set_target_properties(kwin_natural_slide_config PROPERTIES PREFIX "")
install(TARGETS kwin_natural_slide_config
    DESTINATION "${KDE_INSTALL_PLUGINDIR}/kwin/effects/configs" COMPONENT settings)
