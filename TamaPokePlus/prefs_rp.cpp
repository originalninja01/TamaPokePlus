#if defined(ARDUINO_ARCH_RP2040)
#include "prefs_rp.h"
#include <LittleFS.h>

namespace {
struct Entry {
  char key[16];
  uint8_t len;   // 0xFF = unused slot
  uint8_t data[40];
};
const int MAX_ENTRIES = 96;
Entry gTab[MAX_ENTRIES];
bool gLoaded = false;
bool gFsOk = false;
bool gDirty = false;
const char *PATH = "/tamapoke.prefs";
const uint32_t MAGIC = 0x50544D54;  // "TMTP"

Entry *find(const char *k) {
  for (auto &e : gTab)
    if (e.len != 0xFF && strncmp(e.key, k, sizeof(e.key)) == 0) return &e;
  return nullptr;
}

void loadOnce() {
  if (gLoaded) return;
  gLoaded = true;
  for (auto &e : gTab) e.len = 0xFF;
  gFsOk = LittleFS.begin();  // formats the FS partition on first boot
  if (!gFsOk) { Serial.println("prefs: LittleFS not available (set Flash Size with an FS)"); return; }
  File f = LittleFS.open(PATH, "r");
  if (!f) return;
  uint32_t magic = 0;
  uint16_t n = 0;
  if (f.read((uint8_t *)&magic, 4) != 4 || magic != MAGIC || f.read((uint8_t *)&n, 2) != 2) { f.close(); return; }
  for (uint16_t i = 0; i < n && i < MAX_ENTRIES; i++) {
    Entry e;
    if (f.read((uint8_t *)e.key, sizeof(e.key)) != sizeof(e.key) || f.read(&e.len, 1) != 1 ||
        e.len > sizeof(e.data) || f.read(e.data, e.len) != e.len) break;
    e.key[sizeof(e.key) - 1] = 0;
    gTab[i] = e;
  }
  f.close();
}
}  // namespace

// Writes the store to flash. Call it only from a shallow point of the main
// loop (prefsService), never from deep inside game code: LittleFS needs a fair
// amount of stack when it reorganises its metadata, and the RP2350 main core
// only has a few KB. LittleFS replaces a file atomically when it is closed, so
// a power cut mid-save leaves the previous save intact.
void prefsCommitAll() {
  if (!gDirty || !gFsOk) return;
  uint32_t t0 = millis();
  Serial.println("prefs: saving");
  File f = LittleFS.open(PATH, "w");
  if (!f) { Serial.println("prefs: cannot write"); gDirty = false; return; }
  uint16_t n = 0;
  for (auto &e : gTab) if (e.len != 0xFF) n++;
  f.write((const uint8_t *)&MAGIC, 4);
  f.write((const uint8_t *)&n, 2);
  for (auto &e : gTab) {
    if (e.len == 0xFF) continue;
    f.write((const uint8_t *)e.key, sizeof(e.key));
    f.write(&e.len, 1);
    f.write(e.data, e.len);
  }
  f.close();
  gDirty = false;
  Serial.printf("prefs: saved (%u keys, %lu ms)\n", n, (unsigned long)(millis() - t0));
}

// called once per pass of loop(): performs a save that game code requested
void prefsService() { prefsCommitAll(); }

bool Preferences::begin(const char *, bool readOnly, const char *) {
  loadOnce();
  _ro = readOnly;
  return true;
}

// deferred: the write happens in prefsService() from the top of loop()
void Preferences::end() {}
bool Preferences::commit() { return true; }

bool Preferences::clear() {
  loadOnce();
  for (auto &e : gTab) e.len = 0xFF;
  gDirty = true;
  return true;
}

bool Preferences::remove(const char *k) {
  loadOnce();
  Entry *e = find(k);
  if (!e) return false;
  e->len = 0xFF;
  gDirty = true;
  return true;
}

bool Preferences::isKey(const char *k) {
  loadOnce();
  return find(k) != nullptr;
}

size_t Preferences::put(const char *k, const void *v, size_t len) {
  loadOnce();
  if (_ro || len > sizeof(Entry::data) || strlen(k) >= sizeof(Entry::key)) return 0;
  Entry *e = find(k);
  if (e && e->len == len && memcmp(e->data, v, len) == 0) return len;  // unchanged: no flash write
  if (!e) {
    for (auto &s : gTab)
      if (s.len == 0xFF) { e = &s; break; }
    if (!e) { Serial.println("prefs: table full"); return 0; }
    memset(e->key, 0, sizeof(e->key));
    strncpy(e->key, k, sizeof(e->key) - 1);
  }
  e->len = (uint8_t)len;
  memcpy(e->data, v, len);
  gDirty = true;
  return len;
}

bool Preferences::get(const char *k, void *out, size_t len) {
  loadOnce();
  Entry *e = find(k);
  if (!e || e->len != len) return false;
  memcpy(out, e->data, len);
  return true;
}

size_t Preferences::putString(const char *k, const char *v) {
  size_t n = strlen(v);
  if (n + 1 > sizeof(Entry::data)) n = sizeof(Entry::data) - 1;
  char tmp[sizeof(Entry::data)];
  memcpy(tmp, v, n);
  tmp[n] = 0;
  return put(k, tmp, n + 1);
}

size_t Preferences::getString(const char *k, char *out, size_t maxLen) {
  loadOnce();
  Entry *e = find(k);
  if (!e || !maxLen) return 0;
  size_t n = e->len < maxLen ? e->len : maxLen;
  memcpy(out, e->data, n);
  out[n - 1] = 0;
  return n;
}

String Preferences::getString(const char *k, const String &d) {
  char buf[sizeof(Entry::data)];
  return getString(k, buf, sizeof(buf)) ? String(buf) : d;
}

size_t Preferences::getBytes(const char *k, void *out, size_t maxLen) {
  loadOnce();
  Entry *e = find(k);
  if (!e) return 0;
  size_t n = e->len < maxLen ? e->len : maxLen;
  memcpy(out, e->data, n);
  return n;
}
#endif
