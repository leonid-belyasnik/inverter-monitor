/******************************************************************************
**
** File      DeviceConfig.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <Arduino.h>

namespace deviceconfig {

constexpr size_t MAX_WIFI_SSID_LENGTH = 32;
constexpr size_t MIN_WIFI_PASSWORD_LENGTH = 8;
constexpr size_t MAX_WIFI_PASSWORD_LENGTH = 63;
constexpr size_t MAX_MQTT_HOST_LENGTH = 64;
constexpr size_t MAX_MQTT_USERNAME_LENGTH = 64;
constexpr size_t MAX_MQTT_PASSWORD_LENGTH = 64;
constexpr size_t MAX_DEVICE_ID_LENGTH = 64;
constexpr uint8_t MIN_MODBUS_SLAVE_ID = 1;
constexpr uint8_t MAX_MODBUS_SLAVE_ID = 247;

// Passwords must never be printed.
struct WifiSettings
{
	String ssid;
	String password;
};

struct MqttSettings
{
	String host;
	uint16_t port = 0;
	String username;
	String password;
};

struct ModbusSettings
{
	uint8_t slaveId = 0;
	uint32_t baud = 0;
};

// Runtime-changeable configuration, stored in NVS.
struct Config
{
	WifiSettings wifi;
	MqttSettings mqtt;
	String deviceId;
	ModbusSettings modbus;
};

bool operator==(const WifiSettings &a, const WifiSettings &b);
bool operator==(const MqttSettings &a, const MqttSettings &b);
bool operator==(const ModbusSettings &a, const ModbusSettings &b);
bool operator==(const Config &a, const Config &b);

// Configuration keys as used by the Serial console ("config set <key> ...").
enum class Key
{
	WifiSsid,
	WifiPassword,
	MqttHost,
	MqttPort,
	MqttUser,
	MqttPassword,
	DeviceId,
	ModbusSlaveId,
	ModbusBaud,
};

constexpr size_t KEY_COUNT = 9;

Key keyAt(size_t index);
const char *keyName(Key key);
bool findKey(const String &name, Key &key);
bool isSecret(Key key);
// Keys that may be empty and are emptied with "config clear <key>".
bool isClearable(Key key);
// Human-readable allowed values, for help output.
const char *keyDescription(Key key);

// Value as shown to the user; secrets are shown as "<set>" or "<empty>".
String displayValue(const Config &config, Key key);

bool sameValue(const Config &a, const Config &b, Key key);

// Parses `text` into `key`. Only checks the syntax of numeric values; use
// validateKey() for the value rules. Returns nullptr on success.
const char *setValue(Config &config, Key key, const String &text);
void clearValue(Config &config, Key key);

// Returns nullptr if the value of `key` is valid, otherwise a short reason
// that never contains any part of the configured values.
const char *validateKey(const Config &config, Key key);

// Validates all keys; on failure sets `badKey` and returns the reason.
const char *validate(const Config &config, Key &badKey);

enum class LoadStatus
{
	Stored,        // valid configuration loaded from NVS
	NotStored,     // nothing saved yet; defaults used
	StoredInvalid, // NVS entry unreadable or invalid; defaults used, entry kept
	NvsError,      // NVS could not be opened; defaults used
};

const char *toString(LoadStatus status);

// Defaults from secrets.h (Wi-Fi, MQTT, device id) and Config.h (Modbus).
Config defaults();

// Fills `config` from NVS, or with defaults() if no valid entry exists.
LoadStatus load(Config &config);

// Validates and writes the configuration as a single NVS entry, then reads
// it back. If validation fails or NVS cannot be opened or written, the
// previously stored entry is left unchanged. Returns nullptr on success.
const char *save(const Config &config);

// Removes the stored entry so defaults() are used. Returns nullptr on
// success, otherwise a short reason.
const char *clear();

} // namespace deviceconfig
