#pragma once

#define XPOWERS_CHIP_AXP2101

#if defined(ARDUINO_ARCH_RP2040)
// ---------------------------------------------------------------------------
// Waveshare RP2350-Touch-AMOLED-1.75 (also -G). Pins taken from Waveshare's own
// examples: github.com/waveshareteam/RP2350-Touch-AMOLED-1.75
//   examples/Arduino/01_GUI (qspi_pio.h, DEV_Config.h), examples/C/02_ES8311,
//   examples/MicroPython/01_SD
// ---------------------------------------------------------------------------
#define BOARD_RP2350 1

// AMOLED 466x466, CO5300 over QSPI, driven by PIO (the RP2350 has no QSPI
// peripheral free for the panel). CLK must be the pin before D0..D3.
#define LCD_CS 10
#define LCD_SCLK 11
#define LCD_SDIO0 12  // D0..D3 must be consecutive: 12, 13, 14, 15
#define LCD_RESET 16
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
#define LCD_COL_OFFSET 6

// Shared I2C bus (I2C1): CST9217 touch, AXP2101 PMU, PCF85063 RTC, ES8311
// codec, QMI8658 IMU
#define IIC_SDA 6
#define IIC_SCL 7
#define I2C_BUS Wire1
#define TP_INT 22
#define TP_RESET 23

// ES8311 audio. The RP2350 is the I2S master: it drives MCLK, BCLK and LRCLK
// (LRCLK must be BCLK + 1 for the arduino-pico I2S class).
#define I2S_DO_IO 1   // RP2350 -> codec DSDIN
#define I2S_DI_IO 2   // codec ASDOUT (microphone, unused)
#define I2S_MCK_IO 3
#define I2S_BCK_IO 4
#define I2S_WS_IO 5
#define PA 0          // speaker amplifier enable

// microSD in SPI mode (SPI0)
#define SD_SCK 18
#define SD_MOSI 19
#define SD_MISO 20
#define SD_CS 21

// AXP2101 IRQ line (not needed: the PWR button is polled over I2C)
#define SYS_OUT 9

#else
// ---------------------------------------------------------------------------
// Original board: Waveshare ESP32-S3-Touch-AMOLED-1.75
// Fuente: github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75 (libraries/Mylibrary/pin_config.h)
// ---------------------------------------------------------------------------

// Pantalla AMOLED 466x466, driver CO5300 por QSPI
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// Táctil capacitivo CST9217 por I2C
#define IIC_SDA 15
#define IIC_SCL 14
#define I2C_BUS Wire
#define TP_INT 11
#define TP_RESET 40

// Audio ES8311. NOTA: el MCLK real es GPIO42 (verificado en placa con el
// proyecto PlaneRadar2.0); el 16 que figuraba era erroneo. Aun asi el codec se
// configura con reloj derivado del BCLK, asi que el MCLK apenas importa.
#define I2S_MCK_IO 42
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8
#define PA 46

// Ranura TF (no usada todavía)
#define SDMMC_CLK 2
#define SDMMC_CMD 1
#define SDMMC_DATA 3
#define SDMMC_CS 41
#endif
