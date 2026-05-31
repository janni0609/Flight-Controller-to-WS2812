// -----------------------------------------------------------------------------
// ws2812.pio.h
//
// Pre-assembled WS2812 PIO program for the RP2040.
//
// This is the standard "ws2812" program from the Raspberry Pi pico-examples,
// already assembled to machine code so you don't need the pioasm tool in the
// build.  One PIO state machine clocks the bits out at 800 kHz with exact
// timing, completely independent of the CPU.
//
// Source PIO assembly (for reference):
//
//   .program ws2812
//   .side_set 1
//   .define public T1 2
//   .define public T2 5
//   .define public T3 3
//   .wrap_target
//   bitloop:
//       out x, 1        side 0 [T3 - 1]   ; shift out one data bit
//       jmp !x do_zero  side 1 [T1 - 1]   ; drive high, branch on bit value
//   do_one:
//       jmp  bitloop    side 1 [T2 - 1]   ; long high  -> a '1' bit
//   do_zero:
//       nop             side 0 [T2 - 1]   ; short high -> a '0' bit
//   .wrap
//
// Total = T1 + T2 + T3 = 10 PIO clocks per output bit.
// -----------------------------------------------------------------------------
#pragma once

#include <hardware/pio.h>
#include <hardware/clocks.h>

#define ws2812_T1 2
#define ws2812_T2 5
#define ws2812_T3 3
#define ws2812_CYCLES_PER_BIT (ws2812_T1 + ws2812_T2 + ws2812_T3)  // = 10

static const uint16_t ws2812_program_instructions[] = {
    //     .wrap_target
    0x6221, //  0: out    x, 1            side 0 [2]
    0x1123, //  1: jmp    !x, 3           side 1 [1]
    0x1400, //  2: jmp    0               side 1 [4]
    0xa442, //  3: nop                    side 0 [4]
    //     .wrap
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
};

static inline pio_sm_config ws2812_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 3);
    sm_config_set_sideset(&c, 1, false, false);
    return c;
}

// Configure a state machine to drive WS2812 LEDs on `pin`.
//   freq : data rate in Hz (800000 for standard WS2812 / WS2812B)
//   rgbw : true for SK6812-RGBW (32 bits/pixel), false for WS2812 (24 bits)
static inline void ws2812_program_init(PIO pio, uint sm, uint offset,
                                       uint pin, float freq, bool rgbw) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    pio_sm_config c = ws2812_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, rgbw ? 32 : 24);  // MSB first, autopull
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);             // deeper TX FIFO

    float div = clock_get_hz(clk_sys) / (freq * ws2812_CYCLES_PER_BIT);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
