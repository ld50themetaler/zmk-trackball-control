/*
 * Generic Advanced Trackball Control Subsystem for ZMK
 * Copyright (c) 2024-2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>

#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>

#include "trackball_control.h"

#if IS_ENABLED(CONFIG_KUGEL_INDICATOR)
#include "kugel_indicator.h"
#endif

LOG_MODULE_REGISTER(trackball_control, LOG_LEVEL_INF);

// Default Configuration Constants (from Kconfig if defined)
#ifndef CONFIG_TRACKBALL_DEFAULT_SPEED_LEVEL
#define DEFAULT_SPEED_LEVEL 8        // Level 8 = Normal (1.00x)
#else
#define DEFAULT_SPEED_LEVEL CONFIG_TRACKBALL_DEFAULT_SPEED_LEVEL
#endif

#define MAX_SPEED_LEVEL 16

#ifndef CONFIG_TRACKBALL_DEFAULT_SCROLL_LEVEL
#define DEFAULT_SCROLL_LEVEL 3       // Level 3 = Normal (div 20)
#else
#define DEFAULT_SCROLL_LEVEL CONFIG_TRACKBALL_DEFAULT_SCROLL_LEVEL
#endif

#define MAX_SCROLL_LEVEL 6
#define DEFAULT_AUTOMOUSE_ENABLE 1   // Enabled by default
#define DEFAULT_AUTOMOUSE_TIMEOUT_MS 800
#define MIN_AUTOMOUSE_TIMEOUT_MS     200
#define MAX_AUTOMOUSE_TIMEOUT_MS     3000
#define AUTOMOUSE_TIMEOUT_STEP_MS    100
#define SNIPER_SPEED_DIV 6           // Ultra-fine precision divider

#ifndef CONFIG_TRACKBALL_DEFAULT_ROTATION_ANGLE
#define DEFAULT_ROTATION_ANGLE 0     // 0 deg default (no tilt)
#else
#define DEFAULT_ROTATION_ANGLE CONFIG_TRACKBALL_DEFAULT_ROTATION_ANGLE
#endif

#define MIN_ROTATION_ANGLE -180
#define MAX_ROTATION_ANGLE 180
#define ROTATION_STEP_DEG 10
#define ROTATION_SCALE 1024

struct speed_ratio {
    uint8_t num;
    uint8_t den;
};

// 16-Step Pointer Speed Table (Level 1: 2.50x to Level 16: 0.19x, Level 8: 1.00x default)
static const struct speed_ratio s_speed_table[MAX_SPEED_LEVEL] = {
    { 40, 16 }, // Level 1: 2.50x
    { 36, 16 }, // Level 2: 2.25x
    { 32, 16 }, // Level 3: 2.00x
    { 28, 16 }, // Level 4: 1.75x
    { 24, 16 }, // Level 5: 1.50x
    { 20, 16 }, // Level 6: 1.25x
    { 18, 16 }, // Level 7: 1.125x
    { 16, 16 }, // Level 8: 1.00x (Default / 1:1)
    { 14, 16 }, // Level 9: 0.875x
    { 12, 16 }, // Level 10: 0.75x
    { 10, 16 }, // Level 11: 0.625x
    {  8, 16 }, // Level 12: 0.50x
    {  6, 16 }, // Level 13: 0.375x
    {  5, 16 }, // Level 14: 0.3125x
    {  4, 16 }, // Level 15: 0.25x
    {  3, 16 }, // Level 16: 0.1875x
};

// Scroll Divisor Lookup Table (Level 1 to 6)
static const uint8_t s_scroll_div_table[MAX_SCROLL_LEVEL] = {
    36, // Level 1: Very Slow / Ultra Smooth
    28, // Level 2: Slow / Smooth
    20, // Level 3: Normal / Balanced (Default)
    15, // Level 4: Moderate / Responsive
    10, // Level 5: Fast
    6,  // Level 6: Ultra Fast (Legacy div 6)
};

struct rot_trig {
    int16_t cos_val;
    int16_t sin_val;
};

// 36-Step Fixed-Point (x1024) Trigonometric Table for 0 to 350 deg in 10-deg steps
static const struct rot_trig s_rot_table[36] = {
    /*   0 deg */ {  1024,     0 },
    /*  10 deg */ {  1008,   178 },
    /*  20 deg */ {   962,   350 },
    /*  30 deg */ {   887,   512 },
    /*  40 deg */ {   784,   658 },
    /*  50 deg */ {   658,   784 },
    /*  60 deg */ {   512,   887 },
    /*  70 deg */ {   350,   962 },
    /*  80 deg */ {   178,  1008 },
    /*  90 deg */ {     0,  1024 },
    /* 100 deg */ {  -178,  1008 },
    /* 110 deg */ {  -350,   962 },
    /* 120 deg */ {  -512,   887 },
    /* 130 deg */ {  -658,   784 },
    /* 140 deg */ {  -784,   658 },
    /* 150 deg */ {  -887,   512 },
    /* 160 deg */ {  -962,   350 },
    /* 170 deg */ { -1008,   178 },
    /* 180 deg */ { -1024,     0 },
    /* 190 deg */ { -1008,  -178 },
    /* 200 deg */ {  -962,  -350 },
    /* 210 deg */ {  -887,  -512 },
    /* 220 deg */ {  -784,  -658 },
    /* 230 deg */ {  -658,  -784 },
    /* 240 deg */ {  -512,  -887 },
    /* 250 deg */ {  -350,  -962 },
    /* 260 deg */ {  -178, -1008 },
    /* 270 deg */ {     0, -1024 },
    /* 280 deg */ {   178, -1008 },
    /* 290 deg */ {   350,  -962 },
    /* 300 deg */ {   512,  -887 },
    /* 310 deg */ {   658,  -784 },
    /* 320 deg */ {   784,  -658 },
    /* 330 deg */ {   887,  -512 },
    /* 340 deg */ {   962,  -350 },
    /* 350 deg */ {  1008,  -178 },
};

#define MIN_ACCEL_LEVEL     1
#define MAX_ACCEL_LEVEL     5
#define DEFAULT_ACCEL_LEVEL 3

struct tb_accel_profile {
    int k_coeff;    // Curve multiplier in 1/1000th: 10, 20, 30, 45, 60
    int max_factor; // Cap factor in 1/1000th: 2000, 2750, 3500, 4500, 6000
};

static const struct tb_accel_profile s_accel_table[MAX_ACCEL_LEVEL] = {
    { .k_coeff = 10, .max_factor = 2000 }, // Level 1: Gentle (0.010, 2.00x cap)
    { .k_coeff = 20, .max_factor = 2750 }, // Level 2: Mild   (0.020, 2.75x cap)
    { .k_coeff = 30, .max_factor = 3500 }, // Level 3: Normal (0.030, 3.50x cap) [Default]
    { .k_coeff = 45, .max_factor = 4500 }, // Level 4: Swift  (0.045, 4.50x cap)
    { .k_coeff = 60, .max_factor = 6000 }, // Level 5: Ultra  (0.060, 6.00x cap)
};

struct tb_control_state {
    uint8_t speed_level;
    uint8_t scroll_level;
    uint8_t accel_level;
    int16_t rotation_angle;
    uint16_t automouse_timeout_ms;
    bool automouse_enabled;
    bool automouse_active;
    bool sniper_active;
    bool scroll_mode;
    bool scroll_axis_lock;
    bool acceleration_enabled;
    bool smoothing_enabled;
    struct k_work_delayable automouse_timeout_work;
    struct k_work_delayable settings_save_work;
};

static struct tb_control_state g_tb = {
    .speed_level = DEFAULT_SPEED_LEVEL,
    .scroll_level = DEFAULT_SCROLL_LEVEL,
    .accel_level = DEFAULT_ACCEL_LEVEL,
    .rotation_angle = DEFAULT_ROTATION_ANGLE,
    .automouse_timeout_ms = DEFAULT_AUTOMOUSE_TIMEOUT_MS,
    .automouse_enabled = false,
    .automouse_active = false,
    .sniper_active = false,
    .scroll_mode = false,
    .scroll_axis_lock = true,
    .acceleration_enabled = true,
    .smoothing_enabled = false,
};

#define MOTION_SCALE 1000

static int s_rot_x_accum = 0;
static int s_rot_y_accum = 0;
static int s_motion_x_accum = 0;
static int s_motion_y_accum = 0;
static int s_ema_x = 0;
static int s_ema_y = 0;

/* --- Settings (NVS) Persistence --- */
#if IS_ENABLED(CONFIG_SETTINGS)

static int tb_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
    const char *next;
    int rc;

    if (settings_name_steq(name, "speed", &next) && !next) {
        if (len != sizeof(g_tb.speed_level)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &g_tb.speed_level, sizeof(g_tb.speed_level));
        if (rc >= 0) {
            if (g_tb.speed_level < 1 || g_tb.speed_level > MAX_SPEED_LEVEL) {
                g_tb.speed_level = DEFAULT_SPEED_LEVEL;
            }
            LOG_INF("Loaded tb/speed: %d", g_tb.speed_level);
            return 0;
        }
        return rc;
    }

    if (settings_name_steq(name, "scrl", &next) && !next) {
        if (len != sizeof(g_tb.scroll_level)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &g_tb.scroll_level, sizeof(g_tb.scroll_level));
        if (rc >= 0) {
            if (g_tb.scroll_level < 1 || g_tb.scroll_level > MAX_SCROLL_LEVEL) {
                g_tb.scroll_level = DEFAULT_SCROLL_LEVEL;
            }
            LOG_INF("Loaded tb/scrl: %d (div %d)", g_tb.scroll_level, s_scroll_div_table[g_tb.scroll_level - 1]);
            return 0;
        }
        return rc;
    }

    if (settings_name_steq(name, "am_en", &next) && !next) {
        uint8_t val = 0;
        if (len != sizeof(val)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &val, sizeof(val));
        if (rc >= 0) {
            g_tb.automouse_enabled = (val != 0);
            LOG_INF("Loaded tb/am_en: %d", g_tb.automouse_enabled);
            return 0;
        }
        return rc;
    }

    if (settings_name_steq(name, "accel", &next) && !next) {
        uint8_t val = 0;
        if (len != sizeof(val)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &val, sizeof(val));
        if (rc >= 0) {
            g_tb.acceleration_enabled = (val != 0);
            LOG_INF("Loaded tb/accel: %d", g_tb.acceleration_enabled);
            return 0;
        }
        return rc;
    }

    if (settings_name_steq(name, "accel_lvl", &next) && !next) {
        if (len != sizeof(g_tb.accel_level)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &g_tb.accel_level, sizeof(g_tb.accel_level));
        if (rc >= 0) {
            if (g_tb.accel_level < MIN_ACCEL_LEVEL || g_tb.accel_level > MAX_ACCEL_LEVEL) {
                g_tb.accel_level = DEFAULT_ACCEL_LEVEL;
            }
            LOG_INF("Loaded tb/accel_lvl: %d", g_tb.accel_level);
            return 0;
        }
        return rc;
    }

    if (settings_name_steq(name, "rot", &next) && !next) {
        if (len != sizeof(g_tb.rotation_angle)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &g_tb.rotation_angle, sizeof(g_tb.rotation_angle));
        if (rc >= 0) {
            if (g_tb.rotation_angle < MIN_ROTATION_ANGLE || g_tb.rotation_angle > MAX_ROTATION_ANGLE || (g_tb.rotation_angle % ROTATION_STEP_DEG != 0)) {
                g_tb.rotation_angle = DEFAULT_ROTATION_ANGLE;
            }
            LOG_INF("Loaded tb/rot: %d deg", g_tb.rotation_angle);
            return 0;
        }
        return rc;
    }

    if (settings_name_steq(name, "am_time", &next) && !next) {
        if (len != sizeof(g_tb.automouse_timeout_ms)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &g_tb.automouse_timeout_ms, sizeof(g_tb.automouse_timeout_ms));
        if (rc >= 0) {
            if (g_tb.automouse_timeout_ms < MIN_AUTOMOUSE_TIMEOUT_MS || g_tb.automouse_timeout_ms > MAX_AUTOMOUSE_TIMEOUT_MS) {
                g_tb.automouse_timeout_ms = DEFAULT_AUTOMOUSE_TIMEOUT_MS;
            }
            LOG_INF("Loaded tb/am_time: %d ms", g_tb.automouse_timeout_ms);
            return 0;
        }
        return rc;
    }

    if (settings_name_steq(name, "smth", &next) && !next) {
        uint8_t val = 0;
        if (len != sizeof(val)) {
            return -EINVAL;
        }
        rc = read_cb(cb_arg, &val, sizeof(val));
        if (rc >= 0) {
            g_tb.smoothing_enabled = (val != 0);
            LOG_INF("Loaded tb/smth: %d", g_tb.smoothing_enabled);
            return 0;
        }
        return rc;
    }

    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(tb, "tb", NULL, tb_settings_set, NULL, NULL);

static void settings_save_work_handler(struct k_work *work)
{
    uint8_t am_val = g_tb.automouse_enabled ? 1 : 0;
    uint8_t accel_val = g_tb.acceleration_enabled ? 1 : 0;
    uint8_t smth_val = g_tb.smoothing_enabled ? 1 : 0;
    settings_save_one("tb/speed", &g_tb.speed_level, sizeof(g_tb.speed_level));
    settings_save_one("tb/scrl", &g_tb.scroll_level, sizeof(g_tb.scroll_level));
    settings_save_one("tb/am_en", &am_val, sizeof(am_val));
    settings_save_one("tb/accel", &accel_val, sizeof(accel_val));
    settings_save_one("tb/accel_lvl", &g_tb.accel_level, sizeof(g_tb.accel_level));
    settings_save_one("tb/rot", &g_tb.rotation_angle, sizeof(g_tb.rotation_angle));
    settings_save_one("tb/am_time", &g_tb.automouse_timeout_ms, sizeof(g_tb.automouse_timeout_ms));
    settings_save_one("tb/smth", &smth_val, sizeof(smth_val));
    LOG_INF("Trackball settings saved to NVS (speed=%d, scrl=%d, am_en=%d, accel=%d, smth=%d, rot=%d deg)",
            g_tb.speed_level, g_tb.scroll_level, am_val, accel_val, smth_val, g_tb.rotation_angle);
}

static void schedule_settings_save(void)
{
    k_work_reschedule(&g_tb.settings_save_work, K_MSEC(1000));
}

#else
static void schedule_settings_save(void) {}
#endif /* IS_ENABLED(CONFIG_SETTINGS) */

/* --- Auto-Mouse Layer Logic --- */
static void automouse_timeout_handler(struct k_work *work)
{
    if (g_tb.automouse_active) {
        g_tb.automouse_active = false;
        if (zmk_keymap_layer_active(TRACKBALL_MOUSE_LAYER_ID)) {
            zmk_keymap_layer_deactivate(TRACKBALL_MOUSE_LAYER_ID, false);
            LOG_INF("Auto-Mouse: layer %d deactivated due to timeout", TRACKBALL_MOUSE_LAYER_ID);
        }
    }
}

void trackball_control_on_motion(int8_t dx, int8_t dy)
{
    if (!g_tb.automouse_enabled) {
        return;
    }

    if (!g_tb.automouse_active) {
        g_tb.automouse_active = true;
        if (!zmk_keymap_layer_active(TRACKBALL_MOUSE_LAYER_ID)) {
            zmk_keymap_layer_activate(TRACKBALL_MOUSE_LAYER_ID, false);
            LOG_INF("Auto-Mouse: layer %d activated on motion", TRACKBALL_MOUSE_LAYER_ID);
        }
    }

    k_work_reschedule(&g_tb.automouse_timeout_work, K_MSEC(g_tb.automouse_timeout_ms));
}

/* --- Pointer Speed & Motion Calculation --- */
void trackball_control_calculate_motion(int dx, int dy, int *out_dx, int *out_dy)
{
    if (dx == 0 && dy == 0) {
        *out_dx = 0;
        *out_dy = 0;
        return;
    }

    // Direction reversal detection: clear residual fractional accumulator to prevent overshoot
    if ((dx > 0 && s_motion_x_accum < 0) || (dx < 0 && s_motion_x_accum > 0)) {
        s_motion_x_accum = 0;
    }
    if ((dy > 0 && s_motion_y_accum < 0) || (dy < 0 && s_motion_y_accum > 0)) {
        s_motion_y_accum = 0;
    }

    // Sniper Mode: ultra-fine precision with fractional accumulation
    if (g_tb.sniper_active) {
        s_motion_x_accum += (dx * MOTION_SCALE) / SNIPER_SPEED_DIV;
        s_motion_y_accum += (dy * MOTION_SCALE) / SNIPER_SPEED_DIV;

        int f_dx = s_motion_x_accum / MOTION_SCALE;
        int f_dy = s_motion_y_accum / MOTION_SCALE;
        s_motion_x_accum -= f_dx * MOTION_SCALE;
        s_motion_y_accum -= f_dy * MOTION_SCALE;

        *out_dx = f_dx;
        *out_dy = f_dy;
        return;
    }

    // Optional Soft Smoothing (EMA Filter) for low-speed micro-jitter suppression
    if (g_tb.smoothing_enabled) {
        int v_raw = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
        if (v_raw <= 4) {
            s_ema_x = (dx * 750) + (s_ema_x * 250 / 1000);
            s_ema_y = (dy * 750) + (s_ema_y * 250 / 1000);
        } else {
            s_ema_x = dx * 1000;
            s_ema_y = dy * 1000;
        }
    }

    uint8_t lvl = g_tb.speed_level;
    if (lvl < 1) lvl = 1;
    if (lvl > MAX_SPEED_LEVEL) lvl = MAX_SPEED_LEVEL;

    int num = s_speed_table[lvl - 1].num;
    int den = s_speed_table[lvl - 1].den;

    int factor = 1000;
    if (g_tb.acceleration_enabled) {
        uint8_t a_lvl = g_tb.accel_level;
        if (a_lvl < MIN_ACCEL_LEVEL || a_lvl > MAX_ACCEL_LEVEL) {
            a_lvl = DEFAULT_ACCEL_LEVEL;
        }
        const struct tb_accel_profile *prof = &s_accel_table[a_lvl - 1];

        int v = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
        if (v <= 1) {
            factor = 600;  // 0.60x micro-precision
        } else if (v == 2) {
            factor = 800;  // 0.80x micro-precision
        } else {
            int diff = v - 3;
            factor = 1000 + prof->k_coeff * diff * diff;
            if (factor > prof->max_factor) {
                factor = prof->max_factor;
            }
        }
    }

    int delta_x_scaled;
    int delta_y_scaled;

    if (g_tb.smoothing_enabled && (abs(dx) <= 4 && abs(dy) <= 4)) {
        delta_x_scaled = (s_ema_x * num / den * factor) / 1000;
        delta_y_scaled = (s_ema_y * num / den * factor) / 1000;
    } else {
        delta_x_scaled = (dx * num * factor) / den;
        delta_y_scaled = (dy * num * factor) / den;
    }

    s_motion_x_accum += delta_x_scaled;
    s_motion_y_accum += delta_y_scaled;

    int f_dx = s_motion_x_accum / MOTION_SCALE;
    int f_dy = s_motion_y_accum / MOTION_SCALE;

    s_motion_x_accum -= f_dx * MOTION_SCALE;
    s_motion_y_accum -= f_dy * MOTION_SCALE;

    *out_dx = f_dx;
    *out_dy = f_dy;
}

void trackball_control_clear_accum(void)
{
    s_motion_x_accum = 0;
    s_motion_y_accum = 0;
    s_ema_x = 0;
    s_ema_y = 0;
}

int trackball_control_get_scroll_div(void)
{
    uint8_t lvl = g_tb.scroll_level;
    if (lvl < 1) lvl = 1;
    if (lvl > MAX_SCROLL_LEVEL) lvl = MAX_SCROLL_LEVEL;
    return s_scroll_div_table[lvl - 1];
}

bool trackball_control_is_sniper_active(void)
{
    return g_tb.sniper_active;
}

bool trackball_control_is_scroll_mode(void)
{
    return g_tb.scroll_mode;
}

void trackball_control_set_scroll_mode(bool enable)
{
    g_tb.scroll_mode = enable;
}

void trackball_control_toggle_scroll_mode(void)
{
    g_tb.scroll_mode = !g_tb.scroll_mode;
    LOG_INF("Scroll mode toggled: %d", g_tb.scroll_mode);
}

void trackball_control_speed_up(void)
{
    if (g_tb.speed_level > 1) {
        g_tb.speed_level--;
        LOG_INF("Trackball speed increased -> Level %d", g_tb.speed_level);
        schedule_settings_save();
    }
}

void trackball_control_speed_down(void)
{
    if (g_tb.speed_level < MAX_SPEED_LEVEL) {
        g_tb.speed_level++;
        LOG_INF("Trackball speed decreased -> Level %d", g_tb.speed_level);
        schedule_settings_save();
    }
}

uint8_t trackball_control_get_speed_level(void)
{
    return g_tb.speed_level;
}

void trackball_control_scroll_speed_up(void)
{
    if (g_tb.scroll_level < MAX_SCROLL_LEVEL) {
        g_tb.scroll_level++;
        LOG_INF("Scroll sensitivity increased -> Level %d (div %d)",
                g_tb.scroll_level, s_scroll_div_table[g_tb.scroll_level - 1]);
        schedule_settings_save();
    }
}

void trackball_control_scroll_speed_down(void)
{
    if (g_tb.scroll_level > 1) {
        g_tb.scroll_level--;
        LOG_INF("Scroll sensitivity decreased -> Level %d (div %d)",
                g_tb.scroll_level, s_scroll_div_table[g_tb.scroll_level - 1]);
        schedule_settings_save();
    }
}

uint8_t trackball_control_get_scroll_level(void)
{
    return g_tb.scroll_level;
}

void trackball_control_toggle_automouse(void)
{
    g_tb.automouse_enabled = !g_tb.automouse_enabled;
    if (!g_tb.automouse_enabled && g_tb.automouse_active) {
        g_tb.automouse_active = false;
        k_work_cancel_delayable(&g_tb.automouse_timeout_work);
        if (zmk_keymap_layer_active(TRACKBALL_MOUSE_LAYER_ID)) {
            zmk_keymap_layer_deactivate(TRACKBALL_MOUSE_LAYER_ID, false);
        }
    }
    LOG_INF("Auto-Mouse toggled: %d", g_tb.automouse_enabled);
    schedule_settings_save();
}

bool trackball_control_is_automouse_enabled(void)
{
    return g_tb.automouse_enabled;
}

void trackball_control_automouse_time_up(void)
{
    if (g_tb.automouse_timeout_ms < MAX_AUTOMOUSE_TIMEOUT_MS) {
        g_tb.automouse_timeout_ms += AUTOMOUSE_TIMEOUT_STEP_MS;
        LOG_INF("Auto-Mouse timeout increased -> %d ms", g_tb.automouse_timeout_ms);
        schedule_settings_save();
    }
}

void trackball_control_automouse_time_down(void)
{
    if (g_tb.automouse_timeout_ms > MIN_AUTOMOUSE_TIMEOUT_MS) {
        g_tb.automouse_timeout_ms -= AUTOMOUSE_TIMEOUT_STEP_MS;
        LOG_INF("Auto-Mouse timeout decreased -> %d ms", g_tb.automouse_timeout_ms);
        schedule_settings_save();
    }
}

void trackball_control_automouse_time_reset(void)
{
    g_tb.automouse_timeout_ms = DEFAULT_AUTOMOUSE_TIMEOUT_MS;
    LOG_INF("Auto-Mouse timeout RESET -> %d ms", g_tb.automouse_timeout_ms);
    schedule_settings_save();
}

uint16_t trackball_control_get_automouse_time(void)
{
    return g_tb.automouse_timeout_ms;
}

void trackball_control_toggle_acceleration(void)
{
    g_tb.acceleration_enabled = !g_tb.acceleration_enabled;
    LOG_INF("Trackball acceleration toggled: %d", g_tb.acceleration_enabled);
    schedule_settings_save();
}

bool trackball_control_is_acceleration_enabled(void)
{
    return g_tb.acceleration_enabled;
}

void trackball_control_accel_up(void)
{
    g_tb.acceleration_enabled = true;
    if (g_tb.accel_level < MAX_ACCEL_LEVEL) {
        g_tb.accel_level++;
    }
    LOG_INF("Trackball acceleration level up -> %d", g_tb.accel_level);
    schedule_settings_save();
}

void trackball_control_accel_down(void)
{
    g_tb.acceleration_enabled = true;
    if (g_tb.accel_level > MIN_ACCEL_LEVEL) {
        g_tb.accel_level--;
    }
    LOG_INF("Trackball acceleration level down -> %d", g_tb.accel_level);
    schedule_settings_save();
}

void trackball_control_accel_reset(void)
{
    g_tb.accel_level = DEFAULT_ACCEL_LEVEL;
    g_tb.acceleration_enabled = true;
    LOG_INF("Trackball acceleration level RESET -> %d", g_tb.accel_level);
    schedule_settings_save();
}

uint8_t trackball_control_get_accel_level(void)
{
    return g_tb.accel_level;
}

/* --- Dynamic Soft Smoothing (EMA Filter) Control --- */
void trackball_control_toggle_smoothing(void)
{
    g_tb.smoothing_enabled = !g_tb.smoothing_enabled;
    s_ema_x = 0;
    s_ema_y = 0;
    s_motion_x_accum = 0;
    s_motion_y_accum = 0;
    LOG_INF("Trackball smoothing toggled -> %s", g_tb.smoothing_enabled ? "ON" : "OFF");
#if IS_ENABLED(CONFIG_KUGEL_INDICATOR)
    kugel_indicator_show_smoothing(g_tb.smoothing_enabled);
#endif
    schedule_settings_save();
}

bool trackball_control_is_smoothing_enabled(void)
{
    return g_tb.smoothing_enabled;
}

/* --- Dynamic Rotation Angle Control --- */
void trackball_control_rotate_cw(void)
{
    if (g_tb.rotation_angle < MAX_ROTATION_ANGLE) {
        g_tb.rotation_angle += ROTATION_STEP_DEG;
        s_rot_x_accum = 0;
        s_rot_y_accum = 0;
        LOG_INF("Trackball rotation angle -> %d deg (CW)", g_tb.rotation_angle);
        schedule_settings_save();
    }
}

void trackball_control_rotate_ccw(void)
{
    if (g_tb.rotation_angle > MIN_ROTATION_ANGLE) {
        g_tb.rotation_angle -= ROTATION_STEP_DEG;
        s_rot_x_accum = 0;
        s_rot_y_accum = 0;
        LOG_INF("Trackball rotation angle -> %d deg (CCW)", g_tb.rotation_angle);
        schedule_settings_save();
    }
}

void trackball_control_rotate_reset(void)
{
    g_tb.rotation_angle = DEFAULT_ROTATION_ANGLE;
    s_rot_x_accum = 0;
    s_rot_y_accum = 0;
    LOG_INF("Trackball rotation angle RESET -> %d deg", DEFAULT_ROTATION_ANGLE);
    schedule_settings_save();
}

int16_t trackball_control_get_rotation_angle(void)
{
    return g_tb.rotation_angle;
}

void trackball_control_rotate_motion(int in_dx, int in_dy, int *out_dx, int *out_dy)
{
    int angle = g_tb.rotation_angle;
    if (angle == 0) {
        *out_dx = in_dx;
        *out_dy = in_dy;
        return;
    }

    int norm = angle % 360;
    if (norm < 0) {
        norm += 360;
    }
    int idx = (norm / 10) % 36;
    int c = s_rot_table[idx].cos_val;
    int s = s_rot_table[idx].sin_val;

    int rot_x = in_dx * c - in_dy * s;
    int rot_y = in_dx * s + in_dy * c;

    s_rot_x_accum += rot_x;
    s_rot_y_accum += rot_y;

    int res_dx = (s_rot_x_accum + (s_rot_x_accum >= 0 ? ROTATION_SCALE / 2 : -ROTATION_SCALE / 2)) / ROTATION_SCALE;
    int res_dy = (s_rot_y_accum + (s_rot_y_accum >= 0 ? ROTATION_SCALE / 2 : -ROTATION_SCALE / 2)) / ROTATION_SCALE;

    s_rot_x_accum -= res_dx * ROTATION_SCALE;
    s_rot_y_accum -= res_dy * ROTATION_SCALE;

    *out_dx = res_dx;
    *out_dy = res_dy;
}

/* --- Event Handlers (Layer & Keypress Listeners) --- */

static int layer_state_listener(const zmk_event_t *eh)
{
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (ev->layer == TRACKBALL_SNIPE_LAYER_ID) {
        g_tb.sniper_active = ev->state;
        LOG_INF("Sniper mode %s", g_tb.sniper_active ? "ACTIVATED" : "DEACTIVATED");
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(tb_layer_listener, layer_state_listener);
ZMK_SUBSCRIPTION(tb_layer_listener, zmk_layer_state_changed);

static int position_state_listener(const zmk_event_t *eh)
{
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    // Dismiss auto-mouse layer immediately when a typing key is pressed
    if (g_tb.automouse_active) {
        // Exclude thumb key positions (default thumbs start at 36)
        if (ev->position < 36) {
            g_tb.automouse_active = false;
            k_work_cancel_delayable(&g_tb.automouse_timeout_work);
            if (zmk_keymap_layer_active(TRACKBALL_MOUSE_LAYER_ID)) {
                zmk_keymap_layer_deactivate(TRACKBALL_MOUSE_LAYER_ID, false);
                LOG_INF("Auto-Mouse: dismissed on typing key position %d", ev->position);
            }
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(tb_pos_listener, position_state_listener);
ZMK_SUBSCRIPTION(tb_pos_listener, zmk_position_state_changed);

/* --- Subsystem Initialization --- */
void trackball_control_init(void)
{
    k_work_init_delayable(&g_tb.automouse_timeout_work, automouse_timeout_handler);
#if IS_ENABLED(CONFIG_SETTINGS)
    k_work_init_delayable(&g_tb.settings_save_work, settings_save_work_handler);
#endif
    LOG_INF("Generic Advanced Trackball Control Subsystem initialized (MouseLayer=%d, SnipeLayer=%d)",
            TRACKBALL_MOUSE_LAYER_ID, TRACKBALL_SNIPE_LAYER_ID);
}
