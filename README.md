# TamaPoke Plus

A Pokémon Tamagotchi for Waveshare's round 1.75" AMOLED boards, built on
[socquique's TamaPoke](https://github.com/socquique/TamaPoke) with features inspired by
[ShadowEnemyx's version](https://github.com/ShadowEnemyx/TamaPoke): 386 Pokémon (Kanto, Johto, Hoenn), wild battles
and catching, items, expeditions, step counter, daily goals and weather, auto sleep, Eevee and caught Pokémon as
starters, Agility / Memory training, fishing and slots.

| Board | Firmware file |
|---|---|
| Waveshare **ESP32-S3-Touch-AMOLED-1.75** (and -G) | `TamaPokePlus-esp32s3.bin` |
| Waveshare **RP2350-Touch-AMOLED-1.75** (and -G) | `TamaPokePlus-rp2350.uf2` |

## Getting the firmware

GitHub builds it on every push (**Actions** tab → latest *Build firmware* run → *Artifacts* at the bottom).
Tagged versions (`v…`) also appear under **Releases** with the files attached directly.

## Flashing

**ESP32-S3:** the `.bin` is one merged image, written at address **0x0**.
- In the browser (Chrome/Edge): open <https://espressif.github.io/esptool-js/>, *Connect* (pick the board's USB
  port; if it doesn't show, hold BOOT while plugging in), set Flash Address **0x0**, choose
  `TamaPokePlus-esp32s3.bin`, *Program*. Then press RESET.
- Or with esptool: `esptool.py --chip esp32s3 write_flash 0x0 TamaPokePlus-esp32s3.bin`

**RP2350:** hold **BOOT**, plug in USB, drag `TamaPokePlus-rp2350.uf2` onto the `RP2350` drive.

## SD card (both boards)

Format a microSD card as FAT32 and copy the `mons` folder to its root (`/mons/p001.bin` … `/mons/thumbs.bin`):
from `TamaPokePlus-sdcard.zip` (in the artifacts / release) or from `sdcard/mons` in this repo.

## Repository

- `TamaPokePlus/` — the sketch (one source for both boards; RP2350-only code is behind
  `#if defined(ARDUINO_ARCH_RP2040)`), `TamaPokePlus/test/` — game logic tests (`make asan`)
- `sdcard/mons/` — sprites and Pokédex thumbnails for the SD card
- `tools/` — sprite and data scripts (`make_thumbs.py` rebuilds `thumbs.bin`)
- `docs/` — full feature notes: [RP2350](docs/README_RP2350.md), [ESP32-S3](docs/README_ESP32S3.md),
  [original TamaPoke README](docs/README_original.md)
- `.github/workflows/build.yml` — the build

Building locally in the Arduino IDE also works; the board settings are in the docs.

Licences as upstream: code MIT; sprites PMD SpriteCollab, CC BY-NC 4.0; Pokémon © Nintendo / Game Freak /
The Pokémon Company. Personal, non-commercial use only.
