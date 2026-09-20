# Copyright (c) 2024-2026 The ZMK Contributors
# SPDX-License-Identifier: MIT

zephyr_library()

zephyr_include_directories(include)
zephyr_include_directories(/../zmk/app/include)

if(CONFIG_TRACKBALL_CONTROL)
    zephyr_library_sources(src/trackball_control.c)
    zephyr_library_sources(src/behavior_trackball.c)
endif()
