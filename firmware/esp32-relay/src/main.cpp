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
#include "ModbusClient.h"
#include "RegisterMap.h"
#include "TelemetryPublisher.h"
#include "TimeSync.h"
#include "secrets.h"

namespace {

modbus::ModbusClient modbusClient(Serial1, config::MODBUS_SLAVE_ID, config::RS485_DE_PIN);

WiFiClient wifiClient;
telemetry::TelemetryPublisher publisher(wifiClient, secrets::DEVICE_ID);

uint32_t lastTelemetryMs = 0;
uint32_t lastDiagnosticMs = 0;
uint32_t lastSkipLogMs = 0;
bool firstCycle = true;
bool timeSyncReported = false;

// Reads one register and pauses for the inter-frame gap. Failures are logged
// and reported through the result status; no previous value is reused.
modbus::ReadResult readRegister(uint16_t address, const char *label)
{
	const modbus::ReadResult result = modbusClient.readHoldingRegister(address);
	delay(config::MODBUS_INTER_FRAME_DELAY_MS);

	if (result.status == modbus::ReadStatus::ExceptionResponse) {
		Serial.printf("%s (reg %u): modbus exception 0x%02X\n", label, address,
		              result.exceptionCode);
	} else if (result.status != modbus::ReadStatus::Ok) {
		Serial.printf("%s (reg %u): %s\n", label, address, modbus::toString(result.status));
	}

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

	Serial.println("--- Deye telemetry ---");

	const auto voltage = readRegister(registers::GRID_VOLTAGE, "Grid voltage");
	if (voltage.status == modbus::ReadStatus::Ok) {
		reading.gridVoltageOk = true;
		reading.gridVoltageV = voltage.value / 10.0;
		Serial.printf("Grid voltage: %.1f V\n", reading.gridVoltageV);
	}

	const auto relay = readRegister(registers::GRID_RELAY_STATUS, "Grid relay");
	if (relay.status == modbus::ReadStatus::Ok) {
		reading.gridRelayOk = true;
		reading.gridRelayOn = (relay.value & 1) != 0;
		Serial.printf("Grid relay: %s\n", reading.gridRelayOn ? "ON" : "OFF");
	}

	const auto soc = readRegister(registers::BATTERY_SOC, "Battery SOC");
	if (soc.status == modbus::ReadStatus::Ok) {
		reading.batterySocOk = true;
		reading.batterySocPct = soc.value;
		Serial.printf("Battery SOC: %u%%\n", soc.value);
	}

	reading.batteryPowerOk =
	    readSignedPower(registers::BATTERY_POWER, "Battery power", reading.batteryPowerW);
	if (reading.batteryPowerOk) {
		Serial.printf("Battery power: %.0f W (+discharge / -charge)\n", reading.batteryPowerW);
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

	return reading;
}

void runTelemetryCycle()
{
	const telemetry::Reading reading = pollRegisters();
	const telemetry::PublishResult result = publisher.publish(reading);

	if (result == telemetry::PublishResult::Published) {
		return;
	}

	const uint32_t now = millis();

	if (result == telemetry::PublishResult::PublishFailed || firstCycle ||
	    now - lastSkipLogMs >= config::SKIP_LOG_INTERVAL_MS) {
		lastSkipLogMs = now;
		Serial.printf(
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
	Serial.printf("Time synchronized (NTP): %s\n", timesync::nowIso8601Utc().c_str());
}

void runDiagnosticScan()
{
	Serial.println("--- Candidate register scan (UNVERIFIED, not published) ---");

	for (size_t i = 0; i < registers::CANDIDATE_REGISTER_COUNT; ++i) {
		const registers::CandidateRegister &candidate = registers::CANDIDATE_REGISTERS[i];
		const modbus::ReadResult result = modbusClient.readHoldingRegister(candidate.address);

		if (result.status == modbus::ReadStatus::Ok) {
			Serial.printf("reg %u: raw=%u signed=%d hex=0x%04X status=ok | %s\n", candidate.address,
			              result.value, static_cast<int16_t>(result.value), result.value,
			              candidate.hypothesis);
		} else if (result.status == modbus::ReadStatus::ExceptionResponse) {
			Serial.printf("reg %u: status=modbus_exception code=0x%02X | %s\n", candidate.address,
			              result.exceptionCode, candidate.hypothesis);
		} else {
			Serial.printf("reg %u: status=%s | %s\n", candidate.address,
			              modbus::toString(result.status), candidate.hypothesis);
		}

		delay(config::MODBUS_INTER_FRAME_DELAY_MS);

		// Keep the MQTT session alive during the scan.
		publisher.loop();
	}

	Serial.println();
}

} // namespace

void setup()
{
	Serial.begin(115200);

	const unsigned long start = millis();
	while (!Serial && millis() - start < 10000) {
		delay(100);
	}

	Serial.println();
	Serial.println("HomePowerGuard ESP32 beta firmware");
	Serial.println(
	    "Read-only Modbus RTU (function 0x03); relay control and inverter writes are not used.");
	Serial.printf("Device id: %s\n", secrets::DEVICE_ID);

	modbusClient.begin(config::MODBUS_BAUD, config::RS485_RX_PIN, config::RS485_TX_PIN);

	publisher.begin();
	timesync::begin();
}

void loop()
{
	publisher.loop();
	reportFirstTimeSync();

	const uint32_t now = millis();

	if (firstCycle || now - lastTelemetryMs >= config::TELEMETRY_INTERVAL_MS) {
		lastTelemetryMs = now;
		runTelemetryCycle();
		publisher.loop();
	}

	if (config::DIAGNOSTIC_MODE_ENABLED &&
	    (firstCycle || millis() - lastDiagnosticMs >= config::DIAGNOSTIC_INTERVAL_MS)) {
		lastDiagnosticMs = millis();
		runDiagnosticScan();
	}

	firstCycle = false;
	delay(10);
}
