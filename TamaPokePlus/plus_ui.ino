// TamaPoke Plus: screens and glue for the extra features (wild battles,
// catching, items, expeditions, step counter, daily goals). The rules live in
// plus.cpp; this file only draws and reacts to taps. Arduino appends this file
// to TamaPokePlus_RP2350.ino, so it uses that file's helpers directly.

// ---------- wild encounters ----------
static int16_t wildDex = 0;
static uint16_t wildLv = 0;
static bool wildShiny = false;

static bool mainScreenIdle() {
  return !galleryOpen && !cardOpen && !kbOpen && !clockOpen && !gameOpen && !sackOpen && !battleOpen && !pgMode &&
         !pet.ceremony && !confirmUntil && !choiceKind && !feedMenuUntil && !bathUntil &&
         !pet.awaitingStarter() && !pet.evolving();
}

static void rollWild() {
  uint16_t lv = pet.level();
  int l = (int)lv - 3 + (int)random(4);  // a little below the pet, up to its level
  wildLv = l < 1 ? 1 : (l > 999 ? 999 : (uint16_t)l);
  wildDex = pickWildSpecies(wildLv);
  wildShiny = random(plus.stepsToday >= 2000 ? 70 : 100) == 0;
}

static void offerWild(uint32_t now) {
  rollWild();
  wildOfferUntil = now + 20000;
  chirpPlay(wildDex);
}

// ---------- battle screen state ----------
struct BMsg {
  char a[36], b[36];
  int8_t who;           // 0 the pet acts, 1 the wild one acts, -1 nobody
  uint16_t hpP, hpE;    // HP to show once this message is up
};
static BMsg bmsg[6];
static uint8_t bmsgN = 0;
static uint32_t bmsgT0 = 0;
static uint8_t battleMenu = 0;   // 0 main, 1 bag
static float dispP = 0, dispE = 0;
#define BMSG_MS 1100

static const char *petName() { return pet.nick[0] ? pet.nick : dexName(pet.speciesId); }

static void addMsg(const char *a, const char *b, int8_t who) {
  if (bmsgN >= 6) return;
  BMsg &m = bmsg[bmsgN++];
  snprintf(m.a, sizeof(m.a), "%s", a ? a : "");
  snprintf(m.b, sizeof(m.b), "%s", b ? b : "");
  m.who = who;
  m.hpP = battle.p.hp;
  m.hpE = battle.e.hp;
}

static uint8_t msgIndex() {
  if (!bmsgN) return 0;
  uint32_t i = (millis() - bmsgT0) / BMSG_MS;
  return i >= bmsgN ? bmsgN : (uint8_t)i;
}
static bool msgsPlaying() { return bmsgN && msgIndex() < bmsgN; }

static void effectText(char *out, size_t n, int16_t dmg, uint8_t eff, bool miss) {
  if (miss) snprintf(out, n, "%s", PT(P_MISSED));
  else if (eff == 0) snprintf(out, n, "%s", PT(P_NO_EFFECT));
  else if (eff > 100) snprintf(out, n, "%s -%d", PT(P_SUPER), dmg);
  else if (eff < 100) snprintf(out, n, "%s -%d", PT(P_NOT_VERY), dmg);
  else snprintf(out, n, "-%d", dmg);
}

void startBattle(int16_t dex, uint16_t lv, bool shiny) {
  if (!plusCanBattle() || dex < 1 || dex > DEX_COUNT) return;
  wildOfferUntil = 0;
  battleBegin(dex, lv, shiny);
  battleOpen = true;
  battleMenu = 0;
  galleryPmd.unload();
  galleryPmd.load(dex, shiny);  // the gallery is closed: its sprite slot is free
  dispP = battle.p.hp;
  dispE = battle.e.hp;
  bmsgN = 0;
  char a[36];
  snprintf(a, sizeof(a), PT(P_WILD_APPEARS), dexName(dex));
  addMsg(a, nullptr, 1);
  bmsgT0 = millis();
  chirpPlay(dex);
}

void startWildFromOffer() {
  if (!wildDex || !plusCanBattle()) {
    wildOfferUntil = 0;
    plusSetToast(PT(P_TOO_TIRED), 2000);
    sfxPlay(SFX_DENY);
    return;
  }
  startBattle(wildDex, wildLv, wildShiny);
}

void startWildRandom() {
  rollWild();
  startBattle(wildDex, wildLv, wildShiny);
}

static void closeBattle() {
  battleFinish();
  battleOpen = false;
  galleryPmd.unload();
  bmsgN = 0;
  plus.nextWildMs = millis() + (15 + random(20)) * 60000UL;
}

static void doAction(uint8_t action) {
  const Fighter &p = battle.p, &e = battle.e;
  if ((action == BA_BALL && !plus.items[IT_BALL]) || (action == BA_GREAT && !plus.items[IT_GREAT]) ||
      (action == BA_POTION && !plus.items[IT_POTION])) {
    sfxPlay(SFX_DENY);
    bmsgN = 0;
    addMsg(PT(P_NO_ITEM), nullptr, -1);
    bmsgT0 = millis();
    return;
  }
  uint16_t p0 = p.hp, e0 = e.hp;
  battleTurn(action);
  const TurnLog &L = battle.log;
  bmsgN = 0;
  char a[36], b[36];
  const char *myMove = action == BA_MOVE ? moveName(p.t1) : PT(P_TACKLE);
  // the message list is built with the HP shown at each step
  Fighter &bp = battle.p, &be = battle.e;
  uint16_t pEnd = bp.hp, eEnd = be.hp;
  auto enemyMsg = [&]() {
    if (L.eDmg < 0) return;
    snprintf(a, sizeof(a), "%s: %s!", dexName(e.dex), L.eSpecial ? moveName(e.t1) : PT(P_TACKLE));
    effectText(b, sizeof(b), L.eDmg, L.eEff, L.eMiss);
    bp.hp = pEnd;
    addMsg(a, b, 1);
  };
  bp.hp = p0;
  be.hp = e0;
  switch (action) {
    case BA_RUN:
      if (battle.state == BS_FLED) { addMsg(PT(P_FLED), nullptr, -1); break; }
      addMsg(PT(P_CANT_RUN), nullptr, -1);
      enemyMsg();
      break;
    case BA_GUARD:
    case BA_POTION:
      bp.hp = p0 + L.healed;
      snprintf(b, sizeof(b), PT(P_HEALED_FMT), L.healed);
      addMsg(action == BA_GUARD ? PT(P_GUARDS) : itemName(IT_POTION), b, -1);
      enemyMsg();
      break;
    case BA_BALL:
    case BA_GREAT:
      addMsg(itemName(action == BA_GREAT ? IT_GREAT : IT_BALL), PT(P_BALL_SHAKES), -1);
      if (L.ballShakes == 4) addMsg(PT(P_CAUGHT), dexName(e.dex), -1);
      else { addMsg(PT(P_BROKE_FREE), nullptr, -1); enemyMsg(); }
      break;
    default: {
      auto myMsg = [&]() {
        if (L.pDmg < 0) return;
        snprintf(a, sizeof(a), "%s: %s!", petName(), myMove);
        effectText(b, sizeof(b), L.pDmg, L.pEff, L.pMiss);
        be.hp = eEnd;
        addMsg(a, b, 0);
      };
      if (L.playerFirst) { myMsg(); enemyMsg(); }
      else { enemyMsg(); myMsg(); }
      break;
    }
  }
  bp.hp = pEnd;
  be.hp = eEnd;
  if (battle.state == BS_WON) addMsg(PT(P_WON), nullptr, -1);
  else if (battle.state == BS_LOST) addMsg(PT(P_LOST), nullptr, -1);
  if (action == BA_MOVE || action == BA_TACKLE) sfxPlay(SFX_PLAY);
  else sfxPlay(SFX_TAP);
  bmsgT0 = millis();
  battleMenu = 0;
}

// button boxes (inside the round screen)
#define BB_R1Y 334
#define BB_R2Y 382
#define BB_H 42
static bool inBox(int16_t x, int16_t y, int bx, int by, int bw, int bh) {
  return x >= bx && x <= bx + bw && y >= by && y <= by + bh;
}

void battleTap(int16_t x, int16_t y) {
  if (msgsPlaying()) {  // tap skips to the last message
    bmsgT0 = millis() - (uint32_t)(bmsgN - 1) * BMSG_MS;
    return;
  }
  if (battle.state != BS_CHOOSE) {  // result shown: tap to leave
    closeBattle();
    return;
  }
  if (inBox(x, y, 173, 14, 120, 44)) { doAction(BA_RUN); return; }
  int col = inBox(x, y, 66, 0, 162, 466) ? 0 : (inBox(x, y, 238, 0, 162, 466) ? 1 : -1);
  int row = (y >= BB_R1Y && y <= BB_R1Y + BB_H) ? 0 : ((y >= BB_R2Y && y <= BB_R2Y + BB_H) ? 1 : -1);
  if (col < 0 || row < 0) return;
  int k = row * 2 + col;
  if (battleMenu == 0) {
    if (k == 0) doAction(BA_MOVE);
    else if (k == 1) doAction(BA_TACKLE);
    else if (k == 2) doAction(BA_GUARD);
    else { battleMenu = 1; sfxPlay(SFX_TAP); }
  } else {
    if (k == 0) doAction(BA_BALL);
    else if (k == 1) doAction(BA_GREAT);
    else if (k == 2) doAction(BA_POTION);
    else { battleMenu = 0; sfxPlay(SFX_TAP); }
  }
}

static void drawHpBox(int x, int y, const char *name, uint16_t lv, float hp, uint16_t hpMax, bool shiny, bool showNum) {
  gfx->fillRoundRect(x, y, 170, 60, 10, UI_WHITE);
  gfx->drawRoundRect(x, y, 170, 60, 10, UI_INK);
  char t[28];
  snprintf(t, sizeof(t), "%s%s", shiny ? "*" : "", name);
  gfx->setTextColor(UI_INK);
  setSize(textW(t, 2) <= 150 ? 2 : 1);
  setCur(x + 10, y + 8);
  printT(t);
  char l[12];
  snprintf(l, sizeof(l), T(S_LVL_FMT), lv);
  setSize(1);
  setCur(x + 170 - 10 - textW(l, 1), y + 28);
  printT(l);
  int bw = 120, bx = x + 10, by = y + 40;
  gfx->fillRoundRect(bx, by, bw, 9, 3, UI_TRACK);
  int fw = hpMax ? (int)(hp * bw / hpMax) : 0;
  if (fw > bw) fw = bw;
  uint16_t col = hp * 2 > hpMax ? UI_BAR_OK : (hp * 5 > hpMax ? UI_BAR_WARN : UI_BAR_BAD);
  if (fw > 1) gfx->fillRoundRect(bx, by, fw, 9, 3, col);
  if (showNum) {
    char n[12];
    snprintf(n, sizeof(n), "%u/%u", (unsigned)(hp + 0.5f), hpMax);
    setCur(x + 10, y + 28);
    printT(n);
  }
}

static void drawBall(int cx, int cy, int r, int8_t wobble) {
  cx += wobble;
  gfx->fillCircle(cx, cy, r, UI_WHITE);
  for (int dy = -r; dy < 0; dy++) {  // top half red
    int hw = (int)sqrtf((float)(r * r - dy * dy));
    gfx->drawFastHLine(cx - hw, cy + dy, 2 * hw + 1, UI_BAR_BAD);
  }
  gfx->drawFastHLine(cx - r, cy, 2 * r + 1, INK_K);
  gfx->drawCircle(cx, cy, r, INK_K);
  gfx->fillCircle(cx, cy, r / 3, UI_WHITE);
  gfx->drawCircle(cx, cy, r / 3, INK_K);
}

static void drawBattleButton(int x, int y, int w, const char *label, uint16_t col, bool enabled) {
  gfx->fillRoundRect(x, y, w, BB_H, 10, enabled ? col : UI_TRACK);
  gfx->setTextColor(UI_WHITE);
  uint8_t sz = textW(label, 2) <= w - 10 ? 2 : 1;
  setSize(sz);
  setCur(x + (w - textW(label, sz)) / 2, y + (sz == 2 ? 14 : 18));
  printT(label);
}

void renderBattle() {
  uint32_t now = millis();
  int h = sceneHour();
  gNight = h < 6 || h >= 20;
  const Fighter &p = battle.p, &e = battle.e;
  drawScene(DEX_TBL[e.dex].biome, now, gNight);

  uint8_t mi = msgIndex();
  bool playing = mi < bmsgN;
  const BMsg *cur = bmsgN ? &bmsg[playing ? mi : bmsgN - 1] : nullptr;
  float targetP = cur ? cur->hpP : p.hp, targetE = cur ? cur->hpE : e.hp;
  dispP += (targetP - dispP) * 0.35f;
  dispE += (targetE - dispE) * 0.35f;
  if (fabsf(dispP - targetP) < 0.5f) dispP = targetP;
  if (fabsf(dispE - targetE) < 0.5f) dispE = targetE;
  int8_t who = (playing && cur) ? cur->who : -1;
  uint32_t tIn = (now - bmsgT0) % BMSG_MS;

  // wild one: top right, on a little platform
  gfx->fillRoundRect(250, 182, 150, 16, 8, gNight ? 0x3186 : 0x8E8B);
  bool caughtNow = battle.state == BS_CAUGHT && (!playing || mi >= 1);
  bool ballOut = battle.log.ballShakes >= 0 && playing && cur && mi == 0;
  if (caughtNow || ballOut) {
    int8_t wob = (battle.log.ballShakes > 0 && !caughtNow) ? (int8_t)(((now / 120) % 2) ? 4 : -4) : 0;
    drawBall(325, 168, 16, wob);
  } else if (!(battle.state == BS_WON && !playing)) {
    uint8_t act = PMD_IDLE;
    if (who == 1 && galleryPmd.has(PMD_ATTACK)) act = PMD_ATTACK;
    else if (who == 0 && galleryPmd.has(PMD_HURT)) act = PMD_HURT;
    bool blink = who == 0 && (tIn / 90) % 2;  // flashes when hit
    if (!blink) {
      if (galleryPmd.loaded) drawPmdActM(galleryPmd, act, 325, 190, act == PMD_IDLE ? now : tIn, act == PMD_IDLE, false, 3);
      else {
        const uint8_t *t = thumbs.get(e.dex);
        if (t) drawThumb(t, 285, 100, 3, false);
      }
    }
  }
  // the pet: bottom left
  if (!(battle.state == BS_LOST && !playing)) {
    uint8_t act = PMD_IDLE;
    if (who == 0 && pmd.has(PMD_ATTACK)) act = PMD_ATTACK;
    else if (who == 1 && pmd.has(PMD_HURT)) act = PMD_HURT;
    bool blink = who == 1 && (tIn / 90) % 2;
    if (!blink) {
      if (pmd.loaded) drawPmdActM(pmd, act, 140, 282, act == PMD_IDLE ? now : tIn, act == PMD_IDLE, false, 4);
      else {
        const uint8_t *t = thumbs.get(p.dex);
        if (t) drawThumb(t, 100, 172, 3, false);
      }
    }
  }
  drawHpBox(76, 66, dexName(e.dex), e.lv, dispE, e.hpMax, e.shiny, false);
  drawHpBox(234, 214, petName(), p.lv, dispP, p.hpMax, p.shiny, true);

  // RUN (top)
  if (battle.state == BS_CHOOSE && !playing)
    drawBattleButton(173, 14, 120, PT(P_RUN), 0x8410, true);

  // message lines
  gfx->fillRect(0, 286, 466, 180, gNight ? UI_BG_NIGHT : UI_BG_DAY);
  gfx->setTextColor(inkColor());
  if (cur && (playing || battle.state == BS_CHOOSE)) {
    uint8_t sz = textW(cur->a, 2) <= 400 ? 2 : 1;
    setSize(sz);
    setCur(centerX(cur->a, sz), 292);
    printT(cur->a);
    if (cur->b[0]) {
      setSize(2);
      setCur(centerX(cur->b, 2), 312);
      printT(cur->b);
    }
  }

  if (playing) { gfx->flush(); return; }

  if (battle.state != BS_CHOOSE) {  // result
    battleFinish();  // applies rewards once
    const char *r = battle.state == BS_WON ? PT(P_WON) : battle.state == BS_LOST ? PT(P_LOST)
                    : battle.state == BS_CAUGHT ? PT(P_CAUGHT) : PT(P_FLED);
    gfx->fillRoundRect(80, 330, 306, 96, 16, UI_WHITE);
    gfx->drawRoundRect(80, 330, 306, 96, 16, UI_INK);
    gfx->setTextColor(battle.state == BS_LOST ? UI_BAR_BAD : UI_BAR_OK);
    setSize(3);
    setCur(centerX(r, 3), 344);
    printT(r);
    gfx->setTextColor(UI_INK);
    setSize(2);
    if (battle.state == BS_CAUGHT && battle.newCatch) {
      gfx->setTextColor(UI_BAR_WARN);
      setCur(centerX(PT(P_NEW_CATCH), 2), 376);
      printT(PT(P_NEW_CATCH));
      gfx->setTextColor(UI_INK);
    }
    setSize(1);
    setCur(centerX(PT(P_TAP_CONTINUE), 1), 404);
    printT(PT(P_TAP_CONTINUE));
    gfx->flush();
    return;
  }

  // action buttons
  if (battleMenu == 0) {
    drawBattleButton(66, BB_R1Y, 162, moveName(p.t1), DEX_TBL[p.dex].accent, true);
    drawBattleButton(238, BB_R1Y, 162, PT(P_TACKLE), 0x8C4D, true);
    drawBattleButton(96, BB_R2Y, 132, PT(P_GUARD), 0x4C98, true);
    drawBattleButton(238, BB_R2Y, 132, PT(P_BAG), UI_BAR_WARN, true);
  } else {
    char t[24];
    snprintf(t, sizeof(t), "%s x%u", itemName(IT_BALL), plus.items[IT_BALL]);
    drawBattleButton(66, BB_R1Y, 162, t, UI_BAR_BAD, plus.items[IT_BALL] > 0);
    snprintf(t, sizeof(t), "%s x%u", itemName(IT_GREAT), plus.items[IT_GREAT]);
    drawBattleButton(238, BB_R1Y, 162, t, 0x4C98, plus.items[IT_GREAT] > 0);
    snprintf(t, sizeof(t), "%s x%u", itemName(IT_POTION), plus.items[IT_POTION]);
    drawBattleButton(96, BB_R2Y, 132, t, UI_BAR_OK, plus.items[IT_POTION] > 0);
    drawBattleButton(238, BB_R2Y, 132, PT(P_BACK), 0x8410, true);
  }
  gfx->flush();
}

// ---------- main screen extras ----------
void drawAwayScene() {
  uint32_t left = plusExpeditionLeft(nowEpoch());
  gfx->fillRoundRect(78, 140, 310, 130, 18, UI_WHITE);
  gfx->drawRoundRect(78, 140, 310, 130, 18, UI_INK);
  // a little trail of footprints walking off
  uint32_t ph = (millis() / 400) % 6;
  for (uint32_t i = 0; i < 6; i++) {
    uint16_t c = i <= ph ? UI_INK : UI_TRACK;
    gfx->fillCircle(120 + i * 44, 168 + (i % 2) * 10, 5, c);
  }
  char t[36];
  snprintf(t, sizeof(t), PT(P_EXP_LEFT_FMT), (unsigned)((left + 59) / 60));
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(t, 2), 202);
  printT(t);
  if (imuOk()) {
    uint32_t walked = plus.stepsTotal - plus.expStepsBase;
    uint16_t goal = expeditionWalkGoal(plus.expMins);
    snprintf(t, sizeof(t), "%lu/%u", (unsigned long)(walked > goal ? goal : walked), goal);
    gfx->setTextColor(walked >= goal ? UI_BAR_OK : UI_TRACK);
    setCur(centerX(t, 2), 234);
    printT(t);
  }
}

// today's weather: a small icon left of the step counter, rain falling over the
// scene, now and then a flash of lightning in a storm
static const PStrId WX_NAME[WX_COUNT] = { P_WX_SUNNY, P_WX_CLOUDY, P_WX_RAIN, P_WX_STORM };
static void drawCloud(int x, int y, uint16_t c) {
  gfx->fillCircle(x - 7, y + 2, 6, c);
  gfx->fillCircle(x + 2, y - 2, 8, c);
  gfx->fillCircle(x + 10, y + 3, 5, c);
  gfx->fillRect(x - 7, y + 3, 17, 6, c);
}
void drawWeather() {
  uint8_t wx = weatherForEpoch(nowEpoch());
  uint32_t t = millis();
  if (wx == WX_RAIN || wx == WX_STORM) {
    uint16_t drop = gNight ? C565(0x6a, 0x88, 0xb8) : C565(0x9c, 0xc4, 0xe8);
    for (int i = 0; i < 26; i++) {
      int x = (i * 61 + (int)(t / 9) * (1 + i % 3)) % 440 + 13;
      int y = 132 + (i * 97 + (int)(t / 4)) % 170;
      gfx->drawLine(x, y, x - 3, y + 10, drop);
    }
    if (wx == WX_STORM && (t / 120) % 45 == 0) {  // lightning
      uint16_t b = C565(0xff, 0xe8, 0x60);
      gfx->drawLine(330, 126, 312, 160, b);
      gfx->drawLine(312, 160, 326, 160, b);
      gfx->drawLine(326, 160, 304, 204, b);
    }
  }
  int ix = CX - 112, iy = 117;
  switch (wx) {
    case WX_SUNNY:
      gfx->fillCircle(ix, iy, 6, C565(0xf5, 0xc5, 0x18));
      for (int k = 0; k < 8; k++) {
        float a = k * 0.785f;
        gfx->drawLine(ix + (int)(9 * cosf(a)), iy + (int)(9 * sinf(a)), ix + (int)(12 * cosf(a)), iy + (int)(12 * sinf(a)),
                      C565(0xf5, 0xc5, 0x18));
      }
      break;
    case WX_CLOUDY: drawCloud(ix, iy, C565(0xc8, 0xcc, 0xd4)); break;
    default:
      drawCloud(ix, iy - 2, C565(0x8a, 0x90, 0x9c));
      if (wx == WX_STORM) gfx->fillTriangle(ix - 2, iy + 6, ix + 4, iy + 6, ix - 3, iy + 15, C565(0xf5, 0xc5, 0x18));
      else for (int k = 0; k < 3; k++) gfx->drawLine(ix - 6 + k * 6, iy + 9, ix - 8 + k * 6, iy + 14, C565(0x6a, 0x9c, 0xe0));
      break;
  }
  gfx->setTextColor(inkColor());
  setSize(1);
  setCur(ix + 16, iy - 3);
  printT(PT(WX_NAME[wx]));
}

void drawPlusOverlays() {
  if (!pet.awaitingStarter()) drawWeather();
  if (imuOk() && !pet.awaitingStarter()) {  // step counter, small, under the header
    char t[24];
    snprintf(t, sizeof(t), PT(P_STEPS_HUD), (unsigned long)plus.stepsToday);
    gfx->setTextColor(inkColor());
    setSize(1);
    setCur(centerX(t, 1), 114);
    printT(t);
  }
  if (wildOfferUntil) {
    if (!timeLeft(wildOfferUntil) || !plusCanBattle()) wildOfferUntil = 0;
    else {
      gfx->fillRoundRect(70, 124, 326, 64, 14, UI_WHITE);
      gfx->drawRoundRect(70, 124, 326, 64, 14, UI_BAR_BAD);
      char t[36];
      snprintf(t, sizeof(t), PT(P_WILD_APPEARS), dexName(wildDex));
      gfx->setTextColor(UI_BAR_BAD);
      setSize(2);
      setCur(centerX(t, 2), 134);
      printT(t);
      gfx->setTextColor(UI_INK);
      setCur(centerX(PT(P_TAP_TO_FIGHT), 2), 160);
      printT(PT(P_TAP_TO_FIGHT));
    }
  }
  const char *msg = plusToast();
  if (msg) {
    uint8_t sz = textW(msg, 2) <= 340 ? 2 : 1;
    int w = textW(msg, sz) + 30;
    gfx->fillRoundRect(CX - w / 2, 262, w, 38, 12, UI_INK);
    gfx->setTextColor(UI_WHITE);
    setSize(sz);
    setCur(centerX(msg, sz), sz == 2 ? 274 : 278);
    printT(msg);
  }
}

// ---------- auto sleep ----------
// Falls asleep on its own when nobody has touched, shaken or pressed PWR for
// 40 minutes, or when its energy drops to 5 or less (not again for tiredness
// until energy is back above 10, so waking it up yourself still works).
// Never on an expedition, during a battle or minigame, or as an egg.
#define AUTO_SLEEP_IDLE_MS (40UL * 60UL * 1000UL)
#define AUTO_SLEEP_ENERGY 5
static void autoSleep(uint32_t now) {
  static bool tiredArmed = true;
  if (pet.energy > AUTO_SLEEP_ENERGY + 5) tiredArmed = true;
  if (pet.isEgg() || pet.sleeping || pet.away || pet.ceremony || pet.evolving() || pet.awaitingStarter() ||
      battleOpen || gameOpen || sackOpen || pgMode)
    return;
  // signed: a touch stamped later in this same loop pass must not read as 49 days ago
  bool idle = (int32_t)(now - lastUserTouch) >= (int32_t)AUTO_SLEEP_IDLE_MS;
  bool tired = tiredArmed && pet.energy <= AUTO_SLEEP_ENERGY;
  if (!idle && !tired) return;
  if (tired) tiredArmed = false;
  feedMenuUntil = 0;
  wildOfferUntil = 0;
  pet.toggleLight();  // lights out: same as pressing the light button
  plusSetToast(PT(tired ? P_TIRED_SLEEP : P_AUTO_SLEEP), 5000);
  Serial.printf("auto sleep (%s)\n", tired ? "energy" : "idle 40 min");
}

// ---------- per-loop glue ----------
void plusLoop(uint32_t now) {
  imuPoll(now);
  uint16_t st = imuTakeSteps();
  static uint16_t walkAcc = 0;
  if (st) {
    lastUserTouch = now;  // carrying it around counts as being with it (no auto sleep)
    plusAddSteps(st);
    walkAcc += st;
  }
  bool shook = imuShakeEdge();
  if (shook) lastUserTouch = now;  // picked up / shaken: you're around (even with the screen dark)
  if (shook && !screenOff && dimStage == 0) {
    if (pet.isEgg()) {
      eggWiggleUntil = now + 500;
      if (!eggWiggleUntil) eggWiggleUntil = 1;
      sfxPlay(SFX_TAP);
    } else if (mainScreenIdle() && !pet.sleeping && !pet.away) {
      pet.caress();
      chirpPlay(pet.speciesId);
      plusGoal(G_PET);
    }
  }
  plusService(nowEpoch());
  autoSleep(now);

  // wild encounters: now and then on their own, and more often while walking
  if (wildOfferUntil && !timeLeft(wildOfferUntil)) wildOfferUntil = 0;
  bool canOffer = !wildOfferUntil && !screenOff && dimStage == 0 && mainScreenIdle() && plusCanBattle();
  if ((int32_t)(now - plus.nextWildMs) >= 0) {
    plus.nextWildMs = now + (20 + random(25)) * 60000UL;
    if (canOffer) offerWild(now);
  } else if (walkAcc >= 400) {
    walkAcc = 0;
    if (canOffer && random(100) < 40) offerWild(now);
  }
}

uint32_t plusScreenHash() {
  uint32_t h = 2166136261u;
  auto mix = [&](uint32_t v) { h = (h ^ v) * 16777619u; };
  for (uint8_t i = 0; i < IT_COUNT; i++) mix(plus.items[i]);
  for (uint8_t i = 0; i < DAILY_GOALS; i++) { mix(plus.goalType[i]); mix(plus.goalProg[i]); }
  mix(plus.goalsClaimed);
  mix(plus.stepsToday);
  mix(plus.stepsTotal);
  mix(plus.stepMask);
  mix(plus.wins); mix(plus.losses); mix(plus.bestStreak);
  mix((plusExpeditionLeft(nowEpoch()) + 59) / 60);
  mix(pet.away); mix(imuOk()); mix(plusCaughtCount());
  mix(plusFishLeft()); mix(plusSpinsLeft()); mix(pet.sleeping);
  return h;
}

// ---------- extra card pages (4 daily, 5 bag, 6 expedition, 7 steps) ----------
static const PStrId GOAL_TXT[G_COUNT] = { P_GOAL_FEED, P_GOAL_PLAY, P_GOAL_BATH, P_GOAL_PET,
                                          P_GOAL_WIN, P_GOAL_WALK, P_GOAL_CATCH, P_GOAL_TRAIN };
static const uint8_t EXP_MINS[3] = { 15, 30, 60 };

void cardTitle(const char *t) {
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(t, 3), 44);
  printT(t);
}

static void drawCheck(int cx, int cy) {
  gfx->fillCircle(cx, cy, 11, UI_BAR_OK);
  gfx->setTextColor(UI_WHITE);
  setSize(2);
  setCur(cx - 5, cy - 7);
  printT("v");
}

void renderPlusCard(uint8_t page) {
  char t[40];
  if (page == 4) {  // daily goals
    cardTitle(PT(P_DAILY_TITLE));
    for (uint8_t i = 0; i < DAILY_GOALS; i++) {
      uint8_t g = plus.goalType[i];
      int y = 96 + i * 72;
      bool done = goalDone(i);
      gfx->setTextColor(done ? UI_BAR_OK : UI_INK);
      setSize(2);
      setCur(84, y);
      printT(PT(GOAL_TXT[g < G_COUNT ? g : 0]));
      uint16_t tg = goalTarget(g), pr = plus.goalProg[i] > tg ? tg : plus.goalProg[i];
      gfx->fillRoundRect(84, y + 26, 230, 12, 4, UI_TRACK);
      int fw = 230 * pr / (tg ? tg : 1);
      if (fw > 2) gfx->fillRoundRect(84, y + 26, fw, 12, 4, done ? UI_BAR_OK : UI_BAR_WARN);
      if (done) drawCheck(350, y + 30);
      else {
        snprintf(t, sizeof(t), "%u/%u", pr, tg);
        gfx->setTextColor(UI_INK);
        setSize(1);
        setCur(326, y + 28);
        printT(t);
      }
    }
    if (plus.goalsClaimed) {
      gfx->setTextColor(UI_BAR_OK);
      setSize(2);
      setCur(centerX(PT(P_DAILY_DONE), 2), 324);
      printT(PT(P_DAILY_DONE));
    }
  } else if (page == 5) {  // bag
    cardTitle(PT(P_BAG_TITLE));
    static const uint16_t COL[IT_COUNT] = { UI_BAR_BAD, 0x4C98, UI_BAR_OK, UI_BAR_WARN };
    for (uint8_t i = 0; i < IT_COUNT; i++) {
      int y = 92 + i * 52;
      bool usable = (i == IT_POTION || i == IT_TREAT) && plus.items[i];
      gfx->fillRoundRect(80, y, 306, 44, 10, usable ? UI_WHITE : UI_BG_DAY);
      gfx->drawRoundRect(80, y, 306, 44, 10, COL[i]);
      drawItemIcon(i, 104, y + 22, 14);
      gfx->setTextColor(UI_INK);
      setSize(2);
      setCur(124, y + 15);
      printT(itemName(i));
      snprintf(t, sizeof(t), "x%u", plus.items[i]);
      setCur(386 - 14 - textW(t, 2), y + 15);
      printT(t);
    }
    gfx->setTextColor(UI_TRACK);
    setSize(1);
    setCur(centerX(PT(P_TAP_TO_USE), 1), 310);
    printT(PT(P_TAP_TO_USE));
  } else if (page == 6) {  // expedition
    cardTitle(PT(P_EXP_TITLE));
    if (pet.away) {
      uint32_t left = plusExpeditionLeft(nowEpoch());
      snprintf(t, sizeof(t), PT(P_EXP_LEFT_FMT), (unsigned)((left + 59) / 60));
      gfx->setTextColor(UI_INK);
      setSize(2);
      setCur(centerX(t, 2), 140);
      printT(t);
      if (imuOk()) {
        uint32_t walked = plus.stepsTotal - plus.expStepsBase;
        uint16_t goal = expeditionWalkGoal(plus.expMins);
        snprintf(t, sizeof(t), PT(P_EXP_WALK_FMT), goal);
        gfx->setTextColor(UI_TRACK);
        setCur(centerX(t, 2), 190);
        printT(t);
        int fw = 260 * (walked > goal ? goal : walked) / goal;
        gfx->fillRoundRect(103, 220, 260, 14, 5, UI_TRACK);
        if (fw > 2) gfx->fillRoundRect(103, 220, fw, 14, 5, UI_BAR_OK);
      }
    } else {
      bool can = plusCanExpedition();
      for (uint8_t i = 0; i < 3; i++) {
        int y = 96 + i * 66;
        gfx->fillRoundRect(96, y, 274, 54, 12, can ? 0x4C98 : UI_TRACK);
        snprintf(t, sizeof(t), "%u min", EXP_MINS[i]);
        gfx->setTextColor(UI_WHITE);
        setSize(2);
        setCur(centerX(t, 2), y + 8);
        printT(t);
        if (imuOk()) {
          snprintf(t, sizeof(t), PT(P_EXP_WALK_FMT), expeditionWalkGoal(EXP_MINS[i]));
          setSize(1);
          setCur(centerX(t, 1), y + 34);
          printT(t);
        }
      }
      if (!can && !pet.isEgg()) {
        gfx->setTextColor(UI_BAR_BAD);
        setSize(2);
        setCur(centerX(PT(P_EXP_NEED), 2), 300);
        printT(PT(P_EXP_NEED));
      }
    }
  } else if (page == 8) {  // games
    cardTitle(PT(P_GAMES_TITLE));
    bool can = !pet.isEgg() && !pet.away && !pet.sleeping;
    gfx->fillRoundRect(88, 104, 290, 84, 16, can ? 0x4C98 : UI_TRACK);
    drawItemIcon(0, 128, 146, 22);
    gfx->setTextColor(UI_WHITE);
    setSize(3);
    setCur(164, 122);
    printT(PT(P_FISHING));
    snprintf(t, sizeof(t), PT(P_CASTS_FMT), plusFishLeft());
    setSize(1);
    setCur(164, 160);
    printT(t);
    gfx->fillRoundRect(88, 204, 290, 84, 16, can ? UI_BAR_BAD : UI_TRACK);
    drawItemIcon(4, 128, 246, 22);
    setSize(3);
    setCur(164, 222);
    printT(PT(P_SLOTS));
    snprintf(t, sizeof(t), PT(P_SPINS_FMT), plusSpinsLeft());
    setSize(1);
    setCur(164, 260);
    printT(t);
    gfx->setTextColor(UI_TRACK);
    setCur(centerX(PT(P_SPIN_HINT), 1), 306);
    printT(PT(P_SPIN_HINT));
  } else {  // steps
    cardTitle(PT(P_STEPS_TITLE));
    if (!imuOk()) {
      gfx->setTextColor(UI_TRACK);
      setSize(2);
      setCur(centerX(PT(P_NO_SENSOR), 2), 120);
      printT(PT(P_NO_SENSOR));
    }
    snprintf(t, sizeof(t), "%lu", (unsigned long)plus.stepsToday);
    gfx->setTextColor(UI_INK);
    setSize(5);
    setCur(centerX(t, 5), 92);
    printT(t);
    setSize(2);
    gfx->setTextColor(UI_TRACK);
    setCur(centerX(PT(P_STEPS_TODAY), 2), 140);
    printT(PT(P_STEPS_TODAY));
    static const uint16_t AT[3] = { 500, 2000, 5000 };
    for (uint8_t i = 0; i < 3; i++) {
      int x = 96 + i * 96;
      bool got = plus.stepMask & (1 << i);
      gfx->fillRoundRect(x, 176, 84, 40, 10, got ? UI_BAR_OK : UI_TRACK);
      snprintf(t, sizeof(t), "%u", AT[i]);
      gfx->setTextColor(UI_WHITE);
      setCur(x + (84 - textW(t, 2)) / 2, 189);
      printT(t);
    }
    snprintf(t, sizeof(t), "%s %lu", PT(P_STEPS_TOTAL), (unsigned long)plus.stepsTotal);
    gfx->setTextColor(UI_INK);
    setCur(centerX(t, 2), 240);
    printT(t);
    snprintf(t, sizeof(t), PT(P_CAUGHT_FMT), plusCaughtCount());
    setCur(centerX(t, 2), 280);
    printT(t);
  }
}

// taps on the card pages that have buttons; true if handled (card stays open)
bool plusCardTap(int16_t x, int16_t y) {
  if (cardPage == 1) {
    if (inBox(x, y, 84, 300, 144, 44)) {  // training: strength / agility / memory
      cardOpen = false;
      openTrainingMenu();
      return true;
    }
    if (inBox(x, y, 238, 300, 144, 44)) {  // wild battle
      if (!plusCanBattle()) { sfxPlay(SFX_DENY); return true; }
      cardOpen = false;
      startWildRandom();
      return true;
    }
    return false;
  }
  if (cardPage == 5) {
    for (uint8_t i = 0; i < IT_COUNT; i++) {
      if (!inBox(x, y, 80, 92 + i * 52, 306, 44)) continue;
      if (plusUseItem(i)) sfxPlay(i == IT_POTION ? SFX_EAT : SFX_HEART);
      else sfxPlay(SFX_DENY);
      return true;
    }
    return false;
  }
  if (cardPage == 8) {  // games
    if (inBox(x, y, 88, 104, 290, 84)) { cardOpen = false; startFishing(); return true; }
    if (inBox(x, y, 88, 204, 290, 84)) { cardOpen = false; startSlots(); return true; }
    return false;
  }
  if (cardPage == 6 && !pet.away) {
    for (uint8_t i = 0; i < 3; i++) {
      if (!inBox(x, y, 96, 96 + i * 66, 274, 54)) continue;
      if (plusStartExpedition(EXP_MINS[i], nowEpoch())) {
        sfxPlay(SFX_MEDAL);
        cardOpen = false;
      } else {
        sfxPlay(SFX_DENY);
      }
      return true;
    }
    return false;
  }
  return false;
}

// settings -> NEW STARTER (confirmed): the current Pokemon leaves, a new egg
// comes from the starter choice. Pokedex, items, record and steps are kept.
void startNewStarter() {
  clockOpen = false;
  cardOpen = false;
  galleryOpen = false;
  if (battleOpen) { battleOpen = false; galleryPmd.unload(); }
  wildOfferUntil = 0;
  plus.expEnd = 0;
  plus.expMins = 0;
  pet.away = false;
  plusSave();
  starterRegion = 0;
  pet.restartStarter();
  sfxPlay(SFX_HATCH);
}

// ---------- starter screen: Eevee + caught basic-stage Pokemon ----------
// Eevee is always there; every basic-stage species you have caught joins it.
int starterCaughtList(int16_t *out, int max) {
  int n = 0;
  if (out && n < max) out[n] = DEX_EEVEE;
  n++;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    if (d == DEX_EEVEE || !plusCaught(d) || !plusIsBasic(d)) continue;
    if (out && n < max) out[n] = d;
    n++;
  }
  return n;
}

#define SG_X 73
#define SG_Y 100
#define SG_CELL 80
int16_t starterGridAt(int16_t x, int16_t y) {
  if (x < SG_X || y < SG_Y) return 0;
  int c = (x - SG_X) / SG_CELL, r = (y - SG_Y) / SG_CELL;
  if (c > 3 || r > 2) return 0;
  static int16_t list[DEX_COUNT + 1];
  int n = starterCaughtList(list, DEX_COUNT + 1);
  int i = starterGridPage * 12 + r * 4 + c;
  return i < n ? list[i] : 0;
}

void renderStarterCaught() {
  static int16_t list[DEX_COUNT + 1];
  int n = starterCaughtList(list, DEX_COUNT + 1);
  int pages = (n + 11) / 12;
  if (starterGridPage >= pages) starterGridPage = 0;
  for (int k = 0; k < 12; k++) {
    int i = starterGridPage * 12 + k;
    if (i >= n) break;
    int x = SG_X + (k % 4) * SG_CELL, y = SG_Y + (k / 4) * SG_CELL;
    int16_t d = list[i];
    gfx->fillRoundRect(x + 3, y + 3, SG_CELL - 6, SG_CELL - 6, 10, lerp565(DEX_TBL[d].accent, UI_WHITE, 6, 8));
    const uint8_t *th = thumbs.get(d);
    if (th) drawThumb(th, x, y, 2, false);
    else {  // no SD card: the number
      char num[6];
      snprintf(num, sizeof(num), "%d", d);
      gfx->setTextColor(UI_INK);
      setSize(2);
      setCur(x + (SG_CELL - textW(num, 2)) / 2, y + 32);
      printT(num);
    }
  }
  gfx->setTextColor(UI_INK);
  if (pages > 1) {
    setSize(4);
    setCur(28, 218);
    printT("<");
    setCur(414, 218);
    printT(">");
  }
  if (n <= 12) {  // room left: say how to fill it
    setSize(1);
    setCur(centerX(PT(P_CATCH_TO_ADD), 1), 338);
    printT(PT(P_CATCH_TO_ADD));
  }
}
