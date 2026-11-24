#pragma once
// TamaPoke Plus: the extra features (wild battles and catching, items,
// expeditions, step counter rewards, daily goals) added on top of the
// original game. This file is game state and rules only, no drawing; the
// screens are in plus_ui.ino.
#include <Arduino.h>
#include "dex.h"

class Pet;

// ---------- items ----------
enum : uint8_t { IT_BALL = 0, IT_GREAT, IT_POTION, IT_TREAT, IT_COUNT };
#define ITEM_MAX 99

// ---------- daily goals ----------
enum : uint8_t {
  G_FEED = 0, G_PLAY, G_BATH, G_PET, G_WIN, G_WALK, G_CATCH, G_TRAIN, G_COUNT
};
#define DAILY_GOALS 3

// ---------- persistent state ----------
struct PlusState {
  uint8_t items[IT_COUNT];
  uint8_t caught[49];          // Pokedex bitmap: species caught in the wild (386 bits)
  uint16_t wins, losses, winStreak, bestStreak;
  uint32_t day;                // day number (epoch / 86400) the daily data belongs to
  uint32_t stepsToday, stepsTotal;
  uint8_t stepMask;            // daily step rewards already given (500 / 2000 / 5000)
  uint8_t goalType[DAILY_GOALS];
  uint16_t goalProg[DAILY_GOALS];
  bool goalsClaimed;
  uint32_t expEnd;             // expedition: RTC epoch when the pet comes back (0 = home)
  uint8_t expMins;
  uint32_t expStepsBase;       // stepsTotal when the expedition started
  uint32_t nextWildMs;         // not saved: next wild encounter offer (millis)
  // minigames (plus3)
  uint8_t fishUsed, slotUsed;  // casts / spins used today
  uint16_t agiHi;              // agility record (targets hit)
  uint8_t memHi;               // memory record (rounds)
};
extern PlusState plus;

void plusBegin();                         // load (call after pet.begin(), before pet.syncClock())
void plusSave(bool now = false);          // marks a save (written by the prefs service)
void plusService(uint32_t nowEpoch);      // once per loop: day change, expedition return, step save
const char *plusToast();                  // short message to show on the main screen, or nullptr
void plusSetToast(const char *msg, uint16_t ms = 3500);
void plusReset();                         // new game (WIPE)

// items
bool plusUseItem(uint8_t item);           // outside battle: potion (energy), treat (joy + bond)
void plusGiveItem(uint8_t item, uint8_t n);
const char *itemName(uint8_t item);

// pokedex
bool plusCaught(int16_t dex);
uint16_t plusCaughtCount();

// steps (from the motion sensor)
void plusAddSteps(uint16_t n);

// daily goals
void plusGoal(uint8_t type, uint16_t amount = 1);
uint16_t goalTarget(uint8_t type);
bool goalDone(uint8_t i);

// minigames
#define FISH_PER_DAY 5
#define SLOTS_PER_DAY 3
uint8_t plusFishLeft();
uint8_t plusSpinsLeft();             // 3 a day, +1 per 1000 steps today (up to +3)
bool plusUseFish();
bool plusUseSpin();
int16_t pickWaterSpecies();          // a Pokemon that bites on the fishing line
bool plusIsBasic(int16_t dex);       // not evolved from anything (can be a starter)

// expeditions
bool plusCanExpedition();
bool plusStartExpedition(uint8_t minutes, uint32_t nowEpoch);
uint32_t plusExpeditionLeft(uint32_t nowEpoch);   // seconds, 0 if home
uint16_t expeditionWalkGoal(uint8_t minutes);

// ---------- battles ----------
struct Fighter {
  int16_t dex;
  uint16_t lv;
  uint16_t atk, def, spe, hp, hpMax;
  uint8_t t1, t2;
  bool shiny;
};
enum : uint8_t { BA_TACKLE = 0, BA_MOVE, BA_GUARD, BA_BALL, BA_GREAT, BA_POTION, BA_RUN };
enum : uint8_t { BS_CHOOSE = 0, BS_WON, BS_LOST, BS_CAUGHT, BS_FLED };
struct TurnLog {
  bool playerFirst;
  int16_t pDmg, eDmg;          // damage dealt by player / enemy (-1 = no attack)
  uint8_t pEff, eEff;          // type effect, percent (0, 50, 100, 200, 400)
  bool pMiss, eMiss;
  bool guarded;
  bool eSpecial;               // the wild one used its type move (else TACKLE)
  int16_t healed;
  int8_t ballShakes;           // -1 no ball thrown, 0..3 shakes, 4 = caught
  bool runFailed;
};
struct Battle {
  Fighter p, e;
  uint8_t state;
  uint8_t turns;
  TurnLog log;
  bool newCatch;
  bool applied;        // battleFinish() already ran
};
extern Battle battle;

bool plusCanBattle();                         // pet awake, hatched, home, enough energy
int16_t pickWildSpecies(uint16_t lv);
void battleBegin(int16_t wildDex, uint16_t wildLv, bool shiny);  // costs energy
void battleTurn(uint8_t action);               // fills battle.log, may end the battle
void battleFinish();                           // applies the result (rewards, record, save)
const char *moveName(uint8_t type);
uint8_t typeEffectPct(uint8_t atkType, uint8_t d1, uint8_t d2);
uint16_t fighterHpMax(uint8_t baseHp, uint16_t lv);
