#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <exception>

#include <api/ApiRoutes.h>
#include <api/HttpRouter.h>
#include <api/HttpServer.h>
#include <api/WebSocketServer.h>
#include <application/MeasurementQueryService.h>
#include <application/TelemetryIngestionService.h>
#include <database/DatabaseSchema.h>
#include <database/MeasurementRepository.h>
#include <database/PostgresConnection.h>
#include <mqtt/MqttClient.h>
#include <mqtt/TelemetryAdapter.h>
#include <protocol/TelemetryParser.h>

namespace {

volatile std::sig_atomic_t stopRequested = 0;

void handleSignal(int)
{
	stopRequested = 1;
}

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

enum class LogLevel { Info, Warning, Error };

std::mutex logMutex;

std::string utcTimestamp()
{
	const auto now = std::chrono::system_clock::now();
	const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	const auto milliseconds =
	    std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds).count();
	const std::time_t time = std::chrono::system_clock::to_time_t(seconds);

	std::tm tm{};
	gmtime_r(&time, &tm);

	std::ostringstream stream;
	stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
	       << milliseconds << 'Z';

	return stream.str();
}

// libpq messages may span several lines; each log entry stays on one line.
std::string singleLine(const std::string &text)
{
	std::string result;
	bool pendingSpace = false;

	for (const char character : text) {
		if (std::isspace(static_cast<unsigned char>(character))) {
			pendingSpace = !result.empty();
			continue;
		}

		if (pendingSpace) {
			result += ' ';
			pendingSpace = false;
		}

		result += character;
	}

	return result;
}

void logMessage(const LogLevel level, const std::string &message)
{
	const char *levelName = level == LogLevel::Info      ? "INFO"
	                        : level == LogLevel::Warning ? "WARN"
	                                                     : "ERROR";
	std::ostream &stream = level == LogLevel::Info ? std::cout : std::cerr;

	std::lock_guard lock(logMutex);
	stream << utcTimestamp() << ' ' << levelName << ' ' << message << std::endl;
}

std::string describeIngestionError(const application::IngestionError &error)
{
	std::string description = singleLine(error.message);

	if (description.empty()) {
		description = "unknown error";
	}

	if (!error.sqlState.empty()) {
		description += " (SQLSTATE " + error.sqlState + ')';
	}

	return description;
}

} // namespace

int main()
{
	const std::string mqttHost = getEnvironment("MQTT_HOST", "localhost");

	const int mqttPort = getEnvironmentInt("MQTT_PORT", 1883);

	database::PostgresConnection databaseConnection("");

	if (!databaseConnection.isOk()) {
		logMessage(LogLevel::Error,
		           "Database connection failed: " + singleLine(databaseConnection.errorMessage()));
		return 1;
	}

	database::DatabaseSchema schema(databaseConnection);

	if (!schema.initialize()) {
		logMessage(LogLevel::Error,
		           "Database schema initialization failed: " + singleLine(schema.errorMessage()));
		return 1;
	}

	database::MeasurementRepository repository(databaseConnection);
	application::MeasurementQueryService queryService(repository);
	api::HttpRouter httpRouter;
	api::ApiRoutes::registerRoutes(httpRouter, queryService);

	std::unique_ptr<api::HttpServer> httpServer;
	try {
		httpServer = std::make_unique<api::HttpServer>("0.0.0.0", 8080, httpRouter);
		httpServer->listen();
	} catch (const std::exception& error) {
		logMessage(LogLevel::Error,
		           std::string("HTTP server startup failed on port 8080: ") + error.what());
		return 1;
	}

	std::unique_ptr<api::WebSocketServer> webSocketServer;
	try {
		webSocketServer = std::make_unique<api::WebSocketServer>("0.0.0.0", 8081);
	} catch (const std::exception& error) {
		logMessage(LogLevel::Error,
		           std::string("WebSocket server startup failed on port 8081: ") + error.what());
		return 1;
	}

	application::TelemetryIngestionService ingestion(
	    repository, [&webSocketServer](const application::IngestedMeasurement &measurement) {
		    webSocketServer->publish(measurement);
	    });

	protocol::TelemetryParser parser;
	mqtt::TelemetryAdapter telemetryAdapter(parser);

	std::atomic_bool httpThreadFailed{false};
	std::atomic_bool webSocketThreadFailed{false};
	std::thread httpThread([&httpServer, &httpThreadFailed] {
		try {
			httpServer->run();
		} catch (const std::exception& error) {
			logMessage(LogLevel::Error, std::string("HTTP server failed: ") + error.what());
			httpThreadFailed = true;
		} catch (...) {
			logMessage(LogLevel::Error, "HTTP server failed with an unknown error");
			httpThreadFailed = true;
		}
	});
	std::thread webSocketThread;
	try {
		webSocketThread = std::thread([&webSocketServer, &webSocketThreadFailed] {
			try {
				webSocketServer->run();
			} catch (const std::exception& error) {
				logMessage(LogLevel::Error, std::string("WebSocket server failed: ") + error.what());
				webSocketThreadFailed = true;
			} catch (...) {
				logMessage(LogLevel::Error, "WebSocket server failed with an unknown error");
				webSocketThreadFailed = true;
			}
		});
	} catch (const std::exception& error) {
		logMessage(LogLevel::Error,
		           std::string("WebSocket server thread startup failed: ") + error.what());
		httpServer->stop();
		httpThread.join();
		return 1;
	}
	auto stopServers = [&] {
		httpServer->stop();
		webSocketServer->stop();
		if (httpThread.joinable()) httpThread.join();
		if (webSocketThread.joinable()) webSocketThread.join();
	};

	std::signal(SIGINT, handleSignal);
	std::signal(SIGTERM, handleSignal);

	logMessage(LogLevel::Info, "Starting inverter-monitor");
	logMessage(LogLevel::Info, "MQTT broker: " + mqttHost + ':' + std::to_string(mqttPort));

	mqtt::MqttClient mqtt("inverter-monitor-backend", mqttHost, mqttPort,
	                      [&telemetryAdapter, &ingestion](const std::string_view topic,
	                                                      const std::string_view payload) {
		                      const auto measurement = telemetryAdapter.adapt(topic, payload);

		                      if (!measurement) {
			                      logMessage(LogLevel::Warning, "Invalid telemetry topic or payload");
			                      return;
		                      }

		                      application::IngestionError error;
		                      const auto measurementId = ingestion.ingest(*measurement, &error);
		                      const std::string &deviceId = measurement->deviceId.value;

		                      if (!measurementId) {
			                      logMessage(LogLevel::Error,
			                                 "Failed to persist telemetry for device " + deviceId +
			                                     ": " + describeIngestionError(error));
			                      return;
		                      }

		                      logMessage(LogLevel::Info, "Persisted telemetry for device " + deviceId +
		                                                     " as measurement " +
		                                                     std::to_string(*measurementId));

		                      if (!error.message.empty()) {
			                      logMessage(LogLevel::Warning,
			                                 "Measurement " + std::to_string(*measurementId) +
			                                     " for device " + deviceId +
			                                     " was persisted, but delivery to WebSocket clients failed: " +
			                                     describeIngestionError(error));
		                      }
	                      });

	if (!mqtt.connect()) {
		logMessage(LogLevel::Error, "MQTT connection failed: " + mqtt.errorMessage());
		stopServers();

		return 1;
	}

	if (!mqtt.subscribe("v1/devices/+/telemetry")) {
		logMessage(LogLevel::Error, "MQTT subscribe failed: " + mqtt.errorMessage());
		stopServers();

		return 1;
	}

	if (!mqtt.startLoop()) {
		logMessage(LogLevel::Error, "MQTT loop start failed: " + mqtt.errorMessage());
		stopServers();

		return 1;
	}

	logMessage(LogLevel::Info, "MQTT client is running");
	logMessage(LogLevel::Info, "Waiting for telemetry...");

	while (!stopRequested && !httpThreadFailed && !webSocketThreadFailed) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	mqtt.stopLoop();
	stopServers();

	return (httpThreadFailed || webSocketThreadFailed) ? 1 : 0;
}
