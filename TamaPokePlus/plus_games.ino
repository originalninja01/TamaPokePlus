// TamaPoke Plus minigames: the training menu (strength / agility / memory)
// and the item games (fishing, slots). Taps are handled the moment the
// finger lands (like the punching bag); tapping the top of the screen quits.

// ---------- item icons (also used by the slots) ----------
// 0 Poke Ball, 1 Great Ball, 2 Potion, 3 Treat, 4 star (slots jackpot)
void drawItemIcon(uint8_t sym, int cx, int cy, int r) {
  switch (sym) {
    case 0:
    case 1: {
      uint16_t top = sym == 0 ? UI_BAR_BAD : C565(0x3a, 0x6e, 0xd8);
      gfx->fillCircle(cx, cy, r, UI_WHITE);
      for (int dy = -r; dy < 0; dy++) {
        int hw = (int)sqrtf((float)(r * r - dy * dy));
        gfx->drawFastHLine(cx - hw, cy + dy, 2 * hw + 1, top);
      }
      if (sym == 1) {  // Great Ball: red marks on the blue top
        gfx->fillRoundRect(cx - r * 3 / 4, cy - r * 2 / 3, r / 3, r / 3, 2, UI_BAR_BAD);
        gfx->fillRoundRect(cx + r * 3 / 4 - r / 3, cy - r * 2 / 3, r / 3, r / 3, 2, UI_BAR_BAD);
      }
      gfx->fillRect(cx - r, cy - 2, 2 * r + 1, 4, INK_K);
      gfx->drawCircle(cx, cy, r, INK_K);
      gfx->fillCircle(cx, cy, r / 3, UI_WHITE);
      gfx->drawCircle(cx, cy, r / 3, INK_K);
      break;
    }
    case 2: {  // potion: purple bottle
      uint16_t body = C565(0x9b, 0x4d, 0xca);
      gfx->fillRect(cx - r / 4, cy - r, r / 2, r / 3, C565(0xb0, 0xb0, 0xb8));  // cap
      gfx->fillRect(cx - r / 5, cy - r * 2 / 3, r * 2 / 5, r / 4, body);       // neck
      gfx->fillRoundRect(cx - r * 2 / 3, cy - r / 2, r * 4 / 3, r * 3 / 2, r / 3, body);
      gfx->drawRoundRect(cx - r * 2 / 3, cy - r / 2, r * 4 / 3, r * 3 / 2, r / 3, INK_K);
      gfx->fillRoundRect(cx - r / 2 + 2, cy - r / 3, r / 4, r * 3 / 4, 2, UI_WHITE);  // shine
      break;
    }
    case 3: {  // treat: pink wrapped sweet
      uint16_t c = C565(0xf2, 0x7a, 0xa8);
      gfx->fillTriangle(cx - r, cy - r / 2, cx - r, cy + r / 2, cx - r / 3, cy, c);
      gfx->fillTriangle(cx + r, cy - r / 2, cx + r, cy + r / 2, cx + r / 3, cy, c);
      gfx->fillCircle(cx, cy, r * 3 / 5, c);
      gfx->drawCircle(cx, cy, r * 3 / 5, INK_K);
      gfx->fillCircle(cx - r / 5, cy - r / 5, r / 6, UI_WHITE);
      break;
    }
    default: {  // star
      uint16_t c = C565(0xf5, 0xc5, 0x18);
      for (int k = 0; k < 5; k++) {
        float a = -1.5708f + k * 1.2566f, b1 = a - 0.6283f, b2 = a + 0.6283f;
        gfx->fillTriangle(cx + (int)(r * cosf(a)), cy + (int)(r * sinf(a)),
                          cx + (int)(r * 0.42f * cosf(b1)), cy + (int)(r * 0.42f * sinf(b1)),
                          cx + (int)(r * 0.42f * cosf(b2)), cy + (int)(r * 0.42f * sinf(b2)), c);
      }
      gfx->fillCircle(cx, cy, (int)(r * 0.45f), c);
      break;
    }
  }
}

// ---------- shared ----------
static uint32_t pgOverUntil = 0;  // result screen
static char pgL1[40], pgL2[40], pgL3[40];
static uint16_t pgL2col = 0;

static bool pgCanPlay() { return !pet.isEgg() && !pet.away && !pet.sleeping && !pet.ceremony; }

static void pgResult(const char *l1, const char *l2, uint16_t col2, const char *l3, uint16_t ms) {
  snprintf(pgL1, sizeof(pgL1), "%s", l1 ? l1 : "");
  snprintf(pgL2, sizeof(pgL2), "%s", l2 ? l2 : "");
  snprintf(pgL3, sizeof(pgL3), "%s", l3 ? l3 : "");
  pgL2col = col2;
  pgOverUntil = millis() + ms;
  if (!pgOverUntil) pgOverUntil = 1;
}

static bool drawPgResult() {  // true while the result is up
  if (!pgOverUntil) return false;
  uint16_t ink = inkColor();
  gfx->setTextColor(ink);
  uint8_t sz = textW(pgL1, 4) <= 380 ? 4 : 2;
  setSize(sz);
  setCur(centerX(pgL1, sz), 150);
  printT(pgL1);
  gfx->setTextColor(pgL2col);
  sz = textW(pgL2, 3) <= 380 ? 3 : 2;
  setSize(sz);
  setCur(centerX(pgL2, sz), 210);
  printT(pgL2);
  gfx->setTextColor(ink);
  setSize(2);
  setCur(centerX(pgL3, 2), 256);
  printT(pgL3);
  return true;
}

static void drawQuitHint() {
  gfx->setTextColor(inkColor());
  setSize(1);
  setCur(centerX(PT(P_TOP_QUIT), 1), 16);
  printT(PT(P_TOP_QUIT));
}

static void drawBigButton(int x, int y, int w, int h, const char *label, const char *sub, uint16_t col, bool on) {
  gfx->fillRoundRect(x, y, w, h, 14, on ? col : UI_TRACK);
  gfx->setTextColor(UI_WHITE);
  setSize(3);
  setCur(x + (w - textW(label, 3)) / 2, y + (sub ? 10 : (h - 24) / 2));
  printT(label);
  if (sub) {
    setSize(1);
    setCur(x + (w - textW(sub, 1)) / 2, y + h - 18);
    printT(sub);
  }
}

static void quitPlusGame() {
  pgMode = PG_NONE;
  pgOverUntil = 0;
}

// ---------- training menu ----------
void openTrainingMenu() {
  if (!pgCanPlay()) { sfxPlay(SFX_DENY); return; }
  pgMode = PG_TRAIN_MENU;
  pgOverUntil = 0;
}

static void startAgility();
static void startMemory();

static void renderTrainMenu() {
  cardTitle(PT(P_TRAIN_TITLE));
  drawBigButton(78, 100, 310, 70, PT(P_TR_STR), PT(P_TR_STR_SUB), UI_BAR_BAD, true);
  drawBigButton(78, 184, 310, 70, PT(P_TR_AGI), PT(P_TR_AGI_SUB), 0x4C98, true);
  drawBigButton(78, 268, 310, 70, PT(P_TR_MEM), PT(P_TR_MEM_SUB), UI_BAR_OK, true);
  drawQuitHint();
}

static void trainMenuTap(int16_t x, int16_t y) {
  if (x < 78 || x > 388) return;
  if (y >= 100 && y <= 170) { pgMode = PG_NONE; startSack(); }
  else if (y >= 184 && y <= 254) startAgility();
  else if (y >= 268 && y <= 338) startMemory();
  else return;
  sfxPlay(SFX_TAP);
}

// ---------- agility: tap the shrinking targets (trains speed) ----------
#define AGI_MS 15000
static uint32_t agiUntil = 0, agiBorn = 0, agiLife = 0;
static uint16_t agiHits = 0;
int16_t agiX = 0, agiY = 0;  // (not static: the PC tests aim at the target)

static void agiNewTarget() {
  float a = random(628) / 100.0f, r = 20 + random(130);
  agiX = CX + (int16_t)(r * cosf(a));
  agiY = 232 + (int16_t)(r * sinf(a));
  if (agiY < 100) agiY = 100;
  agiBorn = millis();
  int life = 1250 - agiHits * 22;
  agiLife = life < 560 ? 560 : life;
}

static void startAgility() {
  if (!pgCanPlay()) { sfxPlay(SFX_DENY); return; }
  pgMode = PG_AGILITY;
  pgOverUntil = 0;
  agiHits = 0;
  agiUntil = millis() + AGI_MS + 800;  // a short "ready" pause first
  agiNewTarget();
  agiBorn = millis() + 800;
}

static int agiRadius(uint32_t now) {
  if ((int32_t)(now - agiBorn) < 0) return 38;
  uint32_t age = now - agiBorn;
  if (age >= agiLife) return 0;
  return 12 + (int)(28 * (agiLife - age) / agiLife);
}

static void agilityTap(int16_t x, int16_t y) {
  uint32_t now = millis();
  if (pgOverUntil || (int32_t)(now - agiBorn) < 0) return;
  int r = agiRadius(now) + 14;
  int dx = x - agiX, dy = y - agiY;
  if (r > 14 && dx * dx + dy * dy <= r * r) {
    agiHits++;
    sfxPlay(SFX_PLAY);
    agiNewTarget();
  }
}

static void renderAgility() {
  uint32_t now = millis();
  if (!timeLeft(agiUntil) && !pgOverUntil) {  // time up: train speed
    bool hi = agiHits > plus.agiHi;
    if (hi) plus.agiHi = agiHits;
    uint8_t g = pet.trainStat(2, agiHits / 3, agiHits);
    if (agiHits) plusGoal(G_TRAIN);
    plusSave();
    char a[24], b[24], c[24];
    snprintf(a, sizeof(a), PT(P_HITS2_FMT), agiHits);
    snprintf(b, sizeof(b), PT(P_SPE_GAIN_FMT), g);
    if (hi && agiHits) snprintf(c, sizeof(c), "%s", PT(P_NEW_BEST));
    else snprintf(c, sizeof(c), PT(P_BEST_FMT), plus.agiHi);
    pgResult(a, b, 0x4C98, c, 3500);
    sfxPlay(hi ? SFX_MEDAL : SFX_LEVEL);
  }
  if (pgOverUntil) {
    if (!timeLeft(pgOverUntil)) { quitPlusGame(); return; }
    drawPgResult();
    return;
  }
  if (agiRadius(now) == 0) agiNewTarget();  // missed: next one
  int r = agiRadius(now);
  gfx->fillCircle(agiX, agiY, r, UI_BAR_BAD);
  if (r > 8) gfx->fillCircle(agiX, agiY, r * 2 / 3, UI_WHITE);
  if (r > 4) gfx->fillCircle(agiX, agiY, r / 3, UI_BAR_BAD);
  char n[8];
  snprintf(n, sizeof(n), "%u", agiHits);
  gfx->setTextColor(inkColor());
  setSize(3);
  setCur(centerX(n, 3), 58);
  printT(n);
  setSize(1);
  setCur(centerX(PT(P_AGI_HINT), 1), 388);
  printT(PT(P_AGI_HINT));
  uint32_t left = (int32_t)(agiUntil - now) > 0 ? agiUntil - now : 0;
  if (left > AGI_MS) left = AGI_MS;
  int bw = 240, fw = (int)((uint32_t)bw * left / AGI_MS);
  gfx->fillRoundRect(CX - bw / 2, 404, bw, 12, 5, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 404, fw, 12, 5, 0x4C98);
  drawQuitHint();
}

// ---------- memory: repeat the order of the pads (trains defence) ----------
#define MEM_MAX 24
uint8_t memSeq[MEM_MAX], memLen = 0, memIn = 0, memPhase = 0;  // 0 show, 1 input
static uint32_t memT0 = 0, memLitUntil = 0, memInputSince = 0;
static int8_t memLit = -1, memShownStep = -1;
static const int16_t MEM_PX[4] = { CX, CX + 104, CX, CX - 104 };
static const int16_t MEM_PY[4] = { 128, 232, 336, 232 };
#define MEM_STEP_MS 650

static void memNextRound() {
  memSeq[memLen++] = random(4);
  memPhase = 0;
  memIn = 0;
  memT0 = millis() + 700;
  memShownStep = -1;
}

static void startMemory() {
  if (!pgCanPlay()) { sfxPlay(SFX_DENY); return; }
  pgMode = PG_MEMORY;
  pgOverUntil = 0;
  memLen = 0;
  memLit = -1;
  memNextRound();
}

static void memEnd(uint8_t rounds) {
  bool hi = rounds > plus.memHi;
  if (hi) plus.memHi = rounds;
  uint8_t g = pet.trainStat(1, rounds * 2, rounds * 3);
  if (rounds) plusGoal(G_TRAIN);
  plusSave();
  char a[24], b[24], c[24];
  snprintf(a, sizeof(a), PT(P_ROUND_FMT), rounds);
  snprintf(b, sizeof(b), PT(P_DEF_GAIN_FMT), g);
  if (hi && rounds) snprintf(c, sizeof(c), "%s", PT(P_NEW_BEST));
  else snprintf(c, sizeof(c), PT(P_BEST_FMT), plus.memHi);
  pgResult(a, b, UI_BAR_OK, c, 3500);
  sfxPlay(hi ? SFX_MEDAL : SFX_DENY);
}

static void memoryTap(int16_t x, int16_t y) {
  if (pgOverUntil || memPhase != 1) return;
  for (int8_t p = 0; p < 4; p++) {
    int dx = x - MEM_PX[p], dy = y - MEM_PY[p];
    if (dx * dx + dy * dy > 62 * 62) continue;
    memLit = p;
    memLitUntil = millis() + 250;
    padTonePlay(p);
    memInputSince = millis();
    if (memSeq[memIn] != p) { memEnd(memLen - 1); return; }
    if (++memIn == memLen) {
      if (memLen >= MEM_MAX) memEnd(memLen);
      else memNextRound();
    }
    return;
  }
}

static void renderMemory() {
  uint32_t now = millis();
  if (pgOverUntil) {
    if (!timeLeft(pgOverUntil)) { quitPlusGame(); return; }
    drawPgResult();
    return;
  }
  int8_t lit = -1;
  if (memPhase == 0) {  // showing the order
    if ((int32_t)(now - memT0) >= 0) {
      uint32_t step = (now - memT0) / MEM_STEP_MS, in = (now - memT0) % MEM_STEP_MS;
      if (step >= memLen) { memPhase = 1; memInputSince = now; }
      else {
        if (in < 430) lit = memSeq[step];
        if ((int8_t)step != memShownStep) { memShownStep = step; padTonePlay(memSeq[step]); }
      }
    }
  } else {
    if (timeLeft(memLitUntil)) lit = memLit;
    if (now - memInputSince > 7000) { memEnd(memLen - 1); return; }  // waited too long
  }
  static const uint16_t COL[4] = { C565(0xd8, 0x3c, 0x3c), C565(0x3a, 0x6e, 0xd8), C565(0x3c, 0xa8, 0x50),
                                   C565(0xe8, 0xc0, 0x28) };
  for (int p = 0; p < 4; p++) {
    uint16_t c = p == lit ? lerp565(COL[p], UI_WHITE, 9, 16) : lerp565(COL[p], INK_K, 5, 16);
    gfx->fillCircle(MEM_PX[p], MEM_PY[p], 52, c);
    gfx->drawCircle(MEM_PX[p], MEM_PY[p], 52, p == lit ? UI_WHITE : INK_K);
    if (p == lit) gfx->drawCircle(MEM_PX[p], MEM_PY[p], 56, UI_WHITE);
  }
  char n[12];
  snprintf(n, sizeof(n), "%u", memLen);
  gfx->setTextColor(inkColor());
  setSize(4);
  setCur(centerX(n, 4), 218);
  printT(n);
  const char *h = memPhase == 0 ? PT(P_MEM_WATCH) : PT(P_MEM_TURN);
  setSize(2);
  setCur(centerX(h, 2), 404);
  printT(h);
  drawQuitHint();
}

// ---------- fishing (items; sometimes a water Pokemon bites) ----------
enum : uint8_t { FS_READY = 0, FS_WAIT, FS_BITE, FS_RESULT };
uint8_t fishPhase = FS_READY;
static uint32_t fishBiteAt = 0, fishResultUntil = 0;
static char fishMsg[40];
static int16_t fishBattleDex = 0;

void startFishing() {
  if (!pgCanPlay()) { sfxPlay(SFX_DENY); return; }
  pgMode = PG_FISHING;
  pgOverUntil = 0;
  fishPhase = FS_READY;
  fishBattleDex = 0;
}

static void fishShow(const char *m, uint16_t ms) {
  snprintf(fishMsg, sizeof(fishMsg), "%s", m);
  fishPhase = FS_RESULT;
  fishResultUntil = millis() + ms;
}

static uint8_t rollFishItem() {
  int r = random(100);
  return r < 38 ? IT_BALL : r < 50 ? IT_GREAT : r < 78 ? IT_POTION : IT_TREAT;
}

static void fishingTap(int16_t x, int16_t y) {
  uint32_t now = millis();
  switch (fishPhase) {
    case FS_READY:
      if (x >= CX - 80 && x <= CX + 80 && y >= 120 && y <= 172) {
        if (!plusUseFish()) { sfxPlay(SFX_DENY); return; }
        fishPhase = FS_WAIT;
        fishBiteAt = now + 1500 + random(4500);
        sfxPlay(SFX_TAP);
      }
      break;
    case FS_WAIT:
      fishShow(PT(P_TOO_SOON), 2200);
      sfxPlay(SFX_DENY);
      break;
    case FS_BITE: {
      int r = random(100);
      if (r < 14 && plusCanBattle()) {  // a Pokemon!
        fishBattleDex = pickWaterSpecies();
        fishShow(PT(P_FISH_BATTLE), 1600);
        chirpPlay(fishBattleDex);
      } else if (r < 26) {
        fishShow(PT(P_OLD_BOOT), 2200);
        sfxPlay(SFX_DENY);
      } else {
        uint8_t it = rollFishItem();
        plusGiveItem(it, 1);
        plusSave();
        char m[40];
        snprintf(m, sizeof(m), PT(P_GOT_ITEM_FMT), itemName(it));
        fishShow(m, 2400);
        sfxPlay(SFX_LEVEL);
      }
      break;
    }
    default:
      break;
  }
}

static void renderFishing() {
  uint32_t now = millis();
  // water and a small jetty
  int wy = 236;
  gfx->fillRect(0, wy, 466, 466 - wy, C565(0x2a, 0x6f, 0xb4));
  for (int i = 0; i < 6; i++) {  // ripples
    int rx = (i * 83 + now / 40) % 520 - 30;
    gfx->drawFastHLine(rx, wy + 20 + i * 30, 40, C565(0x7f, 0xb8, 0xe6));
  }
  gfx->fillRect(0, 286, 150, 14, C565(0x8a, 0x5a, 0x34));
  gfx->fillRect(20, 300, 10, 70, C565(0x6a, 0x42, 0x24));
  gfx->fillRect(118, 300, 10, 70, C565(0x6a, 0x42, 0x24));
  if (pmd.loaded) drawPmdActM(pmd, PMD_IDLE, 82, 286, now, true, false, 2);
  // rod line and float
  bool cast = fishPhase == FS_WAIT || fishPhase == FS_BITE;
  if (cast) {
    if (fishPhase == FS_WAIT && (int32_t)(now - fishBiteAt) >= 0) {
      fishPhase = FS_BITE;
      sfxPlay(SFX_LEVEL);
    }
    if (fishPhase == FS_BITE && now - fishBiteAt > 900) {  // too slow
      fishShow(PT(P_TOO_LATE), 2200);
      sfxPlay(SFX_DENY);
    }
  }
  if (fishPhase == FS_WAIT || fishPhase == FS_BITE) {
    bool bite = fishPhase == FS_BITE;
    int bx = 310 + (int)(4 * sinf(now * 0.004f)), by = wy + 34 + (bite ? 9 + (int)(5 * sinf(now * 0.05f)) : (int)(3 * sinf(now * 0.006f)));
    gfx->drawLine(132, 230, bx, by - 8, inkColor());
    gfx->fillCircle(bx, by, 9, UI_BAR_BAD);
    gfx->fillRect(bx - 9, by, 19, 10, UI_WHITE);
    gfx->drawCircle(bx, by, 9, INK_K);
    if (bite) {
      gfx->setTextColor(UI_BAR_BAD);
      setSize(5);
      setCur(bx - 12, by - 76);
      printT("!");
      gfx->setTextColor(inkColor());
      setSize(2);
      setCur(centerX(PT(P_BITE), 2), 120);
      printT(PT(P_BITE));
    } else {
      gfx->setTextColor(inkColor());
      setSize(2);
      setCur(centerX(PT(P_WAIT_BITE), 2), 120);
      printT(PT(P_WAIT_BITE));
    }
  }
  gfx->drawLine(60, 280, 132, 230, C565(0x6a, 0x42, 0x24));  // rod
  if (fishPhase == FS_RESULT) {
    gfx->fillRoundRect(60, 110, 346, 56, 14, UI_WHITE);
    gfx->setTextColor(UI_INK);
    uint8_t sz = textW(fishMsg, 2) <= 330 ? 2 : 1;
    setSize(sz);
    setCur(centerX(fishMsg, sz), 130);
    printT(fishMsg);
    if (!timeLeft(fishResultUntil)) {
      if (fishBattleDex) {  // something big: off to a battle
        int16_t d = fishBattleDex;
        fishBattleDex = 0;
        quitPlusGame();
        int l = (int)pet.level() - 3 + (int)random(4);
        startBattle(d, l < 1 ? 1 : (uint16_t)l, random(100) == 0);
        return;
      }
      fishPhase = FS_READY;
    }
  }
  if (fishPhase == FS_READY) {
    bool can = plusFishLeft() > 0;
    drawBigButton(CX - 80, 120, 160, 52, PT(P_CAST), nullptr, can ? 0x4C98 : UI_TRACK, can);
    char t[32];
    if (can) snprintf(t, sizeof(t), PT(P_CASTS_FMT), plusFishLeft());
    else snprintf(t, sizeof(t), "%s", PT(P_NONE_LEFT_TODAY));
    gfx->setTextColor(inkColor());
    setSize(2);
    setCur(centerX(t, 2), 184);
    printT(t);
  }
  drawQuitHint();
}

// ---------- slots ----------
static const uint8_t REEL[3][10] = {
  { 0, 2, 3, 0, 1, 3, 0, 2, 4, 3 },
  { 3, 0, 2, 1, 0, 3, 4, 0, 2, 3 },
  { 2, 0, 3, 4, 0, 1, 3, 2, 0, 3 },
};
uint8_t reelPos[3] = { 0, 3, 6 };
static bool reelSpin[3] = { false, false, false };
static uint32_t slotStart = 0, slotDoneUntil = 0;
uint8_t slotStage = 0;  // 0 ready, 1 spinning, 2 result
static char slotMsg[40];

void startSlots() {
  if (!pgCanPlay()) { sfxPlay(SFX_DENY); return; }
  pgMode = PG_SLOTS;
  pgOverUntil = 0;
  slotStage = 0;
}

static void slotsPayout() {
  uint8_t a = REEL[0][reelPos[0]], b = REEL[1][reelPos[1]], c = REEL[2][reelPos[2]];
  static const uint8_t SYM_ITEM[5] = { IT_BALL, IT_GREAT, IT_POTION, IT_TREAT, IT_GREAT };
  if (a == b && b == c) {
    if (a == 4) {  // three stars
      plusGiveItem(IT_GREAT, 2);
      plusGiveItem(IT_BALL, 2);
      plusGiveItem(IT_TREAT, 1);
      snprintf(slotMsg, sizeof(slotMsg), "%s", PT(P_JACKPOT));
      sfxPlay(SFX_EVOLVE);
    } else {
      uint8_t n = a == 0 ? 3 : 2;
      plusGiveItem(SYM_ITEM[a], n);
      snprintf(slotMsg, sizeof(slotMsg), PT(P_WIN_FMT), n, itemName(SYM_ITEM[a]));
      sfxPlay(SFX_MEDAL);
    }
  } else if (a == b || b == c || a == c) {
    uint8_t s = (a == b || a == c) ? a : b;
    plusGiveItem(SYM_ITEM[s], 1);
    snprintf(slotMsg, sizeof(slotMsg), PT(P_WIN_FMT), 1, itemName(SYM_ITEM[s]));
    sfxPlay(SFX_LEVEL);
  } else {
    snprintf(slotMsg, sizeof(slotMsg), "%s", PT(P_NO_LUCK));
    sfxPlay(SFX_DENY);
  }
  plusSave();
  slotStage = 2;
  slotDoneUntil = millis() + 2600;
}

static void slotsStopNext() {
  for (int i = 0; i < 3; i++) {
    if (!reelSpin[i]) continue;
    reelSpin[i] = false;
    sfxPlay(SFX_TAP);
    if (i == 2) slotsPayout();
    return;
  }
}

static void slotsTap(int16_t x, int16_t y) {
  if (slotStage == 0) {
    if (x >= CX - 80 && x <= CX + 80 && y >= 300 && y <= 352) {
      if (!plusUseSpin()) { sfxPlay(SFX_DENY); return; }
      for (int i = 0; i < 3; i++) reelSpin[i] = true;
      slotStart = millis();
      slotStage = 1;
      sfxPlay(SFX_PLAY);
    }
  } else if (slotStage == 1) {
    if (millis() - slotStart > 400) slotsStopNext();
  }
}

static void renderSlots() {
  uint32_t now = millis();
  cardTitle(PT(P_SLOTS));
  if (slotStage == 1) {
    for (int i = 0; i < 3; i++)
      if (reelSpin[i]) reelPos[i] = (uint8_t)(((now - slotStart) / (110 + i * 15) + i * 3) % 10);
    // they stop on their own if not tapped
    if (reelSpin[0] && now - slotStart > 3500) slotsStopNext();
    else if (!reelSpin[0] && reelSpin[1] && now - slotStart > 4300) slotsStopNext();
    else if (!reelSpin[1] && reelSpin[2] && now - slotStart > 5100) slotsStopNext();
  }
  gfx->fillRoundRect(72, 120, 322, 132, 18, C565(0x8a, 0x2a, 0x2a));
  for (int i = 0; i < 3; i++) {
    int x = 86 + i * 102;
    gfx->fillRoundRect(x, 134, 90, 104, 10, UI_WHITE);
    drawItemIcon(REEL[i][reelPos[i]], x + 45, 186, 30);
    if (slotStage == 1 && !reelSpin[i]) gfx->drawRoundRect(x, 134, 90, 104, 10, UI_BAR_WARN);
  }
  gfx->setTextColor(inkColor());
  setSize(2);
  if (slotStage == 2) {
    setCur(centerX(slotMsg, 2), 266);
    printT(slotMsg);
    if (!timeLeft(slotDoneUntil)) slotStage = 0;
  } else if (slotStage == 1) {
    setCur(centerX(PT(P_TAP_STOP), 2), 266);
    printT(PT(P_TAP_STOP));
  }
  if (slotStage != 1) {
    bool can = plusSpinsLeft() > 0;
    drawBigButton(CX - 80, 300, 160, 52, PT(P_SPIN), nullptr, can ? UI_BAR_BAD : UI_TRACK, can);
    char t[32];
    if (can) snprintf(t, sizeof(t), PT(P_SPINS_FMT), plusSpinsLeft());
    else snprintf(t, sizeof(t), "%s", PT(P_NONE_LEFT_TODAY));
    gfx->setTextColor(inkColor());
    setSize(2);
    setCur(centerX(t, 2), 364);
    printT(t);
    setSize(1);
    setCur(centerX(PT(P_SPIN_HINT), 1), 390);
    printT(PT(P_SPIN_HINT));
  }
  drawQuitHint();
}

// ---------- routing (called from the sketch) ----------
void plusGameTap(int16_t x, int16_t y) {
  if (y < 60) { quitPlusGame(); return; }  // top of the screen: leave
  switch (pgMode) {
    case PG_TRAIN_MENU: trainMenuTap(x, y); break;
    case PG_AGILITY: agilityTap(x, y); break;
    case PG_MEMORY: memoryTap(x, y); break;
    case PG_FISHING: fishingTap(x, y); break;
    case PG_SLOTS: slotsTap(x, y); break;
  }
}

void renderPlusGame() {
  drawGameScene();
  switch (pgMode) {
    case PG_TRAIN_MENU: renderTrainMenu(); break;
    case PG_AGILITY: renderAgility(); break;
    case PG_MEMORY: renderMemory(); break;
    case PG_FISHING: renderFishing(); break;
    case PG_SLOTS: renderSlots(); break;
  }
  gfx->flush();
}
