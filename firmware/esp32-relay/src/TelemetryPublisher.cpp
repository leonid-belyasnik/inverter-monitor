#include "TelemetryPublisher.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "Config.h"
#include "secrets.h"

namespace telemetry {

namespace {

constexpr uint16_t MQTT_BUFFER_SIZE = 512;
constexpr size_t JSON_DOCUMENT_CAPACITY = 384;
constexpr size_t PAYLOAD_BUFFER_SIZE = 384;

} // namespace

const char* toString(PublishResult result)
{
    switch (result) {
        case PublishResult::Published:
            return "published";
        case PublishResult::NotConnected:
            return "not_connected";
        case PublishResult::IncompleteReading:
            return "incomplete_reading";
        case PublishResult::NoTimestamp:
            return "no_timestamp";
        case PublishResult::PublishFailed:
            return "publish_failed";
    }

    return "unknown";
}

TelemetryPublisher::TelemetryPublisher(WiFiClient& wifiClient, const char* deviceId)
    : m_mqttClient(wifiClient)
    , m_deviceId(deviceId)
    , m_topic(String("v1/devices/") + deviceId + "/telemetry")
{
}

void TelemetryPublisher::begin()
{
    m_mqttClient.setServer(secrets::MQTT_HOST, secrets::MQTT_PORT);
    m_mqttClient.setBufferSize(MQTT_BUFFER_SIZE);

    WiFi.mode(WIFI_STA);
    WiFi.begin(secrets::WIFI_SSID, secrets::WIFI_PASSWORD);
    m_lastWifiAttemptMs = millis();

    Serial.printf("MQTT topic: %s\n", m_topic.c_str());
}

void TelemetryPublisher::loop()
{
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

PublishResult TelemetryPublisher::publish(const Reading& reading)
{
    if (!reading.batterySocOk || !reading.batteryPowerOk) {
        return PublishResult::IncompleteReading;
    }

    if (reading.measuredAt.isEmpty()) {
        return PublishResult::NoTimestamp;
    }

    if (!isConnected()) {
        return PublishResult::NotConnected;
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

    char payload[PAYLOAD_BUFFER_SIZE];
    const size_t length = serializeJson(document, payload, sizeof(payload));

    if (length == 0 || length >= sizeof(payload) || document.overflowed()) {
        Serial.println("Telemetry payload serialization failed");
        return PublishResult::PublishFailed;
    }

    if (!m_mqttClient.publish(m_topic.c_str(),
                              reinterpret_cast<const uint8_t*>(payload),
                              length,
                              false)) {
        return PublishResult::PublishFailed;
    }

    Serial.printf("Published seq=%lu: %s\n",
                  static_cast<unsigned long>(m_sequence), payload);

    return PublishResult::Published;
}

void TelemetryPublisher::ensureWifi()
{
    if (WiFi.status() == WL_CONNECTED) {
        if (!m_wifiWasConnected) {
            m_wifiWasConnected = true;
            Serial.printf("Wi-Fi connected, IP %s\n",
                          WiFi.localIP().toString().c_str());
        }
        return;
    }

    if (m_wifiWasConnected) {
        m_wifiWasConnected = false;
        Serial.println("Wi-Fi connection lost");
    }

    const uint32_t now = millis();

    if (now - m_lastWifiAttemptMs < config::WIFI_CONNECT_TIMEOUT_MS) {
        return;
    }

    m_lastWifiAttemptMs = now;
    Serial.println("Wi-Fi not connected, retrying");
    WiFi.disconnect();
    WiFi.begin(secrets::WIFI_SSID, secrets::WIFI_PASSWORD);
}

void TelemetryPublisher::ensureMqtt()
{
    if (m_mqttClient.connected()) {
        return;
    }

    const uint32_t now = millis();

    if (m_lastMqttAttemptMs != 0 &&
        now - m_lastMqttAttemptMs < config::MQTT_RETRY_INTERVAL_MS) {
        return;
    }

    m_lastMqttAttemptMs = now;

    Serial.printf("Connecting to MQTT broker %s:%u as \"%s\"\n",
                  secrets::MQTT_HOST, secrets::MQTT_PORT, m_deviceId);

    const bool connected = secrets::MQTT_USERNAME[0] != '\0'
        ? m_mqttClient.connect(m_deviceId, secrets::MQTT_USERNAME, secrets::MQTT_PASSWORD)
        : m_mqttClient.connect(m_deviceId);

    if (connected) {
        Serial.println("MQTT connected");
    } else {
        Serial.printf("MQTT connect failed, state=%d\n", m_mqttClient.state());
    }
}

} // namespace telemetry
