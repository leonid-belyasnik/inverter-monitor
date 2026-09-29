#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "mqtt/MqttClient.h"

namespace
{

std::string getEnvironment(
const char* name,
const std::string& defaultValue)
{
const char* value = std::getenv(name);

return value ? value : defaultValue;

}

int getEnvironmentInt(
const char* name,
const int defaultValue)
{
const char* value = std::getenv(name);

if (!value) {
    return defaultValue;
}

try {
    return std::stoi(value);
} catch (...) {
    return defaultValue;
}

}

}

int main()
{
const std::string mqttHost =
getEnvironment("MQTT_HOST", "localhost");

const int mqttPort =
    getEnvironmentInt("MQTT_PORT", 1883);

std::cout
    << "Starting inverter-monitor\n"
    << "MQTT broker: "
    << mqttHost << ':' << mqttPort
    << '\n';

mqtt::MqttClient mqtt(
    "inverter-monitor-backend",
    mqttHost,
    mqttPort,
    [](const std::string_view topic,
       const std::string_view payload)
    {
        std::cout
            << "MQTT message received\n"
            << "  topic:   " << topic << '\n'
            << "  payload: " << payload << '\n';
    });

if (!mqtt.connect()) {
    std::cerr
        << "MQTT connection failed: "
        << mqtt.errorMessage()
        << '\n';

    return 1;
}

if (!mqtt.subscribe("v1/devices/+/telemetry")) {
    std::cerr
        << "MQTT subscribe failed: "
        << mqtt.errorMessage()
        << '\n';

    return 1;
}

if (!mqtt.startLoop()) {
    std::cerr
        << "MQTT loop start failed: "
        << mqtt.errorMessage()
        << '\n';

    return 1;
}

std::cout
    << "MQTT client is running\n"
    << "Waiting for telemetry...\n";

while (true) {
    std::this_thread::sleep_for(
        std::chrono::seconds(1));
}

return 0;

}