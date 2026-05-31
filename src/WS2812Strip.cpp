#include "WS2812Strip.h"

WS2812Strip::WS2812Strip(uint8_t pin, uint16_t numLeds)
    : _pin(pin), _n(numLeds), _buf(nullptr), _brightness(255),
      _pio(pio0), _sm(-1), _offset(0), _started(false) {
    _buf = new uint8_t[_n * 3]();   // zero-initialised
}

WS2812Strip::~WS2812Strip() {
    delete[] _buf;
}

bool WS2812Strip::begin(PIO pio, float freqHz) {
    _pio = pio;

    // Try PIO0 first, then PIO1, so we coexist with anything else using PIO.
    _sm = pio_claim_unused_sm(_pio, false);
    if (_sm < 0) {
        _pio = (pio == pio0) ? pio1 : pio0;
        _sm = pio_claim_unused_sm(_pio, false);
        if (_sm < 0) return false;       // no state machine available
    }

    _offset = pio_add_program(_pio, &ws2812_program);
    ws2812_program_init(_pio, _sm, _offset, _pin, freqHz, /*rgbw=*/false);
    _started = true;

    clear();
    show();
    return true;
}

void WS2812Strip::setPixel(uint16_t i, uint8_t r, uint8_t g, uint8_t b) {
    if (i >= _n) return;
    _buf[i * 3 + 0] = r;
    _buf[i * 3 + 1] = g;
    _buf[i * 3 + 2] = b;
}

void WS2812Strip::setPixel(uint16_t i, uint32_t rgb) {
    setPixel(i, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

uint32_t WS2812Strip::getPixel(uint16_t i) const {
    if (i >= _n) return 0;
    return Color(_buf[i * 3 + 0], _buf[i * 3 + 1], _buf[i * 3 + 2]);
}

void WS2812Strip::fill(uint32_t rgb) {
    for (uint16_t i = 0; i < _n; i++) setPixel(i, rgb);
}

void WS2812Strip::clear() {
    memset(_buf, 0, _n * 3);
}

void WS2812Strip::show() {
    if (!_started) return;
    for (uint16_t i = 0; i < _n; i++) {
        uint8_t r = scale8(_buf[i * 3 + 0], _brightness);
        uint8_t g = scale8(_buf[i * 3 + 1], _brightness);
        uint8_t b = scale8(_buf[i * 3 + 2], _brightness);
        // WS2812 wants GRB, MSB first, in the top 24 bits of the 32-bit word.
        uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
        pio_sm_put_blocking(_pio, _sm, grb << 8u);
    }
    delayMicroseconds(60);   // >50 us low -> latch the new frame
}

uint32_t WS2812Strip::scale(uint32_t rgb, uint8_t v) {
    uint8_t r = scale8((rgb >> 16) & 0xFF, v);
    uint8_t g = scale8((rgb >> 8) & 0xFF, v);
    uint8_t b = scale8(rgb & 0xFF, v);
    return Color(r, g, b);
}

uint32_t WS2812Strip::wheel(uint8_t pos) {
    pos = 255 - pos;
    if (pos < 85) {
        return Color(255 - pos * 3, 0, pos * 3);
    } else if (pos < 170) {
        pos -= 85;
        return Color(0, pos * 3, 255 - pos * 3);
    } else {
        pos -= 170;
        return Color(pos * 3, 255 - pos * 3, 0);
    }
}
