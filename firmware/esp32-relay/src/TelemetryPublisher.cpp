/******************************************************************************
**
** File      TelemetryPublisher.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "TelemetryPublisher.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include <atomic>

#include "Config.h"
#include "secrets.h"

namespace telemetry {

namespace {

constexpr uint16_t MQTT_BUFFER_SIZE = 512;
constexpr size_t JSON_DOCUMENT_CAPACITY = 384;
constexpr size_t PAYLOAD_BUFFER_SIZE = 384;

// ---- Wi-Fi event diagnostics ----
// The Arduino-ESP32 2.x event handler runs in the "arduino_events" task, so
// it only records events into a single-producer/single-consumer ring buffer.
// Logging happens later from loop() in the Arduino task.
constexpr uint32_t WIFI_EVENT_QUEUE_SIZE = 16;

struct WifiEventRecord
{
	arduino_event_id_t id;
	uint8_t reason;
};

WifiEventRecord s_wifiEvents[WIFI_EVENT_QUEUE_SIZE];
std::atomic<uint32_t> s_wifiEventHead{0};
std::atomic<uint32_t> s_wifiEventTail{0};
std::atomic<uint32_t> s_wifiEventsDropped{0};

// WiFi.onEvent() must not race with event delivery, so the handler is
// registered once for the lifetime of the program.
bool s_wifiEventHandlerRegistered = false;

// ---- Wi-Fi scan diagnostics ----
// After a NO_AP_FOUND / AUTH_EXPIRE / AUTH_FAIL disconnect, one asynchronous
// scan is run (at most once per WIFI_SCAN_MIN_INTERVAL_MS, never while
// connected) and its result is printed. Only touched from loop().
constexpr uint32_t WIFI_SCAN_MIN_INTERVAL_MS = 60000;
constexpr int WIFI_SCAN_MAX_PRINTED = 20;

bool s_wifiScanRequested = false;
bool s_wifiScanRunning = false;
bool s_wifiScanEverStarted = false;
uint32_t s_wifiScanStartedMs = 0;

bool isScanTriggerReason(uint8_t reason)
{
	return reason == WIFI_REASON_NO_AP_FOUND || reason == WIFI_REASON_AUTH_EXPIRE ||
	       reason == WIFI_REASON_AUTH_FAIL;
}

const char *authModeName(wifi_auth_mode_t mode)
{
	switch (mode) {
		case WIFI_AUTH_OPEN:
			return "OPEN";
		case WIFI_AUTH_WEP:
			return "WEP";
		case WIFI_AUTH_WPA_PSK:
			return "WPA_PSK";
		case WIFI_AUTH_WPA2_PSK:
			return "WPA2_PSK";
		case WIFI_AUTH_WPA_WPA2_PSK:
			return "WPA_WPA2_PSK";
		case WIFI_AUTH_WPA2_ENTERPRISE:
			return "WPA2_ENTERPRISE";
		case WIFI_AUTH_WPA3_PSK:
			return "WPA3_PSK";
		case WIFI_AUTH_WPA2_WPA3_PSK:
			return "WPA2_WPA3_PSK";
		case WIFI_AUTH_WAPI_PSK:
			return "WAPI_PSK";
		default:
			return "OTHER";
	}
}

void onWifiEvent(arduino_event_t *event)
{
	uint8_t reason = 0;

	switch (event->event_id) {
		case ARDUINO_EVENT_WIFI_STA_CONNECTED:
		case ARDUINO_EVENT_WIFI_STA_GOT_IP:
		case ARDUINO_EVENT_WIFI_STA_LOST_IP:
			break;
		case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
			reason = event->event_info.wifi_sta_disconnected.reason;
			break;
		default:
			return;
	}

	const uint32_t head = s_wifiEventHead.load(std::memory_order_relaxed);
	const uint32_t tail = s_wifiEventTail.load(std::memory_order_acquire);

	if (head - tail >= WIFI_EVENT_QUEUE_SIZE) {
		s_wifiEventsDropped.fetch_add(1, std::memory_order_relaxed);
		return;
	}

	s_wifiEvents[head % WIFI_EVENT_QUEUE_SIZE] = {event->event_id, reason};
	s_wifiEventHead.store(head + 1, std::memory_order_release);
}

void logWifiEvents()
{
	const uint32_t dropped = s_wifiEventsDropped.exchange(0, std::memory_order_relaxed);
	if (dropped != 0) {
		Serial.printf("Wi-Fi events dropped: %lu\n", static_cast<unsigned long>(dropped));
	}

	uint32_t tail = s_wifiEventTail.load(std::memory_order_relaxed);
	const uint32_t head = s_wifiEventHead.load(std::memory_order_acquire);

	while (tail != head) {
		const WifiEventRecord record = s_wifiEvents[tail % WIFI_EVENT_QUEUE_SIZE];
		++tail;
		s_wifiEventTail.store(tail, std::memory_order_release);

		const int status = static_cast<int>(WiFi.status());

		switch (record.id) {
			case ARDUINO_EVENT_WIFI_STA_CONNECTED:
				Serial.printf("Wi-Fi event: STA_CONNECTED, status=%d\n", status);
				break;
			case ARDUINO_EVENT_WIFI_STA_GOT_IP:
				if (status == WL_CONNECTED) {
					Serial.printf("Wi-Fi event: STA_GOT_IP, status=%d, IP %s, RSSI %d dBm\n",
					              status, WiFi.localIP().toString().c_str(), WiFi.RSSI());
				} else {
					Serial.printf("Wi-Fi event: STA_GOT_IP, status=%d\n", status);
				}
				break;
			case ARDUINO_EVENT_WIFI_STA_LOST_IP:
				Serial.printf("Wi-Fi event: STA_LOST_IP, status=%d\n", status);
				break;
			case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
				Serial.printf(
				    "Wi-Fi event: STA_DISCONNECTED, reason=%u (%s), status=%d\n", record.reason,
				    WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(record.reason)),
				    status);
				if (isScanTriggerReason(record.reason)) {
					s_wifiScanRequested = true;
				}
				break;
			default:
				break;
		}
	}
}

void printWifiScanResult(int16_t count)
{
	const int shown = count < WIFI_SCAN_MAX_PRINTED ? count : WIFI_SCAN_MAX_PRINTED;
	int configuredMatches = 0;

	Serial.printf("--- Wi-Fi scan: %d network(s) found ---\n", count);

	for (int i = 0; i < count; ++i) {
		const String ssid = WiFi.SSID(i);
		const bool isConfigured = ssid == secrets::WIFI_SSID;
		if (isConfigured) {
			++configuredMatches;
		}

		if (i < shown) {
			Serial.printf("  [%d] SSID \"%s\" ch=%d rssi=%d dBm auth=%s%s\n", i,
			              ssid.length() != 0 ? ssid.c_str() : "<hidden>",
			              static_cast<int>(WiFi.channel(i)), static_cast<int>(WiFi.RSSI(i)),
			              authModeName(WiFi.encryptionType(i)),
			              isConfigured ? "  <-- configured SSID" : "");
		}
	}

	if (count > shown) {
		Serial.printf("  ... %d more not listed\n", count - shown);
	}

	if (configuredMatches > 0) {
		Serial.printf("Configured SSID: FOUND (%d AP(s))\n", configuredMatches);
	} else {
		Serial.println("Configured SSID: NOT FOUND");
	}
}

// Non-blocking: starts an asynchronous scan and polls it on later calls.
void serviceWifiScan()
{
	if (s_wifiScanRunning) {
		const int16_t result = WiFi.scanComplete();
		if (result == WIFI_SCAN_RUNNING) {
			return;
		}

		s_wifiScanRunning = false;
		if (result < 0) {
			Serial.printf("Wi-Fi scan failed (code %d)\n", static_cast<int>(result));
		} else {
			printWifiScanResult(result);
		}
		WiFi.scanDelete();
		return;
	}

	if (!s_wifiScanRequested) {
		return;
	}

	if (WiFi.status() == WL_CONNECTED) {
		s_wifiScanRequested = false;
		return;
	}

	const uint32_t now = millis();
	if (s_wifiScanEverStarted && now - s_wifiScanStartedMs < WIFI_SCAN_MIN_INTERVAL_MS) {
		return;
	}

	s_wifiScanRequested = false;
	s_wifiScanEverStarted = true;
	s_wifiScanStartedMs = now;

	const int16_t result = WiFi.scanNetworks(true, true);
	if (result == WIFI_SCAN_RUNNING) {
		s_wifiScanRunning = true;
		Serial.println("Wi-Fi scan started (after NO_AP_FOUND/AUTH_EXPIRE/AUTH_FAIL)");
	} else {
		Serial.printf("Wi-Fi scan could not start (code %d)\n", static_cast<int>(result));
	}
}

} // namespace

const char *toString(PublishResult result)
{
	switch (result) {
		case PublishResult::Published:
			return "published";
		case PublishResult::WifiNotConnected:
			return "wifi_not_connected";
		case PublishResult::MqttNotConnected:
			return "mqtt_not_connected";
		case PublishResult::IncompleteReading:
			return "incomplete_reading";
		case PublishResult::NoTimestamp:
			return "no_timestamp";
		case PublishResult::PublishFailed:
			return "publish_failed";
	}

	return "unknown";
}

TelemetryPublisher::TelemetryPublisher(WiFiClient &wifiClient, const char *deviceId)
    : m_mqttClient(wifiClient), m_deviceId(deviceId),
      m_topic(String("v1/devices/") + deviceId + "/telemetry")
{
}

void TelemetryPublisher::begin()
{
	m_mqttClient.setServer(secrets::MQTT_HOST, secrets::MQTT_PORT);
	m_mqttClient.setBufferSize(MQTT_BUFFER_SIZE);

	if (!s_wifiEventHandlerRegistered) {
		WiFi.onEvent(onWifiEvent);
		s_wifiEventHandlerRegistered = true;
	}

	WiFi.mode(WIFI_STA);
	WiFi.begin(secrets::WIFI_SSID, secrets::WIFI_PASSWORD);
	m_lastWifiAttemptMs = millis();

	Serial.printf("MQTT topic: %s\n", m_topic.c_str());
}

void TelemetryPublisher::loop()
{
	logWifiEvents();
	serviceWifiScan();
	ensureWifi();

	if (WiFi.status() != WL_CONNECTED) {
		return;
	}

	ensureMqtt();

	if (m_mqttClient.connected()) {
		m_mqttClient.loop();
	}
}

bool TelemetryPublisher::isConnected()
{
	return WiFi.status() == WL_CONNECTED && m_mqttClient.connected();
}

int TelemetryPublisher::mqttState()
{
	return m_mqttClient.state();
}

PublishResult TelemetryPublisher::publish(const Reading &reading)
{
	if (!reading.batterySocOk || !reading.batteryPowerOk) {
		return PublishResult::IncompleteReading;
	}

	if (reading.measuredAt.isEmpty()) {
		return PublishResult::NoTimestamp;
	}

	if (WiFi.status() != WL_CONNECTED) {
		return PublishResult::WifiNotConnected;
	}

	if (!m_mqttClient.connected()) {
		return PublishResult::MqttNotConnected;
	}

	++m_sequence;

	StaticJsonDocument<JSON_DOCUMENT_CAPACITY> document;
	document["schema_version"] = 1;
	document["seq"] = m_sequence;
	document["measured_at"] = reading.measuredAt;

	// Optional fields are omitted, never zero-filled, when not read
	// successfully from a verified register in this cycle.
	JsonObject measurement = document.createNestedObject("measurement");
	if (reading.pvPowerOk) {
		measurement["pv_power_w"] = reading.pvPowerW;
	}
	if (reading.gridPowerOk) {
		measurement["grid_power_w"] = reading.gridPowerW;
	}
	measurement["battery_power_w"] = reading.batteryPowerW;
	if (reading.loadPowerOk) {
		measurement["load_power_w"] = reading.loadPowerW;
	}
	measurement["battery_soc_pct"] = reading.batterySocPct;
	if (reading.gridVoltageOk) {
		measurement["grid_voltage_v"] = reading.gridVoltageV;
	}
	if (reading.gridRelayOk) {
		measurement["grid_relay_on"] = reading.gridRelayOn;
	}

	char payload[PAYLOAD_BUFFER_SIZE];
	const size_t length = serializeJson(document, payload, sizeof(payload));

	if (length == 0 || length >= sizeof(payload) || document.overflowed()) {
		Serial.println("Telemetry payload serialization failed");
		return PublishResult::PublishFailed;
	}

	if (!m_mqttClient.publish(m_topic.c_str(), reinterpret_cast<const uint8_t *>(payload), length,
	                          false)) {
		return PublishResult::PublishFailed;
	}

	Serial.printf("Published seq=%lu: %s\n", static_cast<unsigned long>(m_sequence), payload);

	return PublishResult::Published;
}

void TelemetryPublisher::ensureWifi()
{
	if (WiFi.status() == WL_CONNECTED) {
		if (!m_wifiWasConnected) {
			m_wifiWasConnected = true;
			Serial.printf("Wi-Fi connected, status=%d, IP %s, RSSI %d dBm\n",
			              static_cast<int>(WiFi.status()), WiFi.localIP().toString().c_str(),
			              WiFi.RSSI());
		}
		return;
	}

	if (m_wifiWasConnected) {
		m_wifiWasConnected = false;
		Serial.printf("Wi-Fi connection lost, status=%d\n", static_cast<int>(WiFi.status()));
	}

	const uint32_t now = millis();

	if (now - m_lastWifiAttemptMs < config::WIFI_CONNECT_TIMEOUT_MS) {
		return;
	}

	m_lastWifiAttemptMs = now;
	Serial.printf("Wi-Fi not connected, retrying, status=%d\n", static_cast<int>(WiFi.status()));
	WiFi.disconnect();
	WiFi.begin(secrets::WIFI_SSID, secrets::WIFI_PASSWORD);
}

void TelemetryPublisher::ensureMqtt()
{
	if (m_mqttClient.connected()) {
		return;
	}

	const uint32_t now = millis();

	if (m_lastMqttAttemptMs != 0 && now - m_lastMqttAttemptMs < config::MQTT_RETRY_INTERVAL_MS) {
		return;
	}

	m_lastMqttAttemptMs = now;

	Serial.printf("Connecting to MQTT broker as \"%s\"\n", m_deviceId);

	const bool connected =
	    secrets::MQTT_USERNAME[0] != '\0'
	        ? m_mqttClient.connect(m_deviceId, secrets::MQTT_USERNAME, secrets::MQTT_PASSWORD)
	        : m_mqttClient.connect(m_deviceId);

	if (connected) {
		Serial.println("MQTT connected");
	} else {
		Serial.printf("MQTT connect failed, state=%d\n", m_mqttClient.state());
	}
}

} // namespace telemetry
