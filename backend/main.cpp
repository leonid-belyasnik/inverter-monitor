#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <application/TelemetryIngestionService.h>
#include <database/DatabaseSchema.h>
#include <database/MeasurementRepository.h>
#include <database/PostgresConnection.h>
#include <mqtt/MqttClient.h>
#include <mqtt/TelemetryAdapter.h>
#include <protocol/TelemetryParser.h>

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

database::PostgresConnection databaseConnection("");

if (!databaseConnection.isOk()) {
    std::cerr
        << "Database connection failed: "
        << databaseConnection.errorMessage()
        << '\n';
    return 1;
}

database::DatabaseSchema schema(databaseConnection);

if (!schema.initialize()) {
    std::cerr << "Database schema initialization failed\n";
    return 1;
}

database::MeasurementRepository repository(databaseConnection);
application::TelemetryIngestionService ingestion(repository);
protocol::TelemetryParser parser;
mqtt::TelemetryAdapter telemetryAdapter(parser);

std::cout
    << "Starting inverter-monitor\n"
    << "MQTT broker: "
    << mqttHost << ':' << mqttPort
    << '\n';

mqtt::MqttClient mqtt(
    "inverter-monitor-backend",
    mqttHost,
    mqttPort,
    [&telemetryAdapter, &ingestion](const std::string_view topic,
                                    const std::string_view payload)
    {
        const auto measurement = telemetryAdapter.adapt(topic, payload);

        if (!measurement) {
            std::cerr << "Invalid telemetry topic or payload\n";
            return;
        }

        const auto measurementId = ingestion.ingest(*measurement);

        if (!measurementId) {
            std::cerr
                << "Failed to persist telemetry for device "
                << measurement->deviceId.value
                << '\n';
            return;
        }

        std::cout
            << "Persisted telemetry for device "
            << measurement->deviceId.value
            << " as measurement " << *measurementId
            << '\n';
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