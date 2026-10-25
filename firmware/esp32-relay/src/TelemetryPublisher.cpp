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
#include "ConsoleLog.h"

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
		consolelog::printf("Wi-Fi events dropped: %lu\n", static_cast<unsigned long>(dropped));
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
				consolelog::printf("Wi-Fi event: STA_CONNECTED, status=%d\n", status);
				break;
			case ARDUINO_EVENT_WIFI_STA_GOT_IP:
				if (status == WL_CONNECTED) {
					consolelog::printf("Wi-Fi event: STA_GOT_IP, status=%d, IP %s, RSSI %d dBm\n",
					                   status, WiFi.localIP().toString().c_str(), WiFi.RSSI());
				} else {
					consolelog::printf("Wi-Fi event: STA_GOT_IP, status=%d\n", status);
				}
				break;
			case ARDUINO_EVENT_WIFI_STA_LOST_IP:
				consolelog::printf("Wi-Fi event: STA_LOST_IP, status=%d\n", status);
				break;
			case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
				consolelog::printf(
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

void printWifiScanResult(int16_t count, const String &configuredSsid)
{
	const int shown = count < WIFI_SCAN_MAX_PRINTED ? count : WIFI_SCAN_MAX_PRINTED;
	int configuredMatches = 0;

	consolelog::printf("--- Wi-Fi scan: %d network(s) found ---\n", count);

	for (int i = 0; i < count; ++i) {
		const String ssid = WiFi.SSID(i);
		const bool isConfigured = ssid == configuredSsid;
		if (isConfigured) {
			++configuredMatches;
		}

		if (i < shown) {
			consolelog::printf("  [%d] SSID \"%s\" ch=%d rssi=%d dBm auth=%s%s\n", i,
			                   ssid.length() != 0 ? ssid.c_str() : "<hidden>",
			                   static_cast<int>(WiFi.channel(i)), static_cast<int>(WiFi.RSSI(i)),
			                   authModeName(WiFi.encryptionType(i)),
			                   isConfigured ? "  <-- configured SSID" : "");
		}
	}

	if (count > shown) {
		consolelog::printf("  ... %d more not listed\n", count - shown);
	}

	if (configuredMatches > 0) {
		consolelog::printf("Configured SSID: FOUND (%d AP(s))\n", configuredMatches);
	} else {
		consolelog::println("Configured SSID: NOT FOUND");
	}
}

// Non-blocking: starts an asynchronous scan and polls it on later calls.
void serviceWifiScan(const String &configuredSsid)
{
	if (s_wifiScanRunning) {
		const int16_t result = WiFi.scanComplete();
		if (result == WIFI_SCAN_RUNNING) {
			return;
		}

		s_wifiScanRunning = false;
		if (result < 0) {
			consolelog::printf("Wi-Fi scan failed (code %d)\n", static_cast<int>(result));
		} else {
			printWifiScanResult(result, configuredSsid);
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
		consolelog::println("Wi-Fi scan started (after NO_AP_FOUND/AUTH_EXPIRE/AUTH_FAIL)");
	} else {
		consolelog::printf("Wi-Fi scan could not start (code %d)\n", static_cast<int>(result));
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

TelemetryPublisher::TelemetryPublisher(WiFiClient &wifiClient) : m_mqttClient(wifiClient) {}

void TelemetryPublisher::begin(const deviceconfig::WifiSettings &wifi,
                               const deviceconfig::MqttSettings &mqtt, const String &deviceId)
{
	m_wifiSettings = wifi;
	m_mqttSettings = mqtt;
	m_deviceId = deviceId;
	m_topic = String("v1/devices/") + m_deviceId + "/telemetry";
	m_mqttClient.setServer(m_mqttSettings.host.c_str(), m_mqttSettings.port);
	m_mqttClient.setBufferSize(MQTT_BUFFER_SIZE);

	if (!s_wifiEventHandlerRegistered) {
		WiFi.onEvent(onWifiEvent);
		s_wifiEventHandlerRegistered = true;
	}

	WiFi.mode(WIFI_STA);
	WiFi.begin(m_wifiSettings.ssid.c_str(), m_wifiSettings.password.c_str());
	m_lastWifiAttemptMs = millis();

	consolelog::printf("MQTT topic: %s\n", m_topic.c_str());
}

void TelemetryPublisher::applyWifiSettings(const deviceconfig::WifiSettings &wifi)
{
	if (m_mqttClient.connected()) {
		consolelog::println("Wi-Fi settings changed, disconnecting from MQTT broker");
		m_mqttClient.disconnect();
	}

	m_wifiSettings = wifi;
	consolelog::printf("Wi-Fi settings changed, connecting to SSID \"%s\"\n",
	                   m_wifiSettings.ssid.c_str());
	WiFi.disconnect();
	WiFi.begin(m_wifiSettings.ssid.c_str(), m_wifiSettings.password.c_str());
	m_lastWifiAttemptMs = millis();
	m_lastMqttAttemptMs = 0;
}

void TelemetryPublisher::applyMqttSettings(const deviceconfig::MqttSettings &mqtt,
                                           const String &deviceId)
{
	if (m_mqttClient.connected()) {
		consolelog::println("MQTT settings changed, disconnecting from current broker");
		m_mqttClient.disconnect();
	}

	m_mqttSettings = mqtt;
	m_deviceId = deviceId;
	m_topic = String("v1/devices/") + m_deviceId + "/telemetry";
	m_mqttClient.setServer(m_mqttSettings.host.c_str(), m_mqttSettings.port);
	m_lastMqttAttemptMs = 0;

	consolelog::printf("MQTT topic: %s\n", m_topic.c_str());
}

const String &TelemetryPublisher::topic() const
{
	return m_topic;
}

void TelemetryPublisher::loop()
{
	logWifiEvents();
	serviceWifiScan(m_wifiSettings.ssid);
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

PublishResult TelemetryPublisher::publish(const Reading &reading, bool logPublished)
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
		consolelog::println("Telemetry payload serialization failed");
		return PublishResult::PublishFailed;
	}

	if (!m_mqttClient.publish(m_topic.c_str(), reinterpret_cast<const uint8_t *>(payload), length,
	                          false)) {
		return PublishResult::PublishFailed;
	}

	if (logPublished) {
		consolelog::printf("Published seq=%lu: %s\n", static_cast<unsigned long>(m_sequence),
		                   payload);
	}

	return PublishResult::Published;
}

void TelemetryPublisher::ensureWifi()
{
	if (WiFi.status() == WL_CONNECTED) {
		if (!m_wifiWasConnected) {
			m_wifiWasConnected = true;
			consolelog::printf("Wi-Fi connected, status=%d, IP %s, RSSI %d dBm\n",
			                   static_cast<int>(WiFi.status()),
			                   WiFi.localIP().toString().c_str(), WiFi.RSSI());
		}
		return;
	}

	if (m_wifiWasConnected) {
		m_wifiWasConnected = false;
		consolelog::printf("Wi-Fi connection lost, status=%d\n", static_cast<int>(WiFi.status()));
	}

	const uint32_t now = millis();

	if (now - m_lastWifiAttemptMs < config::WIFI_CONNECT_TIMEOUT_MS) {
		return;
	}

	m_lastWifiAttemptMs = now;
	consolelog::printf("Wi-Fi not connected, retrying, status=%d\n",
	                   static_cast<int>(WiFi.status()));
	WiFi.disconnect();
	WiFi.begin(m_wifiSettings.ssid.c_str(), m_wifiSettings.password.c_str());
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

	consolelog::printf("Connecting to MQTT broker %s:%u as \"%s\" (%s)\n",
	                   m_mqttSettings.host.c_str(), m_mqttSettings.port, m_deviceId.c_str(),
	                   m_mqttSettings.username.isEmpty() ? "anonymous" : "with credentials");

	const bool connected =
	    !m_mqttSettings.username.isEmpty()
	        ? m_mqttClient.connect(m_deviceId.c_str(), m_mqttSettings.username.c_str(),
	                               m_mqttSettings.password.c_str())
	        : m_mqttClient.connect(m_deviceId.c_str());

	if (connected) {
		consolelog::println("MQTT connected");
	} else {
		consolelog::printf("MQTT connect failed, state=%d\n", m_mqttClient.state());
	}
}

} // namespace telemetry
