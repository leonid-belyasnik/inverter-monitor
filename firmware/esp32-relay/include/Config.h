/******************************************************************************
**
** File      Config.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <cstdint>
#include <ctime>

// Non-secret deployment knobs. Wi-Fi credentials, MQTT broker address, and
// the device id live in secrets.h (gitignored); see secrets.example.h.
namespace config {

// ---- RS485 / Modbus wiring (verified against the physical inverter) ----
constexpr int RS485_TX_PIN = 17;
constexpr int RS485_RX_PIN = 18;
constexpr int RS485_DE_PIN = 21;

constexpr uint32_t MODBUS_BAUD = 9600;
constexpr uint8_t MODBUS_SLAVE_ID = 1;
constexpr uint32_t MODBUS_RESPONSE_TIMEOUT_MS = 500;

// Settle time between consecutive Modbus requests on the same bus.
constexpr uint32_t MODBUS_INTER_FRAME_DELAY_MS = 100;

// ---- Telemetry polling / publishing ----
// How often verified registers are read and a publish attempt is made.
constexpr uint32_t TELEMETRY_INTERVAL_MS = 5000;

// How often the candidate-register diagnostic scan runs. Kept slower than
// the telemetry interval so the extra reads do not starve Wi-Fi/MQTT
// servicing or the verified-register poll.
constexpr uint32_t DIAGNOSTIC_INTERVAL_MS = 15000;

// Prints raw candidate-register readings to Serial. Does not affect what is
// published to MQTT; see RegisterMap.h for the production/candidate split.
constexpr bool DIAGNOSTIC_MODE_ENABLED = true;

// ---- Wi-Fi / MQTT reconnection (bounded, non-blocking) ----
// Time allowed for one Wi-Fi connection attempt before it is restarted.
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
// Minimum gap between MQTT connection attempts.
constexpr uint32_t MQTT_RETRY_INTERVAL_MS = 5000;

// ---- Time synchronization ----
constexpr const char *NTP_SERVER_PRIMARY = "pool.ntp.org";
constexpr const char *NTP_SERVER_SECONDARY = "time.google.com";

// Sanity floor used to decide whether the system clock reflects a real NTP
// sync (2024-01-01T00:00:00Z) rather than the ESP32's power-on epoch. This
// is a plausibility check, not a precision guarantee.
constexpr time_t MIN_VALID_EPOCH_SECONDS = 1704067200;

// ---- Serial logging ----
// Minimum gap between repeated "publish skipped" diagnostic log lines, to
// avoid flooding the serial console while telemetry is withheld.
constexpr uint32_t SKIP_LOG_INTERVAL_MS = 10000;

} // namespace config
