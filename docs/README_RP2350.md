# TamaPoke Plus for the Waveshare RP2350-Touch-AMOLED-1.75

This is the working RP2350 port of [socquique's TamaPoke](https://github.com/socquique/TamaPoke) (v1.18),
plus a set of new features inspired by [ShadowEnemyx's expanded version](https://github.com/ShadowEnemyx/TamaPoke).
The features were written fresh on top of the port that runs on this board; none of the expanded version's code
is used. All three regions: **Kanto, Johto and Hoenn (#1–386)**, shinies included. Firmware version: `1.18-plus4`.

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

Your plus3 save carries over.

## What's new in plus3: auto sleep

- **Away for 40 minutes:** if nobody touches the screen, presses PWR, shakes it or walks with it for 40 minutes,
  it turns the lights out and goes to sleep by itself (*"Zzz... it fell asleep"*).
- **Energy at 5% or less:** it falls asleep even while you're playing (*"Too tired: sleeping"*). If you wake it
  with the light button it stays up; it only does this again once its energy has been back above 10%.
- Never while it's on an expedition, in a battle or minigame, or still an egg. Wake it as usual with the light
  button. Asleep, energy recovers and its needs drop much more slowly.

## What's new in plus2

- **Johto and Hoenn.** 386 species. The starter screen has a **< KANTO >** switch: tap the arrows for the Johto
  (Chikorita, Cyndaquil, Totodile) or Hoenn (Treecko, Torchic, Mudkip) starters. Eggs and wild Pokémon come from
  all three regions. The Pokédex starts each region on its own page; tap the **REGION n/N >** label at the
  bottom to jump to the next region.
- **Branching evolutions** from those games: Eevee also becomes Espeon (day) or Umbreon (night) with a strong
  bond; Pichu, Cleffa, Igglybuff, Togepi, Golbat, Chansey, Azurill and Budew-style friendship evolutions need
  bond 50+ (the card says *Needs more bond*); Tyrogue depends on attack vs defence; Gloom, Poliwhirl, Slowpoke,
  Wurmple and Clamperl can branch. When several are ready it prefers one missing from your Pokédex.
- **New starter.** Swipe down (time/sound/language screen) → red **NEW STARTER** button, tap it twice (the second
  tap within 4 seconds, it shows *SURE?*). Your current Pokémon leaves and you pick a starter again from any
  region. Pokédex, catches, items, battle record and steps are kept.
- Names: French and German for every species; Japanese and Korean only exist for Kanto, so Johto/Hoenn names
  show in English in those two languages.

Your plus1 save carries over (the new Pokédex entries are stored in extra keys).

## Features added to the original

| Feature | How to use it |
|---|---|
| **Wild battles** | Now and then a banner says *"Wild PIDGEY!"*: tap it. Or swipe up → *BATTLE* card → **WILD**. Battles are turn-based: your type move (EMBER, VINE WHIP…), TACKLE, GUARD (takes a third of the damage and heals a little), BAG, and RUN at the top. Type matchups count. A battle costs 10 energy and needs 20+. Winning trains attack/speed and raises joy and bond. |
| **Catching** | In a battle: BAG → POKE BALL / GREAT BALL. Weaker wild Pokémon are easier to catch; rare and legendary ones are hard. Caught Pokémon appear in colour in the Pokédex with a little ball marker (*CAUGHT n* under the title). |
| **Items** | Poké Ball, Great Ball, Potion (battle: +50% HP; outside: +30 energy), Treat (+joy, +bond). Swipe up → *BAG* page, tap Potion or Treat to use. You start with 5 balls, 1 potion, 1 treat. |
| **Expeditions** | Swipe up → *EXPEDITION* page → 15 / 30 / 60 min. Your Pokémon leaves (its needs pause while away) and comes back with 1–3 items. Walk the shown number of steps meanwhile for a bonus item. Needs 30+ energy. Keeps running while the board is off. |
| **Step counter** | Uses the board's QMI8658 motion sensor. Steps today show under the header; the *STEPS* page has totals. 500 / 2,000 / 5,000 steps a day each give an item; walking cheers it up a little and makes wild Pokémon appear more often. |
| **Shake** | Shake the board: the egg wobbles, a hatched Pokémon gets petted and calls out. |
| **Daily goals** | Three a day (feed, play, bath, pet, win a battle, catch, train, walk). Finish all three for balls and a treat. *DAILY GOALS* page. |
| **Species cries** | Each Pokémon has its own short synthesized call (no game audio): when it's petted, shaken, or appears wild. |

The card (swipe up) now has 8 pages: profile, battle, medals, progress, daily goals, bag, expedition, steps.
Swipe left/right to move between them.

## 1. Flash

Hold **BOOT**, plug in USB (or press RESET while holding BOOT). Drag **`TamaPokePlus_RP2350.uf2`** onto the
`RP2350` drive.

**Your save:** if the board last ran the *original* TamaPoke port, your Pokémon carries over. If it last ran the
expanded port (which froze), that build used a different save layout, so this starts a new game.

## 2. SD card

Use the **`tools/sdcard/mons`** folder from this zip: 772 sprite files (normal + shiny for all 386) and a new
`thumbs.bin` with 386 thumbnails. Copy it to the root of a FAT32 card
so you have `/mons/p001.bin … /mons/thumbs.bin`. **Replace the whole `mons` folder** (the old `thumbs.bin` only has 151
entries, and the expanded port's is a different format). Without a card the game runs with its 9 built-in sprites; wild Pokémon then have no picture.

## Build it yourself (Arduino IDE 2.x)

Same as the original port: board package **Raspberry Pi Pico/RP2040/RP2350** (Earle Philhower, 6.3.2), libraries
**GFX Library for Arduino** 1.6.9, **SensorLib** 0.5.1, **XPowersLib** 0.3.3, **U8g2**. Board **Generic RP2350**,
Flash Size **16MB (Sketch: 14MB, FS: 2MB)**. Open `TamaPokePlus_RP2350.ino` (`plus_ui.ino` in the same folder is
picked up automatically).

## What was checked

- Builds for the RP2350: 538 KB code, 253 KB static RAM, 271 KB free.
- plus4: an end-to-end PC run (memory checkers on) picks Eevee from the CAUGHT page, checks the weather days and
  the berry rule, plays Agility and Memory (aimed taps; DEF +12 for 6 rounds), five casts and a forced
  water-Pokémon battle while fishing, three slot spins, evolves Eevee to Vaporeon on blue berries, and starts
  over with a caught Pidgey. 136 logic tests pass.
- plus2: every one of the 772 sprites and 386 thumbnails loads through the game's own loader; starters from each
  region, all 26 Pokédex pages, battles against Johto/Hoenn species and the New starter reset were run in the
  PC emulation with memory checkers, with no errors. Logic tests: 131 pass (including a check that all 386
  species are reachable through eggs and evolutions, save round-trip of Johto/Hoenn entries, and friendship
  evolutions).
- The whole sketch was run on a PC with memory checkers (AddressSanitizer + UBSan, locals filled with garbage),
  touch going through the real touch code and a simulated motion sensor and SD card with the real sprites:
  starter, egg taps and hatching, 13 battles of every kind (attacks, guard, catching, running, losing), the wild
  banner, every card page, items, a 15‑minute expedition, 50 minutes of walking and shaking. No errors. Also run
  without the sensor, at night, on low battery, and with the expanded port's `thumbs.bin` on the card.
- The original game's logic tests pass (updated for 386).

## Files

New: `plus.h/.cpp` (rules and save data), `plus_ui.ino` (screens), `plus_i18n.*` (text in EN/ES/FR/DE/IT/PT;
Japanese and Korean show the new text in English), `imu_rp.*` (small QMI8658 reader, no library),
`dex.h` now holds the 386-species table, types and evolution rules (data from ShadowEnemyx's table);
`tools/make_thumbs.py` builds the 386 thumbnails. Changed: `pet.*` (expedition pause, reward hooks), `audio_rp.cpp`
(species cries), `TamaPokePlus_RP2350.ino` (hooks, card pages), `canvas8.h` (pixel read-back for tests).

Licences as upstream: code MIT; sprites PMD SpriteCollab, CC BY-NC 4.0; Pokémon © Nintendo / Game Freak /
The Pokémon Company. Personal, non-commercial use.
