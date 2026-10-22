#pragma once

#include <Arduino.h>

// Wraps ESP32 SNTP time synchronization. Local time is never reported as
// UTC until a plausible synced time has been observed: callers must check
// isSynced() before trusting nowIso8601Utc().
namespace timesync {

// Starts the SNTP client. Safe to call before Wi-Fi is connected; SNTP will
// sync opportunistically once network access is available. Call once from
// setup().
void begin();

// True once the system clock holds a plausible post-sync UTC time (see
// Config::MIN_VALID_EPOCH_SECONDS). False means "do not trust the clock
// yet" -- the device's local time still reflects its power-on epoch.
bool isSynced();

// Formats the current UTC time as "YYYY-MM-DDTHH:MM:SS.sssZ", matching the
// simulator's measured_at format exactly. Only meaningful when isSynced()
// is true; returns an empty string otherwise so callers cannot accidentally
// publish an unsynchronized timestamp labeled as UTC.
String nowIso8601Utc();

} // namespace timesync
