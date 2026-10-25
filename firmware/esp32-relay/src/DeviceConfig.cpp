/******************************************************************************
**
** File      DeviceConfig.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "DeviceConfig.h"

#include <Preferences.h>

#include <cstring>

#include "Config.h"
#include "secrets.h"

namespace deviceconfig {

namespace {

constexpr const char *NVS_NAMESPACE = "hpg-config";
constexpr const char *NVS_KEY = "cfg";
constexpr uint8_t BLOB_VERSION = 1;

constexpr uint32_t SUPPORTED_BAUD_RATES[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};

// Stored as one NVS blob so an update replaces all fields at once; NVS
// keeps the previous entry until the new one has been written.
struct StoredBlob
{
	uint8_t version;
	uint8_t modbusSlaveId;
	uint16_t mqttPort;
	uint32_t modbusBaud;
	char wifiSsid[MAX_WIFI_SSID_LENGTH + 1];
	char wifiPassword[MAX_WIFI_PASSWORD_LENGTH + 1];
	char mqttHost[MAX_MQTT_HOST_LENGTH + 1];
	char mqttUsername[MAX_MQTT_USERNAME_LENGTH + 1];
	char mqttPassword[MAX_MQTT_PASSWORD_LENGTH + 1];
	char deviceId[MAX_DEVICE_ID_LENGTH + 1];
};

struct KeyInfo
{
	Key key;
	const char *name;
	bool secret;
	bool clearable;
	const char *description;
};

// Order must match enum class Key.
constexpr KeyInfo KEYS[KEY_COUNT] = {
    {Key::WifiSsid, "wifi.ssid", false, false,
     "1-32 printable ASCII characters, spaces allowed inside"},
    {Key::WifiPassword, "wifi.password", true, true,
     "8-63 printable ASCII characters, spaces allowed inside; empty = open network"},
    {Key::MqttHost, "mqtt.host", false, false,
     "host name or IPv4 address, 1-64 characters: letters, digits, '.', '-', '_'"},
    {Key::MqttPort, "mqtt.port", false, false, "1-65535"},
    {Key::MqttUser, "mqtt.user", false, true,
     "0-64 printable ASCII characters; empty = anonymous; required if mqtt.password is set"},
    {Key::MqttPassword, "mqtt.password", true, true,
     "0-64 printable ASCII characters, spaces allowed inside"},
    {Key::DeviceId, "device.id", false, false,
     "1-64 characters: letters, digits, '.', '-', '_' (used in the MQTT topic)"},
    {Key::ModbusSlaveId, "modbus.slave_id", false, false, "1-247"},
    {Key::ModbusBaud, "modbus.baud", false, false,
     "1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 (8-N-1)"},
};

const KeyInfo &info(Key key)
{
	return KEYS[static_cast<size_t>(key)];
}

// Printable ASCII without leading or trailing spaces.
bool isCleanText(const String &value)
{
	for (size_t i = 0; i < value.length(); ++i) {
		const char c = value[i];
		if (c < 0x20 || c > 0x7E) {
			return false;
		}
	}
	return value.length() == 0 || (value[0] != ' ' && value[value.length() - 1] != ' ');
}

bool isNameChar(char c)
{
	return isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_';
}

bool isName(const String &value)
{
	for (size_t i = 0; i < value.length(); ++i) {
		if (!isNameChar(value[i])) {
			return false;
		}
	}
	return true;
}

bool isSupportedBaud(uint32_t baud)
{
	for (const uint32_t supported : SUPPORTED_BAUD_RATES) {
		if (baud == supported) {
			return true;
		}
	}
	return false;
}

// Plain decimal digits only, at most 9 so the value cannot overflow.
bool parseUnsigned(const String &text, uint32_t &value)
{
	if (text.isEmpty() || text.length() > 9) {
		return false;
	}

	value = 0;
	for (size_t i = 0; i < text.length(); ++i) {
		const char c = text[i];
		if (c < '0' || c > '9') {
			return false;
		}
		value = value * 10 + static_cast<uint32_t>(c - '0');
	}
	return true;
}

bool fieldTerminated(const char *field, size_t size)
{
	return memchr(field, '\0', size) != nullptr;
}

void copyField(char *destination, size_t size, const String &value)
{
	strncpy(destination, value.c_str(), size - 1);
	destination[size - 1] = '\0';
}

} // namespace

bool operator==(const WifiSettings &a, const WifiSettings &b)
{
	return a.ssid == b.ssid && a.password == b.password;
}

bool operator==(const MqttSettings &a, const MqttSettings &b)
{
	return a.host == b.host && a.port == b.port && a.username == b.username &&
	       a.password == b.password;
}

bool operator==(const ModbusSettings &a, const ModbusSettings &b)
{
	return a.slaveId == b.slaveId && a.baud == b.baud;
}

bool operator==(const Config &a, const Config &b)
{
	return a.wifi == b.wifi && a.mqtt == b.mqtt && a.deviceId == b.deviceId &&
	       a.modbus == b.modbus;
}

Key keyAt(size_t index)
{
	return KEYS[index].key;
}

const char *keyName(Key key)
{
	return info(key).name;
}

bool findKey(const String &name, Key &key)
{
	for (const KeyInfo &entry : KEYS) {
		if (name == entry.name) {
			key = entry.key;
			return true;
		}
	}
	return false;
}

bool isSecret(Key key)
{
	return info(key).secret;
}

bool isClearable(Key key)
{
	return info(key).clearable;
}

const char *keyDescription(Key key)
{
	return info(key).description;
}

String displayValue(const Config &config, Key key)
{
	String value;

	switch (key) {
		case Key::WifiSsid:
			value = config.wifi.ssid;
			break;
		case Key::WifiPassword:
			return config.wifi.password.isEmpty() ? "<empty>" : "<set>";
		case Key::MqttHost:
			value = config.mqtt.host;
			break;
		case Key::MqttPort:
			return String(static_cast<unsigned>(config.mqtt.port));
		case Key::MqttUser:
			value = config.mqtt.username;
			break;
		case Key::MqttPassword:
			return config.mqtt.password.isEmpty() ? "<empty>" : "<set>";
		case Key::DeviceId:
			value = config.deviceId;
			break;
		case Key::ModbusSlaveId:
			return String(static_cast<unsigned>(config.modbus.slaveId));
		case Key::ModbusBaud:
			return String(static_cast<unsigned long>(config.modbus.baud));
	}

	return value.isEmpty() ? String("<empty>") : value;
}

bool sameValue(const Config &a, const Config &b, Key key)
{
	switch (key) {
		case Key::WifiPassword:
			return a.wifi.password == b.wifi.password;
		case Key::MqttPassword:
			return a.mqtt.password == b.mqtt.password;
		default:
			return displayValue(a, key) == displayValue(b, key);
	}
}

const char *setValue(Config &config, Key key, const String &text)
{
	uint32_t number = 0;

	switch (key) {
		case Key::WifiSsid:
			config.wifi.ssid = text;
			break;
		case Key::WifiPassword:
			config.wifi.password = text;
			break;
		case Key::MqttHost:
			config.mqtt.host = text;
			break;
		case Key::MqttPort:
			if (!parseUnsigned(text, number) || number == 0 || number > 65535) {
				return "must be a decimal number 1..65535";
			}
			config.mqtt.port = static_cast<uint16_t>(number);
			break;
		case Key::MqttUser:
			config.mqtt.username = text;
			break;
		case Key::MqttPassword:
			config.mqtt.password = text;
			break;
		case Key::DeviceId:
			config.deviceId = text;
			break;
		case Key::ModbusSlaveId:
			if (!parseUnsigned(text, number) || number < MIN_MODBUS_SLAVE_ID ||
			    number > MAX_MODBUS_SLAVE_ID) {
				return "must be a decimal number 1..247";
			}
			config.modbus.slaveId = static_cast<uint8_t>(number);
			break;
		case Key::ModbusBaud:
			if (!parseUnsigned(text, number) || !isSupportedBaud(number)) {
				return "must be one of 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200";
			}
			config.modbus.baud = number;
			break;
	}

	return nullptr;
}

void clearValue(Config &config, Key key)
{
	switch (key) {
		case Key::WifiPassword:
			config.wifi.password = "";
			break;
		case Key::MqttUser:
			config.mqtt.username = "";
			break;
		case Key::MqttPassword:
			config.mqtt.password = "";
			break;
		default:
			break;
	}
}

const char *validateKey(const Config &config, Key key)
{
	switch (key) {
		case Key::WifiSsid:
			if (config.wifi.ssid.isEmpty()) {
				return "must not be empty";
			}
			if (config.wifi.ssid.length() > MAX_WIFI_SSID_LENGTH) {
				return "is too long (max 32 characters)";
			}
			if (!isCleanText(config.wifi.ssid)) {
				return "must be printable ASCII without leading/trailing spaces";
			}
			return nullptr;
		case Key::WifiPassword:
			if (config.wifi.password.isEmpty()) {
				return nullptr;
			}
			if (config.wifi.password.length() < MIN_WIFI_PASSWORD_LENGTH) {
				return "must be empty or at least 8 characters";
			}
			if (config.wifi.password.length() > MAX_WIFI_PASSWORD_LENGTH) {
				return "is too long (max 63 characters)";
			}
			if (!isCleanText(config.wifi.password)) {
				return "must be printable ASCII without leading/trailing spaces";
			}
			return nullptr;
		case Key::MqttHost:
			if (config.mqtt.host.isEmpty()) {
				return "must not be empty";
			}
			if (config.mqtt.host.length() > MAX_MQTT_HOST_LENGTH) {
				return "is too long (max 64 characters)";
			}
			if (!isName(config.mqtt.host)) {
				return "contains invalid characters";
			}
			return nullptr;
		case Key::MqttPort:
			return config.mqtt.port == 0 ? "must be 1..65535" : nullptr;
		case Key::MqttUser:
			if (config.mqtt.username.length() > MAX_MQTT_USERNAME_LENGTH) {
				return "is too long (max 64 characters)";
			}
			if (!isCleanText(config.mqtt.username)) {
				return "must be printable ASCII without leading/trailing spaces";
			}
			if (config.mqtt.username.isEmpty() && !config.mqtt.password.isEmpty()) {
				return "is required when mqtt.password is set";
			}
			return nullptr;
		case Key::MqttPassword:
			if (config.mqtt.password.length() > MAX_MQTT_PASSWORD_LENGTH) {
				return "is too long (max 64 characters)";
			}
			if (!isCleanText(config.mqtt.password)) {
				return "must be printable ASCII without leading/trailing spaces";
			}
			return nullptr;
		case Key::DeviceId:
			if (config.deviceId.isEmpty()) {
				return "must not be empty";
			}
			if (config.deviceId.length() > MAX_DEVICE_ID_LENGTH) {
				return "is too long (max 64 characters)";
			}
			if (!isName(config.deviceId)) {
				return "contains invalid characters";
			}
			return nullptr;
		case Key::ModbusSlaveId:
			return config.modbus.slaveId < MIN_MODBUS_SLAVE_ID ||
			               config.modbus.slaveId > MAX_MODBUS_SLAVE_ID
			           ? "must be 1..247"
			           : nullptr;
		case Key::ModbusBaud:
			return isSupportedBaud(config.modbus.baud) ? nullptr : "is not a supported baud rate";
	}

	return "unknown key";
}

const char *validate(const Config &config, Key &badKey)
{
	for (const KeyInfo &entry : KEYS) {
		const char *reason = validateKey(config, entry.key);
		if (reason != nullptr) {
			badKey = entry.key;
			return reason;
		}
	}
	return nullptr;
}

const char *toString(LoadStatus status)
{
	switch (status) {
		case LoadStatus::Stored:
			return "stored";
		case LoadStatus::NotStored:
			return "not_stored";
		case LoadStatus::StoredInvalid:
			return "stored_invalid";
		case LoadStatus::NvsError:
			return "nvs_error";
	}

	return "unknown";
}

Config defaults()
{
	Config result;
	result.wifi.ssid = secrets::WIFI_SSID;
	result.wifi.password = secrets::WIFI_PASSWORD;
	result.mqtt.host = secrets::MQTT_HOST;
	result.mqtt.port = secrets::MQTT_PORT;
	result.mqtt.username = secrets::MQTT_USERNAME;
	result.mqtt.password = secrets::MQTT_PASSWORD;
	result.deviceId = secrets::DEVICE_ID;
	result.modbus.slaveId = config::MODBUS_SLAVE_ID;
	result.modbus.baud = config::MODBUS_BAUD;
	return result;
}

LoadStatus load(Config &config)
{
	config = defaults();

	Preferences preferences;
	// Read-write so the namespace is created on first boot; opening a
	// missing namespace read-only fails like a real NVS error would.
	if (!preferences.begin(NVS_NAMESPACE, false)) {
		return LoadStatus::NvsError;
	}

	if (!preferences.isKey(NVS_KEY)) {
		preferences.end();
		return LoadStatus::NotStored;
	}

	StoredBlob blob{};
	const bool read = preferences.getBytesLength(NVS_KEY) == sizeof(blob) &&
	                  preferences.getBytes(NVS_KEY, &blob, sizeof(blob)) == sizeof(blob);
	preferences.end();

	Config stored;
	bool valid = read && blob.version == BLOB_VERSION &&
	             fieldTerminated(blob.wifiSsid, sizeof(blob.wifiSsid)) &&
	             fieldTerminated(blob.wifiPassword, sizeof(blob.wifiPassword)) &&
	             fieldTerminated(blob.mqttHost, sizeof(blob.mqttHost)) &&
	             fieldTerminated(blob.mqttUsername, sizeof(blob.mqttUsername)) &&
	             fieldTerminated(blob.mqttPassword, sizeof(blob.mqttPassword)) &&
	             fieldTerminated(blob.deviceId, sizeof(blob.deviceId));
	if (valid) {
		stored.wifi.ssid = blob.wifiSsid;
		stored.wifi.password = blob.wifiPassword;
		stored.mqtt.host = blob.mqttHost;
		stored.mqtt.port = blob.mqttPort;
		stored.mqtt.username = blob.mqttUsername;
		stored.mqtt.password = blob.mqttPassword;
		stored.deviceId = blob.deviceId;
		stored.modbus.slaveId = blob.modbusSlaveId;
		stored.modbus.baud = blob.modbusBaud;
		Key badKey;
		valid = validate(stored, badKey) == nullptr;
	}
	memset(&blob, 0, sizeof(blob));

	if (!valid) {
		return LoadStatus::StoredInvalid;
	}

	config = stored;
	return LoadStatus::Stored;
}

const char *save(const Config &config)
{
	Key badKey;
	if (validate(config, badKey) != nullptr) {
		return "configuration is not valid";
	}

	StoredBlob blob{};
	blob.version = BLOB_VERSION;
	blob.modbusSlaveId = config.modbus.slaveId;
	blob.mqttPort = config.mqtt.port;
	blob.modbusBaud = config.modbus.baud;
	copyField(blob.wifiSsid, sizeof(blob.wifiSsid), config.wifi.ssid);
	copyField(blob.wifiPassword, sizeof(blob.wifiPassword), config.wifi.password);
	copyField(blob.mqttHost, sizeof(blob.mqttHost), config.mqtt.host);
	copyField(blob.mqttUsername, sizeof(blob.mqttUsername), config.mqtt.username);
	copyField(blob.mqttPassword, sizeof(blob.mqttPassword), config.mqtt.password);
	copyField(blob.deviceId, sizeof(blob.deviceId), config.deviceId);

	const char *error = nullptr;
	Preferences preferences;
	if (!preferences.begin(NVS_NAMESPACE, false)) {
		error = "NVS open failed";
	} else {
		if (preferences.putBytes(NVS_KEY, &blob, sizeof(blob)) != sizeof(blob)) {
			error = "NVS write failed";
		} else {
			StoredBlob readBack{};
			if (preferences.getBytes(NVS_KEY, &readBack, sizeof(readBack)) != sizeof(readBack) ||
			    memcmp(&readBack, &blob, sizeof(blob)) != 0) {
				error = "NVS read-back verification failed";
			}
			memset(&readBack, 0, sizeof(readBack));
		}
		preferences.end();
	}

	memset(&blob, 0, sizeof(blob));
	return error;
}

const char *clear()
{
	Preferences preferences;
	if (!preferences.begin(NVS_NAMESPACE, false)) {
		return "NVS open failed";
	}

	const char *error = nullptr;
	if (preferences.isKey(NVS_KEY) && !preferences.remove(NVS_KEY)) {
		error = "NVS remove failed";
	}
	preferences.end();

	return error;
}

} // namespace deviceconfig
