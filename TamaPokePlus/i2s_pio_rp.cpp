#if defined(ARDUINO_ARCH_RP2040)
#include "i2s_pio_rp.h"
#include "pin_config.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

// .program audio_i2s  (Raspberry Pi pico-extras, BSD-3-Clause)
// .side_set 2                 ; bit 0 = BCLK, bit 1 = LRCLK
// bitloop1:
//     out pins, 1       side 0b10
//     jmp x-- bitloop1  side 0b11
//     out pins, 1       side 0b00
//     set x, 14         side 0b01
// bitloop0:
//     out pins, 1       side 0b00
//     jmp x-- bitloop0  side 0b01
//     out pins, 1       side 0b10
// public entry_point:
//     set x, 14         side 0b11
static const uint16_t kI2sInstr[] = {
  0x7001, 0x1840, 0x6001, 0xe82e, 0x6001, 0x0844, 0x7001, 0xf82e,
};
static const pio_program_t kI2sProg = {
  kI2sInstr, 8, -1, 0,
#if PICO_PIO_VERSION > 0
  0,
#endif
};
static const uint kEntryPoint = 7;

static PIO s_pio = nullptr;
static uint s_sm = 0;
static bool s_ok = false;

bool i2sPioBegin(uint32_t sampleRate) {
  uint offset;
  if (!pio_claim_free_sm_and_add_program_for_gpio_range(&kI2sProg, &s_pio, &s_sm, &offset,
                                                        I2S_DO_IO, I2S_BCK_IO + 2 - I2S_DO_IO, true))
    return false;
  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap(&c, offset, offset + 7);
  sm_config_set_sideset(&c, 2, false, false);
  sm_config_set_out_pins(&c, I2S_DO_IO, 1);
  sm_config_set_sideset_pins(&c, I2S_BCK_IO);
  sm_config_set_out_shift(&c, false, true, 32);  // MSB first, autopull 32 bits
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
  // 2 PIO cycles per bit, 32 bits per stereo frame
  float div = (float)clock_get_hz(clk_sys) / (float)(sampleRate * 32u * 2u);
  sm_config_set_clkdiv(&c, div);
  pio_gpio_init(s_pio, I2S_DO_IO);
  pio_gpio_init(s_pio, I2S_BCK_IO);
  pio_gpio_init(s_pio, I2S_BCK_IO + 1);
  pio_sm_set_consecutive_pindirs(s_pio, s_sm, I2S_DO_IO, 1, true);
  pio_sm_set_consecutive_pindirs(s_pio, s_sm, I2S_BCK_IO, 2, true);
  pio_sm_init(s_pio, s_sm, offset + kEntryPoint, &c);
  pio_sm_exec(s_pio, s_sm, pio_encode_jmp(offset + kEntryPoint));
  pio_sm_set_enabled(s_pio, s_sm, true);
  s_ok = true;
  return true;
}

void i2sPioWrite(int16_t left, int16_t right) {
  if (!s_ok) return;
  // the high half goes out while LRCLK is high (right channel in Philips I2S),
  // the low half while it is low (left); verified with a PIO simulation
  pio_sm_put_blocking(s_pio, s_sm, ((uint32_t)(uint16_t)right << 16) | (uint16_t)left);
}

void i2sPioSilence(uint32_t ms, uint32_t sampleRate) {
  uint32_t n = sampleRate * ms / 1000;
  while (n--) i2sPioWrite(0, 0);
}
#endif
