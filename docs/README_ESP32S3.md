# TamaPoke Plus for the Waveshare ESP32-S3-Touch-AMOLED-1.75

The same game as the RP2350 build (`1.18-plus4`): all 386 Pokémon of Kanto, Johto and Hoenn, wild battles and
catching, items, expeditions, step counter, daily goals and weather, auto sleep, Eevee and caught Pokémon as
starters, the Agility / Memory training games, fishing and slots. This is the board TamaPoke was originally
written for, so it runs on its own original code paths: 16-bit screen buffer in PSRAM, sprites loaded whole into
PSRAM, settings in NVS, sound from a FreeRTOS task, CPU light sleep between frames.

It is the same source as the RP2350 project; only the sketch name differs. Everything that is RP2350-only sits
behind `#if defined(ARDUINO_ARCH_RP2040)` and is left out here.

> **Board:** this is for the **Waveshare ESP32-S3-Touch-AMOLED-1.75** (or the **-G** GPS version). The separate
> "1.75**C**" and other ESP32-S3 boards have different pins; `pin_config.h` would need changing.

## What's new in plus4

**Starters**
- The starter screen has a fourth page after Kanto/Johto/Hoenn: **CAUGHT**. **Eevee** is always there, and every
  basic-stage Pokémon you have caught joins it (no evolved forms: Magikarp yes, Gyarados no). Use the arrows on the
  sides if the list runs past 12. Works for a new game and for *NEW STARTER* in settings.

**Eevee's evolution** (at level 30, plus any care slip-ups, like before)
- The berry colour you've fed it most since it hatched decides: **red → Flareon, blue → Vaporeon,
  green → Jolteon** (needs at least 5 berries and a clear favourite).
- No clear favourite: **today's weather** decides: rain → Vaporeon, storm → Jolteon, sun → Flareon, clouds → any.
- A very strong bond (80+) still gives **Espeon** by day or **Umbreon** at night.
- The progress card shows what it would become right now and why ("Becomes VAPOREON (berries)").

**Daily weather**
- Sunny, cloudy, rain or storm, one per day, shown next to the step counter. Rain falls on the scene,
  storms flash, and the sun hides on grey days.

**Training** (BATTLE card → TRAIN now opens a menu)
- **Strength**: the punching bag, as before (attack).
- **Agility**: 15 seconds of tapping targets that pop up and shrink, faster as you go (speed).
- **Memory**: four coloured pads light up in an order, one more each round; repeat it (defence, +2 per round).
- Each session gives up to +18, records are kept. Tap the top of the screen to quit any game.

**Games** (new card page, swipe to the last one)
- **Fishing**: CAST, wait for the float to dip and the "!" — then tap fast. Too early and it swims off, too late
  and it gets away. Catches Poké Balls, Great Balls, Potions, Treats… or an old boot, and now and then a
  **water Pokémon bites** and you battle it. 5 casts a day.
- **Slots**: SPIN, then tap to stop each reel. Three of a kind pays 2–3 of that item, a pair pays 1, three stars
  is the jackpot (2 Great Balls, 2 Poké Balls, a Treat). 3 spins a day, +1 for every 1000 steps (up to +3).


Other features (battles, catching, items, expeditions, steps, daily goals, auto sleep, regions) work as described
in the RP2350 README, which is included here as `README_RP2350.md`.

## 1. Build and flash (Arduino IDE 2.x)

1. *Boards Manager*: **esp32** by Espressif Systems (3.x).
2. *Library Manager*: **GFX Library for Arduino** (moononournation), **SensorLib** (Lewis He), **XPowersLib**
   (Lewis He), **U8g2** (olikraus). The motion sensor needs no library.
3. Open `TamaPokePlus_ESP32S3.ino` (`plus_ui.ino` and `plus_games.ino` come with it automatically).
4. *Tools*:
   - Board: **ESP32S3 Dev Module**
   - Flash Size: **16MB**
   - PSRAM: **OPI PSRAM** (required: the 466×466 screen buffer and the sprites live there)
   - Partition Scheme: **16M Flash (3MB APP/9.9MB FATFS)**
   - USB CDC On Boot: **Enabled**
5. Plug the board in and **Upload**. If the port doesn't show, hold BOOT while plugging it in.

Command line equivalent:

```
arduino-cli compile --upload -p <port> \
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" \
  TamaPokePlus_ESP32S3
```

## 2. SD card

Same as the RP2350 build: copy `tools/sdcard/mons` to the root of a FAT32 microSD card (`/mons/p001.bin` …
`/mons/thumbs.bin`, 772 sprites + 386 thumbnails). A card prepared for the RP2350 build works as is.

## What was checked, and what wasn't

I couldn't install the ESP32 toolchain where this was built, so **this version has not been compiled by the
real ESP32 compiler yet**. What was done instead:

- The whole sketch (all ESP32 code paths: display, audio task, SD_MMC sprite loader, light sleep, NVS) was
  compiled on a PC as ESP32 code, with stand-in headers for the ESP-only libraries and the real CO5300 display
  driver from the GFX library.
- It was then linked and run on the PC in that ESP32 mode with memory checkers on: the ESP sprite loader read
  the real sprite files, the 16-bit canvas drew every screen, and the full end-to-end test passed (Eevee
  starter, weather and berries, Agility, Memory, fishing with a water-Pokémon battle, slots, Eevee → Vaporeon,
  a caught Pokémon as the new starter).

If the Arduino IDE reports an error when you build it, send me the message and I'll fix it.

## ESP32-specific changes in Plus

- `audio.cpp`: species cries and the memory game's pad tones (same synthesis as the RP2350 build), sound queue
  widened to 16-bit ids.
- Light sleep between frames is capped at 40 ms while the motion sensor is working, so steps and shakes are still
  counted with the screen dark (the original slept up to a second at a time).
- Everything else (battles, items, games, starters, weather, auto sleep) is shared code.
