#pragma once
#if defined(ARDUINO_ARCH_RP2040)
#include <Arduino.h>

// Minimal stand-in for the ESP32 "Preferences" (NVS) class, for arduino-pico.
//
// Keys live in RAM and are written to one small file on the internal-flash
// LittleFS (/tamapoke.prefs) by commit(), which end() also calls. The file is
// replaced atomically (write a temp file, then rename), so a power cut while
// saving leaves the previous save intact.
//
// Only the calls TamaPoke makes are implemented. All namespaces share one
// store (TamaPoke only uses "tamapoke").
class Preferences {
public:
  bool begin(const char *name, bool readOnly = false, const char *partition = nullptr);
  void end();
  bool commit();  // write to flash now if anything changed
  bool clear();
  bool remove(const char *key);
  bool isKey(const char *key);

  size_t putChar(const char *k, int8_t v) { return put(k, &v, 1); }
  size_t putUChar(const char *k, uint8_t v) { return put(k, &v, 1); }
  size_t putShort(const char *k, int16_t v) { return put(k, &v, 2); }
  size_t putUShort(const char *k, uint16_t v) { return put(k, &v, 2); }
  size_t putInt(const char *k, int32_t v) { return put(k, &v, 4); }
  size_t putUInt(const char *k, uint32_t v) { return put(k, &v, 4); }
  size_t putBool(const char *k, bool v) { uint8_t b = v; return put(k, &b, 1); }
  size_t putString(const char *k, const char *v);
  size_t putString(const char *k, const String &v) { return putString(k, v.c_str()); }
  size_t putBytes(const char *k, const void *v, size_t len) { return put(k, v, len); }

  int8_t getChar(const char *k, int8_t d = 0) { get(k, &d, 1); return d; }
  uint8_t getUChar(const char *k, uint8_t d = 0) { get(k, &d, 1); return d; }
  int16_t getShort(const char *k, int16_t d = 0) { get(k, &d, 2); return d; }
  uint16_t getUShort(const char *k, uint16_t d = 0) { get(k, &d, 2); return d; }
  int32_t getInt(const char *k, int32_t d = 0) { get(k, &d, 4); return d; }
  uint32_t getUInt(const char *k, uint32_t d = 0) { get(k, &d, 4); return d; }
  bool getBool(const char *k, bool d = false) { uint8_t b = d; get(k, &b, 1); return b != 0; }
  size_t getString(const char *k, char *out, size_t maxLen);
  String getString(const char *k, const String &d = String());
  size_t getBytes(const char *k, void *out, size_t maxLen);

private:
  size_t put(const char *k, const void *v, size_t len);
  bool get(const char *k, void *out, size_t len);  // only if the stored size matches
  bool _ro = false;
};

// writes any pending change (safe to call often; it only touches flash when
// dirty). Main core only, from a shallow stack: see prefs_rp.cpp.
void prefsCommitAll();
void prefsService();
#endif
