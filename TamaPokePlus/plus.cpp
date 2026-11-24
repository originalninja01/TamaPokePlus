#include "plus.h"
#include "plus_i18n.h"
#include "pet.h"
#include "dex.h"
#include "audio.h"
#if defined(ARDUINO_ARCH_RP2040)
#include "prefs_rp.h"
#else
#include <Preferences.h>
#endif

extern Pet pet;
bool plusHasSensor = false;  // set by the sketch when the motion sensor answers

PlusState plus;
Battle battle;

static Preferences plusPrefs;
static char gToast[48];
static uint32_t gToastUntil = 0;
static uint32_t gStepsUnsaved = 0;
static uint32_t gStepSaveAt = 0;

#define PLUS_VER 1

// ---------------------------------------------------------------------------
// save / load: two small blobs (the settings store holds up to 40 bytes per key)
// ---------------------------------------------------------------------------
template <class T> static void put(uint8_t *&p, T v) { memcpy(p, &v, sizeof(T)); p += sizeof(T); }
template <class T> static void get(const uint8_t *&p, T &v) { memcpy(&v, p, sizeof(T)); p += sizeof(T); }

static void defaults() {
  memset(&plus, 0, sizeof(plus));
  plus.items[IT_BALL] = 5;
  plus.items[IT_POTION] = 1;
  plus.items[IT_TREAT] = 1;
  plus.day = 0xFFFFFFFFu;  // "no day yet": the first service rolls today's goals
}

void plusSave(bool) {
  uint8_t a[40], b[40];
  uint8_t *p = a;
  put<uint8_t>(p, PLUS_VER);
  for (uint8_t i = 0; i < IT_COUNT; i++) put(p, plus.items[i]);
  for (uint8_t i = 0; i < 19; i++) put(p, plus.caught[i]);
  put(p, plus.wins); put(p, plus.losses); put(p, plus.winStreak); put(p, plus.bestStreak);
  size_t na = p - a;
  p = b;
  put<uint8_t>(p, PLUS_VER);
  put(p, plus.day); put(p, plus.stepsToday); put(p, plus.stepsTotal); put(p, plus.stepMask);
  for (uint8_t i = 0; i < DAILY_GOALS; i++) put(p, plus.goalType[i]);
  for (uint8_t i = 0; i < DAILY_GOALS; i++) put(p, plus.goalProg[i]);
  put<uint8_t>(p, plus.goalsClaimed ? 1 : 0);
  put(p, plus.expEnd); put(p, plus.expMins); put(p, plus.expStepsBase);
  size_t nb = p - b;
  plusPrefs.putBytes("px1", a, na);
  plusPrefs.putBytes("px2", b, nb);
  plusPrefs.putBytes("px3", plus.caught + 19, 30);  // caught, Johto + Hoenn
  uint8_t c[6] = { PLUS_VER, plus.fishUsed, plus.slotUsed, (uint8_t)(plus.agiHi & 0xFF), (uint8_t)(plus.agiHi >> 8),
                   plus.memHi };
  plusPrefs.putBytes("px4", c, sizeof(c));  // minigames
  gStepsUnsaved = 0;
}

void plusBegin() {
  defaults();
  plusPrefs.begin("tamapoke", false);
  uint8_t a[40], b[40];
  size_t na = plusPrefs.getBytes("px1", a, sizeof(a));
  size_t nb = plusPrefs.getBytes("px2", b, sizeof(b));
  if (na >= 32 && a[0] == PLUS_VER) {
    const uint8_t *p = a + 1;
    for (uint8_t i = 0; i < IT_COUNT; i++) get(p, plus.items[i]);
    for (uint8_t i = 0; i < 19; i++) get(p, plus.caught[i]);
    get(p, plus.wins); get(p, plus.losses); get(p, plus.winStreak); get(p, plus.bestStreak);
  }
  plusPrefs.getBytes("px3", plus.caught + 19, 30);
  uint8_t c[6];
  if (plusPrefs.getBytes("px4", c, sizeof(c)) == sizeof(c) && c[0] == PLUS_VER) {
    plus.fishUsed = c[1];
    plus.slotUsed = c[2];
    plus.agiHi = c[3] | (c[4] << 8);
    plus.memHi = c[5];
  }
  if (nb >= 33 && b[0] == PLUS_VER) {
    const uint8_t *p = b + 1;
    get(p, plus.day); get(p, plus.stepsToday); get(p, plus.stepsTotal); get(p, plus.stepMask);
    for (uint8_t i = 0; i < DAILY_GOALS; i++) get(p, plus.goalType[i]);
    for (uint8_t i = 0; i < DAILY_GOALS; i++) get(p, plus.goalProg[i]);
    uint8_t c; get(p, c); plus.goalsClaimed = c;
    get(p, plus.expEnd); get(p, plus.expMins); get(p, plus.expStepsBase);
    for (uint8_t i = 0; i < DAILY_GOALS; i++)
      if (plus.goalType[i] >= G_COUNT) plus.goalType[i] = G_PET;
  }
  for (uint8_t i = 0; i < IT_COUNT; i++)
    if (plus.items[i] > ITEM_MAX) plus.items[i] = ITEM_MAX;
  pet.away = plus.expEnd != 0;
  plus.nextWildMs = millis() + 10UL * 60 * 1000;  // first wild offer after ~10 min
}

void plusReset() {
  defaults();
  pet.away = false;
  plusSave();
}

const char *plusToast() { return (gToastUntil && timeLeft(gToastUntil)) ? gToast : nullptr; }
void plusSetToast(const char *msg, uint16_t ms) {
  strncpy(gToast, msg, sizeof(gToast) - 1);
  gToast[sizeof(gToast) - 1] = 0;
  gToastUntil = millis() + ms;
  if (!gToastUntil) gToastUntil = 1;
}

// ---------------------------------------------------------------------------
// items
// ---------------------------------------------------------------------------
const char *itemName(uint8_t item) {
  static const PStrId ids[IT_COUNT] = { P_ITEM_BALL, P_ITEM_GREAT, P_ITEM_POTION, P_ITEM_TREAT };
  return item < IT_COUNT ? PT(ids[item]) : "";
}

void plusGiveItem(uint8_t item, uint8_t n) {
  if (item >= IT_COUNT) return;
  uint16_t v = plus.items[item] + n;
  plus.items[item] = v > ITEM_MAX ? ITEM_MAX : (uint8_t)v;
}

bool plusUseItem(uint8_t item) {
  if (item >= IT_COUNT || !plus.items[item]) return false;
  if (pet.isEgg() || pet.away || pet.sleeping || pet.ceremony) return false;
  if (item == IT_POTION) pet.boost(0, 0, 30, 0, 0);
  else if (item == IT_TREAT) pet.boost(10, 25, 0, 0, 2);
  else return false;  // balls are for battles
  plus.items[item]--;
  plusSave();
  return true;
}

// ---------------------------------------------------------------------------
// pokedex (caught)
// ---------------------------------------------------------------------------
bool plusCaught(int16_t dex) {
  return dex >= 1 && dex <= DEX_COUNT && (plus.caught[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
}
uint16_t plusCaughtCount() {
  uint16_t n = 0;
  for (int i = 1; i <= DEX_COUNT; i++) if (plusCaught(i)) n++;
  return n;
}
static void setCaught(int16_t dex) {
  if (dex >= 1 && dex <= DEX_COUNT) plus.caught[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
}

// ---------------------------------------------------------------------------
// daily goals
// ---------------------------------------------------------------------------
uint16_t goalTarget(uint8_t type) {
  switch (type) {
    case G_FEED: return 3;
    case G_PET: return 5;
    case G_WALK: return 1000;
    default: return 1;
  }
}
bool goalDone(uint8_t i) {
  return i < DAILY_GOALS && plus.goalProg[i] >= goalTarget(plus.goalType[i]);
}

static void rollGoals(uint32_t day) {
  uint32_t s = day * 2654435761u + 12345u;
  uint8_t n = 0;
  for (int guard = 0; n < DAILY_GOALS && guard < 64; guard++) {
    s = s * 1103515245u + 12345u;
    uint8_t t = (s >> 16) % G_COUNT;
    if (t == G_WALK && !plusHasSensor) continue;
    bool dup = false;
    for (uint8_t j = 0; j < n; j++) dup |= plus.goalType[j] == t;
    if (!dup) plus.goalType[n++] = t;
  }
  while (n < DAILY_GOALS) plus.goalType[n] = n, n++;  // (never reached in practice)
  memset(plus.goalProg, 0, sizeof(plus.goalProg));
  plus.goalsClaimed = false;
}

static void checkDailyReward() {
  if (plus.goalsClaimed) return;
  for (uint8_t i = 0; i < DAILY_GOALS; i++) if (!goalDone(i)) return;
  plus.goalsClaimed = true;
  plusGiveItem(IT_BALL, 2);
  plusGiveItem(IT_TREAT, 1);
  if (random(100) < 35) plusGiveItem(IT_GREAT, 1);
  plusSetToast(PT(P_DAILY_REWARD));
  sfxPlay(SFX_MEDAL);
  plusSave();
}

void plusGoal(uint8_t type, uint16_t amount) {
  bool changed = false;
  for (uint8_t i = 0; i < DAILY_GOALS; i++) {
    if (plus.goalType[i] != type || goalDone(i)) continue;
    uint32_t v = (type == G_WALK) ? amount : (uint32_t)plus.goalProg[i] + amount;
    plus.goalProg[i] = v > 60000 ? 60000 : (uint16_t)v;
    changed = true;
  }
  if (!changed) return;
  if (type != G_WALK) plusSave();
  checkDailyReward();
}

// ---------------------------------------------------------------------------
// steps
// ---------------------------------------------------------------------------
static const uint16_t STEP_REWARD_AT[3] = { 500, 2000, 5000 };
static const uint8_t STEP_REWARD_ITEM[3] = { IT_BALL, IT_POTION, IT_GREAT };

void plusAddSteps(uint16_t n) {
  if (!n) return;
  plus.stepsToday += n;
  plus.stepsTotal += n;
  gStepsUnsaved += n;
  for (uint8_t i = 0; i < 3; i++) {
    if (!(plus.stepMask & (1 << i)) && plus.stepsToday >= STEP_REWARD_AT[i]) {
      plus.stepMask |= 1 << i;
      plusGiveItem(STEP_REWARD_ITEM[i], 1);
      char m[48];
      snprintf(m, sizeof(m), "%s +1 %s", PT(P_STEP_REWARD), itemName(STEP_REWARD_ITEM[i]));
      plusSetToast(m);
      sfxPlay(SFX_LEVEL);
      plusSave();
    }
  }
  // walking makes it happy, a little (at most every 100 steps)
  static uint32_t joyAt = 0;
  if (plus.stepsTotal - joyAt >= 100) {
    joyAt = plus.stepsTotal;
    if (!pet.isEgg() && !pet.sleeping && !pet.away && !pet.ceremony) pet.boost(0, 1, 0, 0, 0);
  }
  plusGoal(G_WALK, plus.stepsToday > 60000 ? 60000 : (uint16_t)plus.stepsToday);
}

// ---------------------------------------------------------------------------
// minigames: fishing casts and slot spins
// ---------------------------------------------------------------------------
uint8_t plusFishLeft() { return plus.fishUsed >= FISH_PER_DAY ? 0 : FISH_PER_DAY - plus.fishUsed; }
uint8_t plusSpinsLeft() {
  uint32_t bonus = plus.stepsToday / 1000;
  uint8_t total = SLOTS_PER_DAY + (bonus > 3 ? 3 : (uint8_t)bonus);
  return plus.slotUsed >= total ? 0 : total - plus.slotUsed;
}
bool plusUseFish() {
  if (!plusFishLeft()) return false;
  plus.fishUsed++;
  plusSave();
  return true;
}
bool plusUseSpin() {
  if (!plusSpinsLeft()) return false;
  plus.slotUsed++;
  plusSave();
  return true;
}

bool plusIsBasic(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return false;
  for (uint16_t k = 0; k < EVOLUTION_RULE_COUNT; k++)
    if (EVOLUTION_RULES[k].to == dex) return false;
  return true;
}

int16_t pickWaterSpecies() {
  if (random(100) < 40) return 129;  // MAGIKARP: the classic
  for (int guard = 0; guard < 200; guard++) {
    int16_t d = 1 + random(DEX_COUNT);
    const DexEntry &e = DEX_TBL[d];
    if (e.type1 != TYPE_WATER && e.type2 != TYPE_WATER) continue;
    if (e.rarity == R_LEGENDARIO) continue;
    if (e.rarity == R_EVO && random(100) < 70) continue;  // mostly basic forms
    return d;
  }
  return 129;
}

// ---------------------------------------------------------------------------
// expeditions
// ---------------------------------------------------------------------------
uint16_t expeditionWalkGoal(uint8_t minutes) {
  return minutes <= 15 ? 250 : (minutes <= 30 ? 500 : 1000);
}
bool plusCanExpedition() {
  return !pet.isEgg() && !pet.away && !pet.sleeping && !pet.ceremony && pet.energy >= 30;
}
bool plusStartExpedition(uint8_t minutes, uint32_t nowEpoch) {
  if (!plusCanExpedition() || !nowEpoch) return false;
  pet.boost(0, 0, minutes >= 60 ? -25 : (minutes >= 30 ? -18 : -12), 0, 0);
  plus.expEnd = nowEpoch + (uint32_t)minutes * 60;
  plus.expMins = minutes;
  plus.expStepsBase = plus.stepsTotal;
  pet.away = true;
  plusSave();
  return true;
}
uint32_t plusExpeditionLeft(uint32_t nowEpoch) {
  if (!plus.expEnd) return 0;
  return nowEpoch < plus.expEnd ? plus.expEnd - nowEpoch : 0;
}

static uint8_t rollItem() {
  int r = random(100);
  return r < 45 ? IT_BALL : r < 60 ? IT_GREAT : r < 85 ? IT_POTION : IT_TREAT;
}

static void finishExpedition() {
  uint8_t rolls = plus.expMins >= 60 ? 3 : (plus.expMins >= 30 ? 2 : 1);
  if (plus.stepsTotal - plus.expStepsBase >= expeditionWalkGoal(plus.expMins)) rolls++;
  uint8_t got[IT_COUNT] = { 0 };
  for (uint8_t i = 0; i < rolls; i++) got[rollItem()]++;
  char m[48];
  int n = snprintf(m, sizeof(m), "%s", PT(P_EXP_BACK));
  for (uint8_t i = 0; i < IT_COUNT; i++) {
    if (!got[i]) continue;
    plusGiveItem(i, got[i]);
    if (n > 0 && n < (int)sizeof(m)) n += snprintf(m + n, sizeof(m) - n, " +%u", got[i]);
    // compact: show the count and the first letters of the item
    if (n > 0 && n < (int)sizeof(m) - 6) {
      const char *nm = itemName(i);
      n += snprintf(m + n, sizeof(m) - n, " %.5s", nm);
    }
  }
  plus.expEnd = 0;
  plus.expMins = 0;
  pet.away = false;
  pet.boost(0, 10, 0, 0, 1);
  plusSetToast(m, 6000);
  sfxPlay(SFX_MEDAL);
  plusSave();
}

// ---------------------------------------------------------------------------
// once per loop
// ---------------------------------------------------------------------------
void plusService(uint32_t nowEpoch) {
  if (nowEpoch) {
    uint32_t day = nowEpoch / 86400;
    if (day != plus.day) {
      plus.day = day;
      plus.stepsToday = 0;
      plus.stepMask = 0;
      plus.fishUsed = 0;
      plus.slotUsed = 0;
      rollGoals(day);
      plusSave();
    }
    if (plus.expEnd && nowEpoch >= plus.expEnd) finishExpedition();
  }
  if (plus.expEnd) pet.away = true;
  // steps: written in batches (every 200 steps or 5 minutes), not on every step
  uint32_t now = millis();
  if (gStepsUnsaved && (gStepsUnsaved >= 200 || now - gStepSaveAt > 300000UL)) {
    gStepSaveAt = now;
    plusSave();
  }
}

// ---------------------------------------------------------------------------
// battles
// ---------------------------------------------------------------------------
static const char TYPE_CHART[TYPE_COUNT][TYPE_COUNT + 1] = {
  // defender: - NOR FIR WAT ELE GRA ICE FIG POI GRO FLY PSY BUG ROC GHO DRA DAR STE FAI
  "1111111111111111111",  // NONE
  "1111111111111h011h1",  // NORMAL
  "11hh122111112h1h121",  // FIRE
  "112h1h111211121h111",  // WATER
  "1112hh111021111h111",  // ELECTRIC
  "11h21h11h2h1h21h1h1",  // GRASS
  "11hh12h1122111121h1",  // ICE
  "12111121h1hhh20122h",  // FIGHTING
  "11111211hh111hh1102",  // POISON
  "11212h112101h211121",  // GROUND
  "1111h21211112h111h1",  // FLYING
  "11111112211h11110h1",  // PSYCHIC
  "11h1121hh1h211h12hh",  // BUG
  "1121112h1h2121111h1",  // ROCK
  "1011111111121121h11",  // GHOST
  "11111111111111121h0",  // DRAGON
  "1111111h11121121h1h",  // DARK
  "11hhh121111112111h2",  // STEEL
  "11h11112h11111122h1",  // FAIRY
};

static uint16_t relPct(uint8_t a, uint8_t d) {
  if (a >= TYPE_COUNT || d >= TYPE_COUNT || d == TYPE_NONE) return 100;
  switch (TYPE_CHART[a][d]) {
    case '2': return 200;
    case 'h': return 50;
    case '0': return 0;
    default: return 100;
  }
}
uint8_t typeEffectPct(uint8_t atkType, uint8_t d1, uint8_t d2) {
  uint32_t v = (uint32_t)relPct(atkType, d1) * relPct(atkType, d2) / 100;
  return v > 255 ? 255 : (uint8_t)v;
}

const char *moveName(uint8_t type) {
  static const char *const N[TYPE_COUNT] = {
    "STRUGGLE", "HEADBUTT", "EMBER", "WATER GUN", "SPARK", "VINE WHIP", "ICE SHARD",
    "KARATE CHOP", "ACID", "MUD SHOT", "WING ATTACK", "CONFUSION", "BUG BITE",
    "ROCK THROW", "LICK", "TWISTER", "BITE", "METAL CLAW", "FAIRY WIND",
  };
  return type < TYPE_COUNT ? N[type] : N[0];
}

uint16_t fighterHpMax(uint8_t baseHp, uint16_t lv) {
  uint16_t L = lv > 100 ? 100 : lv;
  return 20 + (uint32_t)baseHp * (L + 20) / 50;
}

bool plusCanBattle() {
  return !pet.isEgg() && !pet.sleeping && !pet.away && !pet.ceremony && pet.energy >= 20;
}

// level its pre-evolution evolves at (evolved forms only appear wild above it)
static uint8_t prevoLevel(int16_t dex) {
  for (uint16_t k = 0; k < EVOLUTION_RULE_COUNT; k++)
    if (EVOLUTION_RULES[k].to == dex) return EVOLUTION_RULES[k].minLevel;
  return 0;
}

int16_t pickWildSpecies(uint16_t lv) {
  for (int guard = 0; guard < 80; guard++) {
    int16_t d = 1 + random(DEX_COUNT);
    uint8_t r = DEX_TBL[d].rarity;
    if (r == R_LEGENDARIO) { if (random(100) < 3 && lv >= 30) return d; continue; }
    if (r == R_RARO) { if (random(100) < 25) return d; continue; }
    if (r == R_EVO) { if (lv >= prevoLevel(d) && random(100) < 60) return d; continue; }
    return d;  // common
  }
  return random(2) ? 16 : 19;  // Pidgey / Rattata
}

static Fighter makeWild(int16_t dex, uint16_t lv, bool shiny) {
  const DexEntry &d = DEX_TBL[dex];
  Fighter f;
  f.dex = dex;
  f.lv = lv;
  f.atk = d.bAtk + lv;
  f.def = d.bDef + lv;
  f.spe = d.bSpe + lv;
  f.hpMax = f.hp = fighterHpMax(d.bHp, lv);
  f.t1 = DEX_TBL[dex].type1;
  f.t2 = DEX_TBL[dex].type2;
  f.shiny = shiny;
  return f;
}

void battleBegin(int16_t wildDex, uint16_t wildLv, bool shiny) {
  memset(&battle, 0, sizeof(battle));
  int16_t me = pet.speciesId;
  Fighter &p = battle.p;
  p.dex = me;
  p.lv = pet.level();
  p.atk = pet.atkStat();
  p.def = pet.defStat();
  p.spe = pet.speStat();
  p.hpMax = fighterHpMax(DEX_TBL[me].bHp, p.lv);
  // a tired pet starts the fight weaker
  p.hp = (uint32_t)p.hpMax * (50 + pet.energy / 2) / 100;
  if (p.hp < 1) p.hp = 1;
  p.t1 = DEX_TBL[me].type1;
  p.t2 = DEX_TBL[me].type2;
  p.shiny = pet.shiny;
  battle.e = makeWild(wildDex, wildLv, shiny);
  battle.state = BS_CHOOSE;
  battle.log.ballShakes = -1;
  battle.log.pDmg = battle.log.eDmg = -1;
  pet.boost(0, 0, -10, 0, 0);
}

// one hit; returns damage (0 if missed or no effect)
static int16_t hit(Fighter &a, Fighter &d, bool special, uint8_t &eff, bool &miss) {
  uint8_t type = special ? a.t1 : TYPE_NORMAL;
  uint8_t power = special ? 50 : 40;
  uint8_t acc = special ? 90 : 100;
  miss = random(100) >= acc;
  eff = typeEffectPct(type, d.t1, d.t2);
  if (miss || !eff) return 0;
  uint16_t L = a.lv > 100 ? 100 : a.lv;
  uint32_t dmg = (uint32_t)power * (L + 20) / 60;
  dmg = dmg * a.atk / (d.def ? d.def : 1) / 4 + 2;
  if (type == a.t1 || type == a.t2) dmg = dmg * 3 / 2;
  dmg = dmg * eff / 100;
  dmg = dmg * (85 + random(16)) / 100;
  if (dmg < 1) dmg = 1;
  if (dmg > 999) dmg = 999;
  d.hp = d.hp > dmg ? d.hp - dmg : 0;
  return (int16_t)dmg;
}

static void enemyAttacks(bool guarded) {
  TurnLog &L = battle.log;
  bool special = random(100) < 50;
  L.eSpecial = special;
  uint16_t before = battle.p.hp;
  L.eDmg = hit(battle.e, battle.p, special, L.eEff, L.eMiss);
  if (guarded && L.eDmg > 0) {  // guard: takes only a third of the hit
    int16_t g = L.eDmg / 3;
    if (g < 1) g = 1;
    battle.p.hp = before > (uint16_t)g ? before - g : 0;
    L.eDmg = g;
  }
}

static uint8_t catchChance(bool great) {
  const Fighter &e = battle.e;
  int c = 100 - 75 * e.hp / (e.hpMax ? e.hpMax : 1);   // 25% at full HP, ~100% near 0
  switch (DEX_TBL[e.dex].rarity) {
    case R_LEGENDARIO: c = c / 5; break;
    case R_RARO: c = c * 3 / 5; break;
    case R_EVO: c = c * 4 / 5; break;
  }
  if (great) c = c * 3 / 2;
  if (plus.stepsToday >= 2000) c += 5;  // walkers have an easier time
  if (c > 95) c = 95;
  if (c < 2) c = 2;
  return (uint8_t)c;
}

void battleTurn(uint8_t action) {
  if (battle.state != BS_CHOOSE) return;
  TurnLog &L = battle.log;
  memset(&L, 0, sizeof(L));
  L.pDmg = L.eDmg = -1;
  L.ballShakes = -1;
  battle.turns++;
  Fighter &p = battle.p, &e = battle.e;

  switch (action) {
    case BA_RUN: {
      int chance = p.spe >= e.spe ? 90 : 55;
      if (random(100) < chance) { battle.state = BS_FLED; return; }
      L.runFailed = true;
      enemyAttacks(false);
      break;
    }
    case BA_GUARD: {
      L.guarded = true;
      int16_t h = p.hpMax / 10;
      if (p.hp + h > p.hpMax) h = p.hpMax - p.hp;
      p.hp += h;
      L.healed = h;
      enemyAttacks(true);
      break;
    }
    case BA_POTION: {
      if (!plus.items[IT_POTION]) return;
      plus.items[IT_POTION]--;
      int16_t h = p.hpMax / 2;
      if (p.hp + h > p.hpMax) h = p.hpMax - p.hp;
      p.hp += h;
      L.healed = h;
      enemyAttacks(false);
      break;
    }
    case BA_BALL:
    case BA_GREAT: {
      uint8_t it = action == BA_GREAT ? IT_GREAT : IT_BALL;
      if (!plus.items[it]) return;
      plus.items[it]--;
      uint8_t c = catchChance(it == IT_GREAT);
      if ((uint8_t)random(100) < c) {
        L.ballShakes = 4;
        battle.newCatch = !plusCaught(e.dex);
        battle.state = BS_CAUGHT;
        return;
      }
      L.ballShakes = (int8_t)random(c / 30 + 1);  // closer calls shake more
      if (L.ballShakes > 3) L.ballShakes = 3;
      enemyAttacks(false);
      break;
    }
    default: {  // BA_TACKLE / BA_MOVE
      bool special = action == BA_MOVE;
      L.playerFirst = p.spe > e.spe || (p.spe == e.spe && random(2));
      if (L.playerFirst) {
        L.pDmg = hit(p, e, special, L.pEff, L.pMiss);
        if (e.hp) enemyAttacks(false);
      } else {
        enemyAttacks(false);
        if (p.hp) L.pDmg = hit(p, e, special, L.pEff, L.pMiss);
      }
      break;
    }
  }
  if (e.hp == 0) battle.state = BS_WON;
  else if (p.hp == 0) battle.state = BS_LOST;
}

void battleFinish() {
  if (battle.applied) return;
  battle.applied = true;
  switch (battle.state) {
    case BS_WON:
      plus.wins++;
      if (++plus.winStreak > plus.bestStreak) plus.bestStreak = plus.winStreak;
      pet.battleTrain(true);
      plusGoal(G_WIN);
      if (random(100) < 30) plusGiveItem(IT_BALL, 1);  // found a ball after the fight
      break;
    case BS_LOST:
      plus.losses++;
      plus.winStreak = 0;
      pet.battleTrain(false);
      break;
    case BS_CAUGHT:
      setCaught(battle.e.dex);
      pet.boost(0, 5, 0, 0, 1);
      plusGoal(G_CATCH);
      break;
    default:
      break;
  }
  plusSave();
}
