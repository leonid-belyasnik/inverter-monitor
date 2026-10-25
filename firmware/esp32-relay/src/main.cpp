/******************************************************************************
**
** File      main.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "Config.h"
#include "ConsoleLog.h"
#include "DeviceConfig.h"
#include "ModbusClient.h"
#include "RegisterMap.h"
#include "SerialConsole.h"
#include "TelemetryPublisher.h"
#include "TimeSync.h"

namespace {

modbus::ModbusClient modbusClient(Serial1, config::MODBUS_SLAVE_ID, config::RS485_DE_PIN);

WiFiClient wifiClient;
telemetry::TelemetryPublisher publisher(wifiClient);
console::SerialConsole serialConsole(publisher, modbusClient);

uint32_t lastTelemetryMs = 0;
uint32_t lastDiagnosticMs = 0;
uint32_t lastSkipLogMs = 0;
bool firstCycle = true;
bool timeSyncReported = false;

// Serial telemetry output state of the current cycle, and the error state
// that is reported on changes only while that output is off.
bool cycleVerbose = false;
unsigned cycleFailures = 0;
char firstFailure[64] = "";
bool modbusFaulted = false;
int lastLoggedResult = -1;

// Waits like delay(), but keeps echoing typed characters. Commands are not
// executed here, so the Modbus configuration cannot change between requests.
void pauseWithConsole(uint32_t ms)
{
	const uint32_t start = millis();

	while (millis() - start < ms) {
		serialConsole.service();
		delay(1);
	}
}

// Reads one register and pauses for the inter-frame gap. Failures are logged
// and reported through the result status; no previous value is reused. With
// the Serial telemetry output off, only the first failure of a cycle is kept
// for the summary line.
modbus::ReadResult readRegister(uint16_t address, const char *label)
{
	const modbus::ReadResult result = modbusClient.readHoldingRegister(address);
	pauseWithConsole(config::MODBUS_INTER_FRAME_DELAY_MS);

	if (result.status == modbus::ReadStatus::Ok) {
		return result;
	}

	char text[64];
	if (result.status == modbus::ReadStatus::ExceptionResponse) {
		snprintf(text, sizeof(text), "%s (reg %u): modbus exception 0x%02X", label, address,
		         result.exceptionCode);
	} else {
		snprintf(text, sizeof(text), "%s (reg %u): %s", label, address,
		         modbus::toString(result.status));
	}

	if (cycleVerbose) {
		consolelog::println(text);
	} else if (cycleFailures == 0) {
		snprintf(firstFailure, sizeof(firstFailure), "%s", text);
	}
	++cycleFailures;

	return result;
}

bool readSignedPower(uint16_t address, const char *label, double &watts)
{
	const modbus::ReadResult result = readRegister(address, label);

	if (result.status != modbus::ReadStatus::Ok) {
		return false;
	}

	watts = static_cast<int16_t>(result.value);
	return true;
}

telemetry::Reading pollRegisters()
{
	telemetry::Reading reading;
	reading.measuredAt = timesync::nowIso8601Utc();

	cycleVerbose = serialConsole.telemetryOutputEnabled();
	cycleFailures = 0;

	if (cycleVerbose) {
		consolelog::println("--- Deye telemetry ---");
	}

	const auto voltage = readRegister(registers::GRID_VOLTAGE, "Grid voltage");
	if (voltage.status == modbus::ReadStatus::Ok) {
		reading.gridVoltageOk = true;
		reading.gridVoltageV = voltage.value / 10.0;
		if (cycleVerbose) {
			consolelog::printf("Grid voltage: %.1f V\n", reading.gridVoltageV);
		}
	}

	const auto relay = readRegister(registers::GRID_RELAY_STATUS, "Grid relay");
	if (relay.status == modbus::ReadStatus::Ok) {
		reading.gridRelayOk = true;
		reading.gridRelayOn = (relay.value & 1) != 0;
		if (cycleVerbose) {
			consolelog::printf("Grid relay: %s\n", reading.gridRelayOn ? "ON" : "OFF");
		}
	}

	const auto soc = readRegister(registers::BATTERY_SOC, "Battery SOC");
	if (soc.status == modbus::ReadStatus::Ok) {
		reading.batterySocOk = true;
		reading.batterySocPct = soc.value;
		if (cycleVerbose) {
			consolelog::printf("Battery SOC: %u%%\n", soc.value);
		}
	}

	reading.batteryPowerOk =
	    readSignedPower(registers::BATTERY_POWER, "Battery power", reading.batteryPowerW);
	if (reading.batteryPowerOk && cycleVerbose) {
		consolelog::printf("Battery power: %.0f W (+discharge / -charge)\n", reading.batteryPowerW);
	}

	if (registers::PV_POWER_VERIFIED) {
		double pv1 = 0.0;
		double pv2 = 0.0;

		reading.pvPowerOk = readSignedPower(registers::PV1_POWER, "PV1 power", pv1) &&
		                    readSignedPower(registers::PV2_POWER, "PV2 power", pv2);
		reading.pvPowerW = reading.pvPowerOk ? pv1 + pv2 : 0.0;
	}

	if (registers::GRID_POWER_VERIFIED) {
		reading.gridPowerOk =
		    readSignedPower(registers::GRID_POWER, "Grid power", reading.gridPowerW);
	}

	if (registers::LOAD_POWER_VERIFIED) {
		reading.loadPowerOk =
		    readSignedPower(registers::LOAD_POWER, "Load power", reading.loadPowerW);
	}

	// With the Serial output off, Modbus errors are reported when they start
	// and when they end, not on every cycle.
	if (!cycleVerbose) {
		if (cycleFailures != 0 && !modbusFaulted) {
			consolelog::printf("Modbus read errors started: %s (%u failed read(s) this cycle); "
			                   "repeats are not printed while telemetry output is off, "
			                   "see \"status\"\n",
			                   firstFailure, cycleFailures);
		} else if (cycleFailures == 0 && modbusFaulted) {
			consolelog::println("Modbus reads recovered");
		}
	}
	modbusFaulted = cycleFailures != 0;

	return reading;
}

void runTelemetryCycle()
{
	const telemetry::Reading reading = pollRegisters();
	const bool verbose = cycleVerbose;
	const telemetry::PublishResult result = publisher.publish(reading, verbose);
	serialConsole.recordCycle(reading, result);

	const int code = static_cast<int>(result);
	const int previous = lastLoggedResult;
	lastLoggedResult = code;

	if (result == telemetry::PublishResult::Published) {
		if (!verbose && previous >= 0 && previous != code) {
			consolelog::println("Telemetry publishing resumed");
		}
		return;
	}

	const uint32_t now = millis();

	// With the Serial output on, the "not published" line is rate-limited as
	// before; with it off, it is printed when the reason changes.
	const bool due = verbose ? (result == telemetry::PublishResult::PublishFailed || firstCycle ||
	                            now - lastSkipLogMs >= config::SKIP_LOG_INTERVAL_MS)
	                         : previous != code;

	if (due) {
		lastSkipLogMs = now;
		consolelog::printf(
		    "Telemetry not published: %s (wifi_status=%d, mqtt_state=%d, time_synced=%s)\n",
		    telemetry::toString(result), static_cast<int>(WiFi.status()), publisher.mqttState(),
		    timesync::isSynced() ? "yes" : "no");
	}
}

void reportFirstTimeSync()
{
	if (timeSyncReported || !timesync::isSynced()) {
		return;
	}

	timeSyncReported = true;
	consolelog::printf("Time synchronized (NTP): %s\n", timesync::nowIso8601Utc().c_str());
}

void runDiagnosticScan()
{
	consolelog::println("--- Candidate register scan (UNVERIFIED, not published) ---");

	for (size_t i = 0; i < registers::CANDIDATE_REGISTER_COUNT; ++i) {
		const registers::CandidateRegister &candidate = registers::CANDIDATE_REGISTERS[i];
		const modbus::ReadResult result = modbusClient.readHoldingRegister(candidate.address);

		if (result.status == modbus::ReadStatus::Ok) {
			consolelog::printf("reg %u: raw=%u signed=%d hex=0x%04X status=ok | %s\n",
			                   candidate.address, result.value, static_cast<int16_t>(result.value),
			                   result.value, candidate.hypothesis);
		} else if (result.status == modbus::ReadStatus::ExceptionResponse) {
			consolelog::printf("reg %u: status=modbus_exception code=0x%02X | %s\n",
			                   candidate.address, result.exceptionCode, candidate.hypothesis);
		} else {
			consolelog::printf("reg %u: status=%s | %s\n", candidate.address,
			                   modbus::toString(result.status), candidate.hypothesis);
		}

		pauseWithConsole(config::MODBUS_INTER_FRAME_DELAY_MS);

		// Keep the MQTT session alive during the scan.
		publisher.loop();
		serialConsole.poll();
	}

	consolelog::println("");
}

} // namespace

void setup()
{
	// The default USB CDC receive buffer is 256 bytes; a larger one keeps
	// pasted input intact while loop() is busy with a Modbus cycle.
	Serial.setRxBufferSize(1024);
	Serial.begin(115200);

	const unsigned long start = millis();
	while (!Serial && millis() - start < 10000) {
		delay(100);
	}

	Serial.println();
	Serial.println("HomePowerGuard ESP32 beta firmware");
	Serial.println(
	    "Read-only Modbus RTU (function 0x03); relay control and inverter writes are not used.");

	deviceconfig::Config deviceConfig;
	const deviceconfig::LoadStatus loadStatus = deviceconfig::load(deviceConfig);

	Serial.printf("Device id: %s\n", deviceConfig.deviceId.c_str());
	Serial.printf("Configuration: %s (load status: %s)\n",
	              loadStatus == deviceconfig::LoadStatus::Stored ? "NVS" : "defaults",
	              deviceconfig::toString(loadStatus));
	Serial.printf("Wi-Fi SSID \"%s\"; MQTT broker %s:%u, %s; Modbus slave %u, %lu baud\n",
	              deviceConfig.wifi.ssid.c_str(), deviceConfig.mqtt.host.c_str(),
	              static_cast<unsigned>(deviceConfig.mqtt.port),
	              deviceConfig.mqtt.username.isEmpty() ? "anonymous" : "with credentials",
	              static_cast<unsigned>(deviceConfig.modbus.slaveId),
	              static_cast<unsigned long>(deviceConfig.modbus.baud));
	if (loadStatus == deviceconfig::LoadStatus::StoredInvalid) {
		Serial.println("Stored configuration is invalid and was left untouched; "
		               "use \"config save\" or \"config reset\" to replace it");
	}
	deviceconfig::Key badKey;
	const char *configError = deviceconfig::validate(deviceConfig, badKey);
	if (configError != nullptr) {
		Serial.printf("Warning: active configuration is not valid: %s %s\n",
		              deviceconfig::keyName(badKey), configError);
	}

	modbusClient.setIdleCallback([]() { serialConsole.service(); });
	modbusClient.setSlaveId(deviceConfig.modbus.slaveId);
	modbusClient.begin(deviceConfig.modbus.baud, config::RS485_RX_PIN, config::RS485_TX_PIN);

	publisher.begin(deviceConfig.wifi, deviceConfig.mqtt, deviceConfig.deviceId);
	serialConsole.begin(deviceConfig, loadStatus);
	timesync::begin();
}

void loop()
{
	serialConsole.poll();
	publisher.loop();
	reportFirstTimeSync();

	const uint32_t now = millis();

	if (firstCycle || now - lastTelemetryMs >= config::TELEMETRY_INTERVAL_MS) {
		lastTelemetryMs = now;
		runTelemetryCycle();
		publisher.loop();
	}

	// The candidate scan only prints diagnostics, so it runs only while the
	// Serial telemetry output is on. While it is off, the timer is held so
	// "telemetry on" does not trigger an immediate extra scan.
	if (config::DIAGNOSTIC_MODE_ENABLED) {
		if (!serialConsole.telemetryOutputEnabled()) {
			lastDiagnosticMs = millis();
		} else if (firstCycle || millis() - lastDiagnosticMs >= config::DIAGNOSTIC_INTERVAL_MS) {
			lastDiagnosticMs = millis();
			runDiagnosticScan();
		}
	}

	firstCycle = false;
	delay(10);
}
