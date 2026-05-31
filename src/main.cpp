// -----------------------------------------------------------------------------
// WS2812 driver - Waveshare RP2040-Zero
//
// Two WS2812 strips driven by the RP2040 PIO peripheral:
//     strip 1 -> GP29
//     strip 2 -> GP28
//
// A flight-controller PWM channel on GP2 (6-position switch) selects the mode.
// What each strip shows for each switch position - colour, animation and
// brightness - is configured in the tables below, in code (no serial control).
//
// To customise: edit STRIP1 / STRIP2 / the two config tables. That's it.
// -----------------------------------------------------------------------------
#include <Arduino.h>
#include "WS2812Strip.h"
#include "RcPwm.h"

// ============================ HARDWARE WIRING ================================
constexpr uint8_t  STRIP1_PIN = 29;   // GP29
constexpr uint16_t STRIP1_LEN = 20;

constexpr uint8_t  STRIP2_PIN = 28;   // GP28
constexpr uint16_t STRIP2_LEN = 80;    // <-- set to your 2nd strip's LED count

constexpr uint8_t  PWM_PIN    = 2;    // GP2 <- PWM from flight controller

WS2812Strip strip1(STRIP1_PIN, STRIP1_LEN);
WS2812Strip strip2(STRIP2_PIN, STRIP2_LEN);
RcPwm       rc;

// ============================ ANIMATIONS ====================================
enum Animation : uint8_t {
    ANIM_OFF = 0,   // all LEDs dark
    ANIM_STATIC,    // solid colour
    ANIM_FLASH,     // base colour + single periodic flash
    ANIM_DBLFLASH,  // base colour + white double-flash (strobe style)
    ANIM_BREATHE,   // base colour fading in and out
    ANIM_RAINBOW,   // rainbow sweeping along the strip
    ANIM_CHASE,     // theatre-chase running light
    ANIM_WIPE,      // colour fills the strip one LED at a time
};

struct LedConfig {
    Animation anim;
    uint32_t  color;        // base colour 0xRRGGBB (used by most animations)
    uint32_t  flashColor;   // 2nd colour for ANIM_FLASH
    uint8_t   brightness;   // 0..255 (WS2812 are bright - 60..120 is usually plenty)
};

// ======================= USER CONFIGURATION (edit here) =====================
// One row per switch position. Index 0 = no signal / failsafe, 1..6 = the
// six switch positions. Each strip has its own table so they can differ.
//
//                anim          color      flashColor  brightness
LedConfig strip1Cfg[7] = {
    /* 0 no sig */ { ANIM_OFF,    0x000000, 0x000000,   0 },
    /* 1        */ { ANIM_OFF,    0x000000, 0x000000,   0 },   // all LEDs off
    /* 2        */ { ANIM_DBLFLASH, 0xFF0000, 0xFFFFFF,  255 },   // red
    /* 3        */ { ANIM_DBLFLASH, 0x00FF00, 0xFFFFFF,  255 },   // green
    /* 4        */ { ANIM_DBLFLASH, 0x0000FF, 0xFFFFFF,  255 },   // blue
    /* 5        */ { ANIM_DBLFLASH, 0xFF00FF, 0xFFFFFF,  255 },   // magenta
    /* 6        */ { ANIM_RAINBOW,0x000000, 0x000000,  255 },   // rainbow
};

LedConfig strip2Cfg[7] = {
    /* 0 no sig */ { ANIM_OFF,    0x000000, 0x000000,   0 },
    /* 1        */ { ANIM_OFF,    0x000000, 0x000000,   0 },   // all LEDs off
    /* 2        */ { ANIM_DBLFLASH, 0xFF0000, 0xFFFFFF,  255 },   // red
    /* 3        */ { ANIM_DBLFLASH, 0x00FF00, 0xFFFFFF,  255 },   // green
    /* 4        */ { ANIM_DBLFLASH, 0x0000FF, 0xFFFFFF,  255 },   // blue
    /* 5        */ { ANIM_DBLFLASH, 0xFF00FF, 0xFFFFFF,  255 },   // magenta
    /* 6        */ { ANIM_RAINBOW,0x000000, 0x000000,  255 },   // rainbow
};
// ============================================================================

// Flash timing (shared by ANIM_FLASH).
constexpr uint16_t FLASH_PERIOD = 1000;  // one flash per second
constexpr uint16_t FLASH_WIDTH  = 90;    // flash on for 90 ms

// Double-flash timing (ANIM_DBLFLASH): two short pulses, then a longer pause.
constexpr uint16_t DF_PERIOD = 1400;     // whole cycle length
constexpr uint16_t DF_WIDTH  = 55;       // each flash on-time
constexpr uint16_t DF_GAP    = 200;      // start of 2nd flash (from cycle start)

// Render one animation frame for `s` using config `c`.
void renderStrip(WS2812Strip &s, const LedConfig &c) {
    const uint16_t n = s.count();

    switch (c.anim) {
        case ANIM_OFF:
            s.clear();
            break;

        case ANIM_STATIC:
            s.fill(c.color);
            break;

        case ANIM_FLASH: {
            bool on = (millis() % FLASH_PERIOD) < FLASH_WIDTH;
            s.fill(on ? c.flashColor : c.color);
            break;
        }

        case ANIM_DBLFLASH: {
            uint32_t t = millis() % DF_PERIOD;
            bool on = (t < DF_WIDTH) ||                          // 1st flash
                      (t >= DF_GAP && t < DF_GAP + DF_WIDTH);    // 2nd flash
            s.fill(on ? c.flashColor : c.color);
            break;
        }

        case ANIM_BREATHE: {
            float phase = (millis() % 3000) / 3000.0f;       // 0..1
            float tri   = 1.0f - fabsf(phase * 2.0f - 1.0f); // 0..1..0
            uint8_t v   = uint8_t(tri * tri * 255.0f);
            s.fill(WS2812Strip::scale(c.color, v));
            break;
        }

        case ANIM_RAINBOW: {
            uint8_t base = (millis() / 8) & 0xFF;            // sweep speed
            for (uint16_t i = 0; i < n; i++)
                s.setPixel(i, WS2812Strip::wheel(base + i * (256 / (n ? n : 1))));
            break;
        }

        case ANIM_CHASE: {
            uint8_t step = (millis() / 120) % 3;
            for (uint16_t i = 0; i < n; i++)
                s.setPixel(i, ((i % 3) == step) ? c.color : 0);
            break;
        }

        case ANIM_WIPE: {
            uint16_t pos = (millis() / 150) % (n + 4);       // +4 = pause when full
            for (uint16_t i = 0; i < n; i++)
                s.setPixel(i, (i <= pos) ? c.color : 0);
            break;
        }
    }

    s.setBrightness(c.brightness);
    s.show();
}

// ======================= Flight-controller switch ===========================
// A 6-pos switch sends 6 evenly-spaced pulse widths across the 1000-2000 us
// servo range. Bin the pulse with ~200 us margins around each centre.
int positionFromUs(uint16_t us) {
    if (us == 0)   return 0;   // no signal
    if (us < 1100) return 1;   // ~1000 us
    if (us < 1300) return 2;   // ~1200 us
    if (us < 1500) return 3;   // ~1400 us
    if (us < 1700) return 4;   // ~1600 us
    if (us < 1900) return 5;   // ~1800 us
    return 6;                  // ~2000 us
}

// Debounced switch position (0..6). Requires a stable reading for 60 ms.
int readPosition() {
    static int stable = 0, candidate = -1;
    static uint32_t since = 0;

    int pos = rc.valid() ? positionFromUs(rc.pulseUs()) : 0;
    if (pos != candidate) { candidate = pos; since = millis(); }
    if (millis() - since >= 60) stable = pos;
    return stable;
}

// ================================ Setup / loop ==============================
void setup() {
    Serial.begin(115200);   // optional: status output only, no commands

    if (!strip1.begin() || !strip2.begin()) {
        while (true) { Serial.println(F("ERROR: no free PIO state machine")); delay(1000); }
    }

    rc.begin(PWM_PIN);
}

void loop() {
    int pos = readPosition();

    // Log mode changes (handy for calibration; safe to ignore/remove).
    static int lastPos = -1;
    if (pos != lastPos) {
        lastPos = pos;
        Serial.print(F("[PWM] ")); Serial.print(rc.pulseUs());
        Serial.print(F(" us @ ")); Serial.print(rc.freqHz());
        Serial.print(F(" Hz -> position ")); Serial.println(pos);
    }

    static uint32_t last = 0;
    if (millis() - last >= 16) {        // ~60 FPS
        last = millis();
        renderStrip(strip1, strip1Cfg[pos]);
        renderStrip(strip2, strip2Cfg[pos]);
    }
}
