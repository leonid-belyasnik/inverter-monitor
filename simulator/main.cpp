/******************************************************************************
**
** File      main.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include <mosquitto.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include <domain/Measurement.h>

namespace {

std::string getEnvironment(const char *name, const std::string &defaultValue)
{
	const char *value = std::getenv(name);

	return value ? value : defaultValue;
}

int getEnvironmentInt(const char *name, const int defaultValue)
{
	const char *value = std::getenv(name);

	if (!value) {
		return defaultValue;
	}

	try {
		return std::stoi(value);
	} catch (...) {
		return defaultValue;
	}
}

std::string formatTimestamp(const std::chrono::system_clock::time_point time)
{
	const auto timeValue = std::chrono::system_clock::to_time_t(time);

	std::tm tm{};
	gmtime_r(&timeValue, &tm);

	const auto milliseconds =
	    std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;

	std::ostringstream stream;

	stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
	       << milliseconds.count() << 'Z';

	return stream.str();
}

std::string formatMeasurement(const domain::Measurement &measurement, const std::uint64_t sequence)
{
	std::ostringstream stream;

	stream << std::fixed << std::setprecision(2);

	stream << "{" << "\"schema_version\":1," << "\"seq\":" << sequence << ','
	       << "\"measured_at\":\"" << formatTimestamp(measurement.measuredAt) << "\","
	       << "\"measurement\":{" << "\"pv_power_w\":" << measurement.pvPower.value() << ','
	       << "\"grid_power_w\":" << measurement.gridPower.value() << ','
	       << "\"battery_power_w\":" << measurement.batteryPower << ','
	       << "\"load_power_w\":" << measurement.loadPower.value() << ','
	       << "\"battery_soc_pct\":" << measurement.batterySoc << "}" << "}";

	return stream.str();
}

domain::Measurement generateMeasurement(const std::uint64_t sequence,
                                        const std::chrono::system_clock::time_point now)
{
	const double phase = static_cast<double>(sequence) * 0.15;

	const double pvPower = 2200.0 + 500.0 * std::sin(phase);

	const double loadPower = 1500.0 + 300.0 * std::sin(phase * 0.7);

	const double gridPower = -200.0;

	// Positive = battery discharging.
	// Negative = battery charging.
	const double batteryPower = loadPower - pvPower - gridPower;

	const double batterySoc = 73.0 + 4.0 * std::sin(phase * 0.1);

	return domain::Measurement{.measuredAt = now,
	                           .pvPower = pvPower,
	                           .gridPower = gridPower,
	                           .batteryPower = batteryPower,
	                           .loadPower = loadPower,
	                           .batterySoc = batterySoc};
}

} // namespace

int main()
{
	const std::string mqttHost = getEnvironment("MQTT_HOST", "localhost");

	const int mqttPort = getEnvironmentInt("MQTT_PORT", 1883);

	const std::string deviceId = getEnvironment("DEVICE_ID", "simulator-001");

	const int intervalSeconds = getEnvironmentInt("PUBLISH_INTERVAL_SEC", 5);

	const std::string topic = "v1/devices/" + deviceId + "/telemetry";

	mosquitto_lib_init();

	mosquitto *client = mosquitto_new(deviceId.c_str(), true, nullptr);

	if (!client) {
		std::cerr << "Failed to create MQTT client\n";
		mosquitto_lib_cleanup();
		return 1;
	}

	std::cout << "Connecting to MQTT broker " << mqttHost << ':' << mqttPort << '\n';

	int result = mosquitto_connect(client, mqttHost.c_str(), mqttPort, 60);

	if (result != MOSQ_ERR_SUCCESS) {
		std::cerr << "MQTT connection failed: " << mosquitto_strerror(result) << '\n';

		mosquitto_destroy(client);
		mosquitto_lib_cleanup();
		return 1;
	}

	result = mosquitto_loop_start(client);

	if (result != MOSQ_ERR_SUCCESS) {
		std::cerr << "Failed to start MQTT network loop: " << mosquitto_strerror(result) << '\n';

		mosquitto_disconnect(client);
		mosquitto_destroy(client);
		mosquitto_lib_cleanup();
		return 1;
	}

	std::cout << "Connected. Publishing telemetry to " << topic << '\n';

	std::uint64_t sequence = 1;

	while (true) {
		const auto now = std::chrono::system_clock::now();

		const domain::Measurement measurement = generateMeasurement(sequence, now);

		const std::string payload = formatMeasurement(measurement, sequence);

		result = mosquitto_publish(client, nullptr, topic.c_str(), static_cast<int>(payload.size()),
		                           payload.data(), 1, false);

		if (result != MOSQ_ERR_SUCCESS) {
			std::cerr << "MQTT publish failed: " << mosquitto_strerror(result) << '\n';
		} else {
			std::cout << "Published seq=" << sequence << ": " << payload << '\n';
		}

		++sequence;

		std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
	}

	mosquitto_loop_stop(client, true);
	mosquitto_disconnect(client);
	mosquitto_destroy(client);
	mosquitto_lib_cleanup();

	return 0;
}
