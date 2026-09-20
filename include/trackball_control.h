/*
 * Generic Advanced Trackball Control Subsystem for ZMK
 * Copyright (c) 2024-2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef ZEPHYR_DRIVERS_TRACKBALL_CONTROL_H_
#define ZEPHYR_DRIVERS_TRACKBALL_CONTROL_H_

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

// Layer IDs from Kconfig
#ifndef CONFIG_TRACKBALL_MOUSE_LAYER_ID
#define TRACKBALL_MOUSE_LAYER_ID 4
#else
#define TRACKBALL_MOUSE_LAYER_ID CONFIG_TRACKBALL_MOUSE_LAYER_ID
#endif

#ifndef CONFIG_TRACKBALL_SNIPE_LAYER_ID
#define TRACKBALL_SNIPE_LAYER_ID 5
#else
#define TRACKBALL_SNIPE_LAYER_ID CONFIG_TRACKBALL_SNIPE_LAYER_ID
#endif

// Initialization
void trackball_control_init(void);

// Motion Hook: called when sensor detects motion (handles auto-mouse activation)
void trackball_control_on_motion(int8_t dx, int8_t dy);

// Motion Processing & Math
void trackball_control_rotate_motion(int in_dx, int in_dy, int *out_dx, int *out_dy);
void trackball_control_calculate_motion(int dx, int dy, int *out_dx, int *out_dy);
void trackball_control_clear_accum(void);

// Scroll Mode & Sensitivity
bool trackball_control_is_scroll_mode(void);
void trackball_control_set_scroll_mode(bool enable);
void trackball_control_toggle_scroll_mode(void);
void trackball_control_scroll_speed_up(void);
void trackball_control_scroll_speed_down(void);
uint8_t trackball_control_get_scroll_level(void);
int trackball_control_get_scroll_div(void);

// Pointer Speed (16 steps)
void trackball_control_speed_up(void);
void trackball_control_speed_down(void);
uint8_t trackball_control_get_speed_level(void);

// Auto-Mouse Layer Control
void trackball_control_toggle_automouse(void);
bool trackball_control_is_automouse_enabled(void);
void trackball_control_automouse_time_up(void);
void trackball_control_automouse_time_down(void);
void trackball_control_automouse_time_reset(void);
uint16_t trackball_control_get_automouse_time(void);

// Acceleration (Quadratic Curve, 1-5 levels)
void trackball_control_toggle_acceleration(void);
bool trackball_control_is_acceleration_enabled(void);
void trackball_control_accel_up(void);
void trackball_control_accel_down(void);
void trackball_control_accel_reset(void);
uint8_t trackball_control_get_accel_level(void);

// Soft Smoothing (EMA Filter)
void trackball_control_toggle_smoothing(void);
bool trackball_control_is_smoothing_enabled(void);

// Rotation Angle Control (-180 to +180 deg in 10-deg steps)
void trackball_control_rotate_cw(void);
void trackball_control_rotate_ccw(void);
void trackball_control_rotate_reset(void);
int16_t trackball_control_get_rotation_angle(void);

// Sniper Mode Status
bool trackball_control_is_sniper_active(void);

// Backward Compatibility Aliases for PAW3204 driver
#define paw3204_control_init                  trackball_control_init
#define paw3204_control_on_motion             trackball_control_on_motion
#define paw3204_control_rotate_motion         trackball_control_rotate_motion
#define paw3204_control_calculate_motion      trackball_control_calculate_motion
#define paw3204_control_clear_accum           trackball_control_clear_accum
#define paw3204_control_is_scroll_mode        trackball_control_is_scroll_mode
#define paw3204_control_set_scroll_mode       trackball_control_set_scroll_mode
#define paw3204_control_toggle_scroll_mode    trackball_control_toggle_scroll_mode
#define paw3204_control_scroll_speed_up       trackball_control_scroll_speed_up
#define paw3204_control_scroll_speed_down     trackball_control_scroll_speed_down
#define paw3204_control_get_scroll_level      trackball_control_get_scroll_level
#define paw3204_control_get_scroll_div        trackball_control_get_scroll_div
#define paw3204_control_speed_up              trackball_control_speed_up
#define paw3204_control_speed_down            trackball_control_speed_down
#define paw3204_control_get_speed_level       trackball_control_get_speed_level
#define paw3204_control_toggle_automouse      trackball_control_toggle_automouse
#define paw3204_control_is_automouse_enabled  trackball_control_is_automouse_enabled
#define paw3204_control_automouse_time_up     trackball_control_automouse_time_up
#define paw3204_control_automouse_time_down   trackball_control_automouse_time_down
#define paw3204_control_automouse_time_reset  trackball_control_automouse_time_reset
#define paw3204_control_get_automouse_time    trackball_control_get_automouse_time
#define paw3204_control_toggle_acceleration   trackball_control_toggle_acceleration
#define paw3204_control_is_acceleration_enabled trackball_control_is_acceleration_enabled
#define paw3204_control_accel_up              trackball_control_accel_up
#define paw3204_control_accel_down            trackball_control_accel_down
#define paw3204_control_accel_reset           trackball_control_accel_reset
#define paw3204_control_get_accel_level       trackball_control_get_accel_level
#define paw3204_control_toggle_smoothing      trackball_control_toggle_smoothing
#define paw3204_control_is_smoothing_enabled  trackball_control_is_smoothing_enabled
#define paw3204_control_rotate_cw             trackball_control_rotate_cw
#define paw3204_control_rotate_ccw            trackball_control_rotate_ccw
#define paw3204_control_rotate_reset          trackball_control_rotate_reset
#define paw3204_control_get_rotation_angle    trackball_control_get_rotation_angle
#define paw3204_control_is_sniper_active      trackball_control_is_sniper_active

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_TRACKBALL_CONTROL_H_ */
