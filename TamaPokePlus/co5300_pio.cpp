#if defined(ARDUINO_ARCH_RP2040)
#include "co5300_pio.h"
#include "pin_config.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"

CO5300_PIO amoled;

// .program qspi_4wire          (side-set: SCLK, optional)
//   out pins, 4   side 0       ; put a nibble on D0..D3, clock low
//   nop           side 1       ; clock high: the panel samples
// Autopull every 8 bits, shifting left: the byte goes out high nibble first.
// SCLK = clk_sys / 2 (75 MHz at the default 150 MHz).
static const uint16_t kQspiInstr[] = { 0x7004, 0xb842 };
static const pio_program_t kQspiProg = {
  kQspiInstr, 2, -1, 0,
#if PICO_PIO_VERSION > 0
  0,
#endif
};

void CO5300_PIO::select() { gpio_put(LCD_CS, 0); }
void CO5300_PIO::deselect() { gpio_put(LCD_CS, 1); }

void CO5300_PIO::putQuad(uint8_t v) { pio_sm_put_blocking(_pio, _sm, (uint32_t)v << 24); }

// "single line" byte: each bit goes on D0 (bit 0 of a nibble); D1..D3 stay 0.
// Two bits per FIFO byte (high nibble first), MSB first overall.
void CO5300_PIO::putLine1(uint8_t v) {
  for (int i = 3; i >= 0; --i) {
    uint8_t hi = (v >> (2 * i + 1)) & 1, lo = (v >> (2 * i)) & 1;
    putQuad((uint8_t)((hi << 4) | lo));
  }
}

void CO5300_PIO::drain() {
  // the FIFO empties before the last byte has been clocked out: wait for the
  // state machine to stall on an empty FIFO too, or CS rises mid-byte
  while (!pio_sm_is_tx_fifo_empty(_pio, _sm)) tight_loop_contents();
  uint32_t mask = 1u << (PIO_FDEBUG_TXSTALL_LSB + _sm);
  _pio->fdebug = mask;  // write-1-to-clear
  while (!(_pio->fdebug & mask)) tight_loop_contents();
}

// write register: 0x02, 24-bit address 00 reg 00, params, all single line
void CO5300_PIO::cmdParams(uint8_t reg, const uint8_t *data, uint8_t n) {
  select();
  putLine1(0x02);
  putLine1(0x00);
  putLine1(reg);
  putLine1(0x00);
  for (uint8_t i = 0; i < n; i++) putLine1(data[i]);
  drain();
  deselect();
}

bool CO5300_PIO::begin() {
  gpio_init(LCD_CS);
  gpio_set_dir(LCD_CS, GPIO_OUT);
  gpio_put(LCD_CS, 1);
  gpio_init(LCD_RESET);
  gpio_set_dir(LCD_RESET, GPIO_OUT);

  uint offset;
  if (!pio_claim_free_sm_and_add_program_for_gpio_range(&kQspiProg, &_pio, &_sm, &offset,
                                                        LCD_SCLK, 5, true)) {
    Serial.println("CO5300: no free PIO state machine");
    return false;
  }
  pio_sm_config c = pio_get_default_sm_config();
  sm_config_set_wrap(&c, offset, offset + 1);
  sm_config_set_sideset(&c, 2, true, false);
  sm_config_set_sideset_pins(&c, LCD_SCLK);
  sm_config_set_out_pins(&c, LCD_SDIO0, 4);
  sm_config_set_out_shift(&c, false, true, 8);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
  sm_config_set_clkdiv(&c, 1.0f);
  pio_gpio_init(_pio, LCD_SCLK);
  for (int i = 0; i < 4; i++) {
    pio_gpio_init(_pio, LCD_SDIO0 + i);
    gpio_set_drive_strength(LCD_SDIO0 + i, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_slew_rate(LCD_SDIO0 + i, GPIO_SLEW_RATE_FAST);
  }
  gpio_set_drive_strength(LCD_SCLK, GPIO_DRIVE_STRENGTH_8MA);
  gpio_set_slew_rate(LCD_SCLK, GPIO_SLEW_RATE_FAST);
  pio_sm_set_consecutive_pindirs(_pio, _sm, LCD_SCLK, 1, true);
  pio_sm_set_consecutive_pindirs(_pio, _sm, LCD_SDIO0, 4, true);
  pio_sm_init(_pio, _sm, offset, &c);
  pio_sm_set_enabled(_pio, _sm, true);

  _dma = dma_claim_unused_channel(true);
  dma_channel_config dc = dma_channel_get_default_config(_dma);
  channel_config_set_transfer_data_size(&dc, DMA_SIZE_8);
  channel_config_set_read_increment(&dc, true);
  channel_config_set_write_increment(&dc, false);
  channel_config_set_dreq(&dc, pio_get_dreq(_pio, _sm, true));
  dma_channel_configure(_dma, &dc, &_pio->txf[_sm], nullptr, 0, false);

  // hardware reset
  gpio_put(LCD_RESET, 1);
  delay(50);
  gpio_put(LCD_RESET, 0);
  delay(50);
  gpio_put(LCD_RESET, 1);
  delay(200);

  // init sequence from Waveshare's RP2350-Touch-AMOLED-1.75 example
  cmdParams(0x11, nullptr, 0);  // sleep out
  delay(120);
  { const uint8_t d[] = { 0x80 }; cmdParams(0xC4, d, 1); }       // SPI mode ctrl
  { const uint8_t d[] = { 0x01, 0xD7 }; cmdParams(0x44, d, 2); } // tear scanline
  { const uint8_t d[] = { 0x00 }; cmdParams(0x35, d, 1); }       // tearing effect on
  { const uint8_t d[] = { 0x20 }; cmdParams(0x53, d, 1); }       // brightness ctrl on
  delay(10);
  cmdParams(0x29, nullptr, 0);  // display on
  delay(10);
  { const uint8_t d[] = { 0x00 }; cmdParams(0x51, d, 1); }       // brightness 0 until first frame
  cmdParams(0x20, nullptr, 0);  // inversion off
  { const uint8_t d[] = { 0x00 }; cmdParams(0x36, d, 1); }       // MADCTL
  { const uint8_t d[] = { 0x05 }; cmdParams(0x3A, d, 1); }       // RGB565
  _ok = true;
  return true;
}

void CO5300_PIO::setBrightness(uint8_t b) {
  if (!_ok) return;
  waitDma();
  cmdParams(0x51, &b, 1);
}

void CO5300_PIO::beginPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  uint16_t x0 = x + LCD_COL_OFFSET, x1 = x0 + w - 1;
  uint16_t y0 = y, y1 = y + h - 1;
  { const uint8_t d[] = { (uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1 }; cmdParams(0x2A, d, 4); }
  { const uint8_t d[] = { (uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1 }; cmdParams(0x2B, d, 4); }
  // memory write: 0x32 + 24-bit address 00 2C 00 single line, then quad data
  select();
  putLine1(0x32);
  putLine1(0x00);
  putLine1(0x2C);
  putLine1(0x00);
}

void CO5300_PIO::pushAsync(const void *bytes, uint32_t len) {
  dma_channel_set_read_addr(_dma, bytes, false);
  dma_channel_set_trans_count(_dma, len, true);
}

void CO5300_PIO::waitDma() {
  if (_dma >= 0) dma_channel_wait_for_finish_blocking(_dma);
}

void CO5300_PIO::endPixels() {
  waitDma();
  drain();
  deselect();
}
#endif
