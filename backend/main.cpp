#include <chrono>
#include <csignal>
#include <cstdlib>
#include <atomic>
#include <iostream>
#include <memory>
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

} // namespace

int main()
{
	const std::string mqttHost = getEnvironment("MQTT_HOST", "localhost");

	const int mqttPort = getEnvironmentInt("MQTT_PORT", 1883);

	database::PostgresConnection databaseConnection("");

	if (!databaseConnection.isOk()) {
		std::cerr << "Database connection failed: " << databaseConnection.errorMessage() << '\n';
		return 1;
	}

	database::DatabaseSchema schema(databaseConnection);

	if (!schema.initialize()) {
		std::cerr << "Database schema initialization failed: " << schema.errorMessage() << '\n';
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
		std::cerr << "HTTP server startup failed on port 8080: " << error.what() << '\n';
		return 1;
	}

	std::unique_ptr<api::WebSocketServer> webSocketServer;
	try {
		webSocketServer = std::make_unique<api::WebSocketServer>("0.0.0.0", 8081);
	} catch (const std::exception& error) {
		std::cerr << "WebSocket server startup failed on port 8081: " << error.what() << '\n';
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
			std::cerr << "HTTP server failed: " << error.what() << '\n';
			httpThreadFailed = true;
		} catch (...) {
			std::cerr << "HTTP server failed with an unknown error\n";
			httpThreadFailed = true;
		}
	});
	std::thread webSocketThread;
	try {
		webSocketThread = std::thread([&webSocketServer, &webSocketThreadFailed] {
			try {
				webSocketServer->run();
			} catch (const std::exception& error) {
				std::cerr << "WebSocket server failed: " << error.what() << '\n';
				webSocketThreadFailed = true;
			} catch (...) {
				std::cerr << "WebSocket server failed with an unknown error\n";
				webSocketThreadFailed = true;
			}
		});
	} catch (const std::exception& error) {
		std::cerr << "WebSocket server thread startup failed: " << error.what() << '\n';
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

	std::cout << "Starting inverter-monitor\n"
	          << "MQTT broker: " << mqttHost << ':' << mqttPort << '\n';

	mqtt::MqttClient mqtt("inverter-monitor-backend", mqttHost, mqttPort,
	                      [&telemetryAdapter, &ingestion](const std::string_view topic,
	                                                      const std::string_view payload) {
		                      const auto measurement = telemetryAdapter.adapt(topic, payload);

		                      if (!measurement) {
			                      std::cerr << "Invalid telemetry topic or payload\n";
			                      return;
		                      }

		                      const auto measurementId = ingestion.ingest(*measurement);

		                      if (!measurementId) {
			                      std::cerr << "Failed to persist telemetry for device "
			                                << measurement->deviceId.value << '\n';
			                      return;
		                      }

		                      std::cout << "Persisted telemetry for device "
		                                << measurement->deviceId.value << " as measurement "
		                                << *measurementId << '\n';
	                      });

	if (!mqtt.connect()) {
		std::cerr << "MQTT connection failed: " << mqtt.errorMessage() << '\n';
		stopServers();

		return 1;
	}

	if (!mqtt.subscribe("v1/devices/+/telemetry")) {
		std::cerr << "MQTT subscribe failed: " << mqtt.errorMessage() << '\n';
		stopServers();

		return 1;
	}

	if (!mqtt.startLoop()) {
		std::cerr << "MQTT loop start failed: " << mqtt.errorMessage() << '\n';
		stopServers();

		return 1;
	}

	std::cout << "MQTT client is running\n"
	          << "Waiting for telemetry...\n";

	while (!stopRequested && !httpThreadFailed && !webSocketThreadFailed) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	mqtt.stopLoop();
	stopServers();

	return (httpThreadFailed || webSocketThreadFailed) ? 1 : 0;
}
