// -----------------------------------------------------------------------------
// RcPwm.h
//
// Reads a single RC / flight-controller PWM channel using a pin-change
// interrupt, so the main loop never blocks (unlike pulseIn()).
//
// The switch position is encoded in the PULSE WIDTH (high-time, ~1000-2000 us),
// which is independent of the refresh rate - this works from 50 Hz up to the
// 500 Hz that fast FC outputs use. periodUs()/freqHz() report the measured
// refresh rate so you can confirm what the FC is actually sending.
//
// Single channel only - one RcPwm instance is supported (static ISR trampoline).
// -----------------------------------------------------------------------------
#pragma once

#include <Arduino.h>

class RcPwm {
public:
    void begin(uint8_t pin) {
        _pin = pin;
        _instance = this;
        pinMode(pin, INPUT_PULLDOWN);   // idles low between pulses; safe if FC unplugged
        attachInterrupt(digitalPinToInterrupt(pin), isrTrampoline, CHANGE);
    }

    // Latest measured pulse width in microseconds (0 if none yet).
    uint16_t pulseUs() const {
        noInterrupts();
        uint16_t w = _width;
        interrupts();
        return w;
    }

    // Measured refresh period / frequency of the PWM signal.
    uint16_t periodUs() const {
        noInterrupts();
        uint16_t p = _period;
        interrupts();
        return p;
    }
    uint16_t freqHz() const {
        uint16_t p = periodUs();
        return p ? (uint16_t)(1000000UL / p) : 0;
    }

    // True if a valid pulse arrived within `timeoutMs` (signal is live).
    bool valid(uint32_t timeoutMs = 100) const {
        return _width > 0 && (millis() - _lastEdgeMs) < timeoutMs;
    }

private:
    static void isrTrampoline() { if (_instance) _instance->handle(); }

    void handle() {
        uint32_t now = micros();
        if (digitalRead(_pin)) {            // rising edge -> pulse start
            uint32_t p = now - _rise;       // _rise still holds the previous rising edge
            if (p >= 1000 && p <= 30000)    // 33..1000 Hz sanity window
                _period = (uint16_t)p;
            _rise = now;
        } else {                            // falling edge -> pulse end
            uint32_t w = now - _rise;
            if (w >= 700 && w <= 2500) {    // ignore glitches / out-of-range
                _width = (uint16_t)w;
                _lastEdgeMs = millis();
            }
        }
    }

    uint8_t           _pin = 0;
    volatile uint32_t _rise = 0;
    volatile uint16_t _width = 0;
    volatile uint16_t _period = 0;
    volatile uint32_t _lastEdgeMs = 0;

    static RcPwm *_instance;
};

inline RcPwm *RcPwm::_instance = nullptr;
