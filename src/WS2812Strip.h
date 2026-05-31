// -----------------------------------------------------------------------------
// WS2812Strip.h
//
// Tiny WS2812 ("NeoPixel") driver for the RP2040 built on the PIO peripheral.
// Keeps an RGB frame buffer in RAM; show() streams it to the LEDs through a
// claimed PIO state machine.  Colours are 0xRRGGBB; the strip handles the
// WS2812 GRB byte order and global brightness internally.
// -----------------------------------------------------------------------------
#pragma once

#include <Arduino.h>
#include "ws2812.pio.h"

class WS2812Strip {
public:
    WS2812Strip(uint8_t pin, uint16_t numLeds);
    ~WS2812Strip();

    // Claim a PIO + state machine and start the WS2812 program.
    // Returns false if no state machine was free. Defaults: PIO0, 800 kHz.
    bool begin(PIO pio = pio0, float freqHz = 800000.0f);

    void setPixel(uint16_t i, uint8_t r, uint8_t g, uint8_t b);
    void setPixel(uint16_t i, uint32_t rgb);          // 0xRRGGBB
    uint32_t getPixel(uint16_t i) const;              // 0xRRGGBB (un-scaled)

    void fill(uint32_t rgb);
    void clear();

    void setBrightness(uint8_t b) { _brightness = b; } // 0..255, applied at show()
    uint8_t brightness() const { return _brightness; }

    void show();                                       // push buffer to the LEDs
    uint16_t count() const { return _n; }

    // Colour helpers ---------------------------------------------------------
    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
    // Scale a 0xRRGGBB colour by v/255 (per-channel).
    static uint32_t scale(uint32_t rgb, uint8_t v);
    // Position 0..255 around the colour wheel -> 0xRRGGBB.
    static uint32_t wheel(uint8_t pos);

private:
    static inline uint8_t scale8(uint8_t x, uint8_t v) {
        return (uint16_t(x) * (uint16_t(v) + 1)) >> 8;
    }

    uint8_t  _pin;
    uint16_t _n;
    uint8_t *_buf;          // n * 3 bytes, stored R,G,B
    uint8_t  _brightness;
    PIO      _pio;
    int      _sm;
    uint     _offset;
    bool     _started;
};
