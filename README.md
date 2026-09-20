# ZMK Trackball Control Module (`zmk-trackball-control`)

A generic, sensor-agnostic advanced trackball control module for **ZMK Firmware**.  
Compatible with **PMW3610** (torabo-tsuki, Lotom, Keyball, Cocot), **PAW3204** (Bit Trade One ADTB7M, Kugel-1), and any other pointing device.

---

## Features

- 🎯 **Fractional Accumulator (Sub-Pixel Precision)**:
  - Eliminates quantization truncation noise and forced 1-count jumps.
  - Carries fractional remainders across polling cycles for ultra-smooth, linear response even at micro-speeds (0.60x) and sniper mode.
  - Automatic direction reversal detection to clear residual overshoot.
- ⚡ **Continuous Quadratic Acceleration**:
  - 5 configurable acceleration profiles (Gentle to Ultra).
  - Micro-speed precision (0.6x at 1 count, 0.8x at 2 counts) combined with smooth high-speed boost.
- 📐 **Dynamic Rotation Angle Adjustment (10-degree steps)**:
  - Compensates for natural hand tilt when reaching for center/thumb trackballs.
  - Runtime adjustable via keymap (`TB_ROT_CW`, `TB_ROT_CCW`, `TB_ROT_RES`).
- 🌊 **Soft Smoothing (EMA Filter)**:
  - Exponential Moving Average filter to eliminate physical ball micro-jitter at low speeds.
  - Toggleable on the fly via keymap (`TB_SMOOTH_TOG`) with zero latency bypass at high speeds.
- 📜 **Momentary Scroll with Axis Lock**:
  - Smooth vertical/horizontal wheel generation.
  - Directional axis lock prevents accidental horizontal drift during vertical scrolling.
  - 6-level scroll sensitivity adjustment.
- 🐭 **Auto-Mouse Layer**:
  - Automatically activates the mouse layer upon physical motion.
  - Configurable timeout (200ms - 3000ms).
  - Instant dismissal when normal typing keys are pressed.
- 💾 **NVS Settings Persistence**:
  - Speed level, scroll level, rotation angle, acceleration, and smoothing settings are saved to flash and persist across reboots.

---

## Installation via `west.yml`

Add this module to your keyboard repository's `config/west.yml`:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: ld50themetaler
      url-base: https://github.com/ld50themetaler
  projects:
    - name: zmk
      remote: zmkfirmware
      ...
    # Add this project:
    - name: zmk-trackball-control
      remote: ld50themetaler
      revision: main
```

---

## Configuration (`<keyboard>.conf`)

```ini
# Enable Trackball Control Subsystem
CONFIG_TRACKBALL_CONTROL=y

# Optional: Customize Mouse Layer ID for your keymap (Default: 4)
CONFIG_TRACKBALL_MOUSE_LAYER_ID=4

# Optional: Customize Snipe Layer ID (Default: 5)
CONFIG_TRACKBALL_SNIPE_LAYER_ID=5

# Optional: Default pointer speed (1 to 16, 8 = 1.00x default)
CONFIG_TRACKBALL_DEFAULT_SPEED_LEVEL=8

# Optional: Default scroll sensitivity (1 to 6, 3 = normal default)
CONFIG_TRACKBALL_DEFAULT_SCROLL_LEVEL=3

# Optional: Default rotation angle in degrees (Default: 0)
CONFIG_TRACKBALL_DEFAULT_ROTATION_ANGLE=0
```

---

## Keymap Usage

Include the header in your `.keymap` file:

```dts
#include <dt-bindings/zmk/trackball.h>

/ {
    behaviors {
        tb: behavior_trackball {
            compatible = "zmk,behavior-trackball";
            #binding-cells = <1>;
        };
    };
};
```

### Available Behaviors:

| Keycode | Description |
| :--- | :--- |
| `&tb TB_SPD_UP` / `&tb TB_SPD_DN` | Pointer speed up / down (16 levels) |
| `&tb TB_SCRL_UP` / `&tb TB_SCRL_DN` | Scroll sensitivity up / down (6 levels) |
| `&tb TB_SCRL_TOG` / `&tb TB_SCRL_MO` | Momentary scroll mode (hold for scroll) |
| `&tb TB_ACCEL_TOG` | Toggle acceleration ON / OFF |
| `&tb TB_ACCEL_UP` / `&tb TB_ACCEL_DN` | Change acceleration profile (1-5) |
| `&tb TB_SMOOTH_TOG` | Toggle soft smoothing (EMA filter) ON / OFF |
| `&tb TB_ROT_CW` / `&tb TB_ROT_CCW` | Rotate angle by +10° / -10° |
| `&tb TB_ROT_RES` | Reset rotation angle to 0° |
| `&tb TB_AM_TOG` | Toggle Auto-Mouse layer ON / OFF |
| `&tb TB_AM_TIME_UP` / `DN` / `RES` | Auto-Mouse timeout adjust (+100ms / -100ms / reset) |

---

## Driver Integration Example (e.g. PMW3610 / PAW3204)

In your sensor driver:

```c
#include <trackball_control.h>

void on_sensor_data(int raw_dx, int raw_dy) {
    trackball_control_on_motion(raw_dx, raw_dy);

    int rot_dx = 0, rot_dy = 0;
    trackball_control_rotate_motion(raw_dx, raw_dy, &rot_dx, &rot_dy);

    if (trackball_control_is_scroll_mode()) {
        // Handle scroll accumulation with trackball_control_get_scroll_div()
    } else {
        int final_dx = 0, final_dy = 0;
        trackball_control_calculate_motion(rot_dx, rot_dy, &final_dx, &final_dy);

        if (final_dx != 0 || final_dy != 0) {
            input_report_rel(dev, INPUT_REL_X, final_dx, false, K_NO_WAIT);
            input_report_rel(dev, INPUT_REL_Y, final_dy, true, K_NO_WAIT);
        }
    }
}
```

---

## License

MIT License - see [LICENSE](LICENSE) file.
