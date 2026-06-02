# Flight-Controller-to-WS2812

Drive WS2812 ("NeoPixel") LED strips from a flight-controller PWM channel on a
**Waveshare RP2040-Zero**. A 6-position switch on your RC transmitter selects
the colour/animation; the LEDs are clocked entirely by the RP2040's **PIO**
peripheral, so the timing is rock-solid and the CPU stays free.

- Five independent WS2812 strips (default: **GP29**, **GP28**, **GP27**, **GP26** and **GP15**)
- Mode selected by an RC/flight-controller **PWM** input (default: **GP2**)
- Per-position colour, animation and brightness configured **in code** — no
  serial console needed at runtime
- Works at PWM refresh rates from 50 Hz up to 500 Hz (decodes pulse *width*,
  which is independent of frequency)

## Hardware

| Signal            | RP2040-Zero pin | Notes                                   |
|-------------------|-----------------|-----------------------------------------|
| LED strip 1 data  | GP29            | bottom pad                              |
| LED strip 2 data  | GP28            | bottom pad                              |
| LED strip 3 data  | GP27            | bottom pad                              |
| LED strip 4 data  | GP26            | bottom pad                              |
| LED strip 5 data  | GP15            |                                         |
| FC PWM input      | GP2             | 6-position switch channel               |
| Ground            | GND             | **must** be common with the FC and LEDs |

Notes:
- **Common ground** between the RP2040, the flight controller and the LED power
  supply is required.
- GP2 is **not 5 V tolerant**. If your FC drives 5 V PWM, use a level shifter or
  a resistor divider.
- WS2812s want a ~5 V data line; the RP2040's 3.3 V output usually works for
  short strips, but add a level shifter / ~330 Ω series resistor if the first
  pixel misbehaves.
- Power the LEDs from a supply that can handle their current (≈60 mA per LED at
  full white) — not from the RP2040.

## Build & flash

This is a [PlatformIO](https://platformio.org/) project (Arduino /
[arduino-pico](https://github.com/earlephilhower/arduino-pico) core).

```bash
pio run             # build
pio run -t upload   # flash (hold BOOTSEL / double-tap RESET if needed)
pio device monitor  # 115200 — optional status log
```

## Configuration

Everything you tune lives at the top of [`src/main.cpp`](src/main.cpp).

**Wiring / strip length:**

```cpp
constexpr uint8_t  STRIP1_PIN = 29;
constexpr uint16_t STRIP1_LEN = 20;   // LED count of strip 1
constexpr uint8_t  STRIP2_PIN = 28;
constexpr uint16_t STRIP2_LEN = 80;   // LED count of strip 2
constexpr uint8_t  STRIP3_PIN = 27;
constexpr uint16_t STRIP3_LEN = 80;   // LED count of strip 3
constexpr uint8_t  STRIP4_PIN = 26;
constexpr uint16_t STRIP4_LEN = 72;   // LED count of strip 4
constexpr uint8_t  STRIP5_PIN = 15;
constexpr uint16_t STRIP5_LEN = 72;   // LED count of strip 5
constexpr uint8_t  PWM_PIN    = 2;
```

**Per-position behaviour** — one row per switch position (index `0` = no signal
/ failsafe, `1..6` = the six switch positions). Each strip has its own table:

```cpp
//                anim            color     flashColor  brightness
LedConfig strip1Cfg[7] = {
    /* 1 */ { ANIM_OFF,      0x000000, 0x000000,   0 },   // off
    /* 2 */ { ANIM_DBLFLASH, 0xFF0000, 0xFFFFFF, 255 },   // red + white double flash
    /* 3 */ { ANIM_DBLFLASH, 0x00FF00, 0xFFFFFF, 255 },   // green + double flash
    /* 4 */ { ANIM_DBLFLASH, 0x0000FF, 0xFFFFFF, 255 },   // blue + double flash
    /* 5 */ { ANIM_DBLFLASH, 0xFF00FF, 0xFFFFFF, 255 },   // magenta + double flash
    /* 6 */ { ANIM_RAINBOW,  0x000000, 0x000000, 255 },   // rainbow
};
```

### Animations

| Animation       | Description                                  | Uses           |
|-----------------|----------------------------------------------|----------------|
| `ANIM_OFF`      | all LEDs dark                                | –              |
| `ANIM_STATIC`   | solid colour                                 | `color`        |
| `ANIM_FLASH`    | base colour + single periodic flash          | `color`+`flashColor` |
| `ANIM_DBLFLASH` | base colour + double flash (strobe style)    | `color`+`flashColor` |
| `ANIM_BREATHE`  | base colour fading in and out                | `color`        |
| `ANIM_RAINBOW`  | rainbow sweeping along the strip             | –              |
| `ANIM_CHASE`    | theatre-chase running light                  | `color`        |
| `ANIM_WIPE`     | colour fills the strip one LED at a time     | `color`        |

Colours are `0xRRGGBB`. Flash/double-flash use `flashColor` as the blink colour
(set it to `0xFFFFFF` for white).

## Switch calibration

A 6-position switch outputs 6 evenly-spaced pulse widths across the
1000–2000 µs servo range, binned in `positionFromUs()`. Open the serial monitor
and flip the switch — each change prints e.g. `[PWM] 1598 us @ 498 Hz ->
position 4`. If your transmitter uses different values, adjust the thresholds in
`positionFromUs()`.

## Project layout

| File                                       | Purpose                                   |
|--------------------------------------------|-------------------------------------------|
| [`src/main.cpp`](src/main.cpp)             | configuration, animations, main loop      |
| [`src/WS2812Strip.h`](src/WS2812Strip.h) / [`.cpp`](src/WS2812Strip.cpp) | PIO-based WS2812 driver + colour helpers |
| [`src/ws2812.pio.h`](src/ws2812.pio.h)     | pre-assembled WS2812 PIO program          |
| [`src/RcPwm.h`](src/RcPwm.h)               | interrupt-driven RC PWM reader            |
