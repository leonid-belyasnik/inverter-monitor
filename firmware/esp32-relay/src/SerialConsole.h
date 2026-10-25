/******************************************************************************
**
** File      SerialConsole.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <Arduino.h>

#include "DeviceConfig.h"
#include "ModbusClient.h"
#include "TelemetryPublisher.h"

namespace console {

// Line-based Serial commands for viewing and changing the configuration
// stored in NVS ("help" lists them). Changes are collected in a draft and
// only validated as a whole, written to NVS, and applied by "config save".
//
// The firmware echoes typed characters itself (USB CDC has no echo, and
// terminals such as "pio device monitor" do not echo locally). Characters of
// password values are echoed as '*'. While a line is being typed, background
// output (consolelog) is held back and written after Enter, so it does not
// split the typed line.
class SerialConsole
{
  public:
	SerialConsole(telemetry::TelemetryPublisher &publisher, modbus::ModbusClient &modbusClient);

	// `active` is the configuration the firmware started with.
	void begin(const deviceconfig::Config &active, deviceconfig::LoadStatus loadStatus);

	// Reads available Serial input, echoes it, and executes complete lines.
	// Never waits for input. Call from every loop() iteration, and only
	// where applying a configuration change is safe (not during a Modbus
	// request).
	void poll();

	// Reads and echoes input only; a completed line waits for the next
	// poll() and further input stays in the Serial buffer until then. Safe
	// to call while a Modbus request is in progress.
	void service();

	// Whether the periodic telemetry blocks are printed to Serial. Off by
	// default; MQTT publishing and Modbus polling do not depend on it.
	bool telemetryOutputEnabled() const;

	// Remembers the result of the latest telemetry cycle for "status".
	void recordCycle(const telemetry::Reading &reading, telemetry::PublishResult result);

  private:
	static constexpr size_t MAX_LINE_LENGTH = 160;
	static constexpr uint32_t RESET_CONFIRM_TIMEOUT_MS = 30000;
	// Background output is released if the user stops typing for this long.
	static constexpr uint32_t INPUT_IDLE_RELEASE_MS = 10000;
	static constexpr size_t NO_MASK = static_cast<size_t>(-1);

	void handleChar(int c);
	void finishLine();
	void runReadyLine();
	size_t maskStart() const;
	void echoLine();
	void updateHold();
	void resetLine();
	void executeLine();
	void setTelemetryOutput(const String &argument, bool hasArgument);
	void executeConfig(const String &arguments, bool resetArmed);
	void setValue(deviceconfig::Key key, const String &value);
	void clearValue(deviceconfig::Key key);
	void updateDraft(const deviceconfig::Config &draft, deviceconfig::Key key);
	void save();
	void discard();
	void reset(bool confirmed, bool resetArmed);
	void applyChanges(const deviceconfig::Config &previous);
	void printHelp();
	void printConfig();
	void printStatus();

	telemetry::TelemetryPublisher &m_publisher;
	modbus::ModbusClient &m_modbusClient;

	char m_line[MAX_LINE_LENGTH + 1] = {};
	size_t m_length = 0;
	bool m_overflow = false;
	bool m_invalidCharacter = false;
	bool m_lastWasCr = false;
	bool m_lineReady = false;
	bool m_reprint = false;
	uint32_t m_lastKeyMs = 0;

	bool m_telemetryOutput = false;
	telemetry::Reading m_lastReading;
	telemetry::PublishResult m_lastResult = telemetry::PublishResult::IncompleteReading;
	bool m_hasCycle = false;
	uint32_t m_lastCycleMs = 0;

	deviceconfig::Config m_active;
	deviceconfig::Config m_draft;
	bool m_hasDraft = false;
	deviceconfig::LoadStatus m_loadStatus = deviceconfig::LoadStatus::NotStored;

	bool m_resetArmed = false;
	uint32_t m_resetArmedMs = 0;
};

} // namespace console
