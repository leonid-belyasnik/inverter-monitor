/******************************************************************************
**
** File      TelemetryPublisher.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include "DeviceConfig.h"

namespace telemetry {

// One telemetry snapshot. Each "...Ok" flag is true only when the value was
// read successfully in the current poll cycle; values are never carried over
// from earlier cycles.
struct Reading
{
	bool batterySocOk = false;
	double batterySocPct = 0.0;

	bool batteryPowerOk = false;
	double batteryPowerW = 0.0;

	bool pvPowerOk = false;
	double pvPowerW = 0.0;

	bool gridPowerOk = false;
	double gridPowerW = 0.0;

	bool loadPowerOk = false;
	double loadPowerW = 0.0;

	bool gridVoltageOk = false;
	double gridVoltageV = 0.0;

	bool gridRelayOk = false;
	bool gridRelayOn = false;

	// Empty unless the clock is NTP-synced (see timesync::nowIso8601Utc()).
	String measuredAt;
};

enum class PublishResult
{
	Published,
	WifiNotConnected,
	MqttNotConnected,
	IncompleteReading, // battery SOC or battery power not read this cycle
	NoTimestamp,
	PublishFailed,
};

const char *toString(PublishResult result);

// Publishes telemetry to v1/devices/<device_id>/telemetry using the same
// JSON schema as simulator/main.cpp (schema_version 1). battery_soc_pct and
// battery_power_w are required; pv_power_w, grid_power_w, load_power_w,
// grid_voltage_v, and grid_relay_on are omitted when unavailable. Manages
// Wi-Fi and MQTT reconnection with bounded retry intervals; never blocks for
// longer than one connect attempt.
class TelemetryPublisher
{
  public:
	explicit TelemetryPublisher(WiFiClient &wifiClient);

	// Configures the MQTT client and starts the Wi-Fi connection.
	void begin(const deviceconfig::WifiSettings &wifi, const deviceconfig::MqttSettings &mqtt,
	           const String &deviceId);

	// Closes an open MQTT session and reconnects Wi-Fi with new settings.
	void applyWifiSettings(const deviceconfig::WifiSettings &wifi);

	// Replaces the broker settings and device id (MQTT client id and
	// topic). An open MQTT session is closed so the next loop() connects
	// with the new settings.
	void applyMqttSettings(const deviceconfig::MqttSettings &mqtt, const String &deviceId);

	const String &topic() const;

	// Services Wi-Fi/MQTT reconnection and the MQTT keep-alive. Call from
	// every loop() iteration.
	void loop();

	bool isConnected();

	// PubSubClient state code (0 = connected, negative = disconnected/error).
	int mqttState();

	// `logPublished` prints the published payload to Serial; it does not
	// change what is published.
	PublishResult publish(const Reading &reading, bool logPublished = false);

  private:
	void ensureWifi();
	void ensureMqtt();

	PubSubClient m_mqttClient;
	deviceconfig::WifiSettings m_wifiSettings;
	// PubSubClient keeps a pointer to the host string, so it lives here.
	deviceconfig::MqttSettings m_mqttSettings;
	String m_deviceId;
	String m_topic;

	uint32_t m_sequence = 0;
	uint32_t m_lastWifiAttemptMs = 0;
	uint32_t m_lastMqttAttemptMs = 0;
	bool m_wifiWasConnected = false;
};

} // namespace telemetry
