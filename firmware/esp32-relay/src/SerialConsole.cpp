/******************************************************************************
**
** File      SerialConsole.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "SerialConsole.h"

#include <WiFi.h>

#include <cstring>

#include "ConsoleLog.h"
#include "TimeSync.h"

namespace console {

namespace {

using deviceconfig::Key;

// Splits `text` at the first space. `hasRest` is true if a space was
// found, even when `rest` is empty.
void splitWord(const String &text, String &word, String &rest, bool &hasRest)
{
	const int space = text.indexOf(' ');
	hasRest = space >= 0;
	word = hasRest ? text.substring(0, space) : text;
	rest = hasRest ? text.substring(space + 1) : String();
}

void printSpacingError()
{
	Serial.println("Error: separate words with exactly one space. Type \"help\".");
}

void printValue(const char *label, bool ok, double value, const char *unit, const char *note)
{
	if (ok) {
		Serial.printf("  %-14s %.1f %s%s\n", label, value, unit, note);
	} else {
		Serial.printf("  %-14s n/a (not read in the latest cycle)\n", label);
	}
}

} // namespace

SerialConsole::SerialConsole(telemetry::TelemetryPublisher &publisher,
                             modbus::ModbusClient &modbusClient)
    : m_publisher(publisher), m_modbusClient(modbusClient)
{
}

void SerialConsole::begin(const deviceconfig::Config &active, deviceconfig::LoadStatus loadStatus)
{
	m_active = active;
	m_loadStatus = loadStatus;
	Serial.println("Type \"help\" for configuration commands.");
}

void SerialConsole::poll()
{
	for (;;) {
		service();
		if (!m_lineReady) {
			return;
		}
		runReadyLine();
	}
}

void SerialConsole::service()
{
	while (!m_lineReady && Serial.available() > 0) {
		handleChar(Serial.read());
	}

	// The user stopped typing: let held-back messages through on a new line.
	// The typed text is shown again when typing continues.
	if (!m_lineReady && consolelog::holding() && millis() - m_lastKeyMs >= INPUT_IDLE_RELEASE_MS) {
		Serial.println();
		consolelog::release();
		m_reprint = true;
	}
}

// Enter is CR, LF, or CRLF. The LF of a CRLF pair is swallowed, so one Enter
// executes exactly one line.
void SerialConsole::handleChar(int c)
{
	m_lastKeyMs = millis();

	if (m_reprint) {
		m_reprint = false;
		echoLine();
	}

	if (c == '\r' || c == '\n') {
		if (c == '\n' && m_lastWasCr) {
			m_lastWasCr = false;
			return;
		}
		m_lastWasCr = c == '\r';
		finishLine();
		return;
	}
	m_lastWasCr = false;

	if (c == 0x08 || c == 0x7F) {
		if (m_length != 0 && !m_overflow) {
			m_line[--m_length] = '\0';
			Serial.print("\b \b");
			updateHold();
		}
	} else if (c < 0x20 || c > 0x7E) {
		m_invalidCharacter = true;
		updateHold();
	} else if (m_length >= MAX_LINE_LENGTH) {
		m_overflow = true;
		updateHold();
	} else {
		m_line[m_length++] = static_cast<char>(c);
		const size_t mask = maskStart();
		Serial.write(static_cast<uint8_t>(mask != NO_MASK && m_length - 1 >= mask ? '*' : c));
		updateHold();
	}
}

void SerialConsole::finishLine()
{
	Serial.println();
	m_lineReady = true;
	updateHold();
}

void SerialConsole::runReadyLine()
{
	// Messages produced while the user was typing come first.
	consolelog::release();

	if (m_overflow) {
		Serial.printf("Input rejected: line longer than %u characters\n",
		              static_cast<unsigned>(MAX_LINE_LENGTH));
	} else if (m_invalidCharacter) {
		Serial.println("Input rejected: unsupported character (printable ASCII only)");
	} else if (m_length != 0) {
		executeLine();
	}

	resetLine();
	m_lineReady = false;
	updateHold();
}

// Index of the first character of a secret value in the line being typed, or
// NO_MASK. A value is secret in "config set <key> <value>" unless <key> is a
// known non-secret key; unknown keys are masked too. Extra spaces between
// words do not defeat this. Nothing is masked until the key is complete.
size_t SerialConsole::maskStart() const
{
	size_t i = 0;
	size_t start = 0;
	size_t end = 0;

	const auto nextWord = [&]() {
		while (i < m_length && m_line[i] == ' ') {
			++i;
		}
		start = i;
		while (i < m_length && m_line[i] != ' ') {
			++i;
		}
		end = i;
		return end > start;
	};
	const auto isWord = [&](const char *word) {
		return end - start == strlen(word) && strncmp(m_line + start, word, end - start) == 0;
	};

	if (!nextWord() || !isWord("config") || !nextWord() || !isWord("set") || !nextWord() ||
	    end >= m_length) {
		return NO_MASK;
	}

	Key key;
	if (deviceconfig::findKey(String(m_line).substring(start, end), key) &&
	    !deviceconfig::isSecret(key)) {
		return NO_MASK;
	}

	return end + 1;
}

void SerialConsole::echoLine()
{
	const size_t mask = maskStart();
	for (size_t i = 0; i < m_length; ++i) {
		Serial.write(static_cast<uint8_t>(mask != NO_MASK && i >= mask ? '*' : m_line[i]));
	}
}

// Background output is held back while a line is being typed or waits to be
// executed.
void SerialConsole::updateHold()
{
	if (m_lineReady || m_length != 0 || m_overflow || m_invalidCharacter) {
		consolelog::hold();
	} else {
		consolelog::release();
	}
}

void SerialConsole::resetLine()
{
	memset(m_line, 0, sizeof(m_line));
	m_length = 0;
	m_overflow = false;
	m_invalidCharacter = false;
	m_reprint = false;
}

bool SerialConsole::telemetryOutputEnabled() const
{
	return m_telemetryOutput;
}

void SerialConsole::recordCycle(const telemetry::Reading &reading, telemetry::PublishResult result)
{
	m_lastReading = reading;
	m_lastResult = result;
	m_lastCycleMs = millis();
	m_hasCycle = true;
}

void SerialConsole::setTelemetryOutput(const String &argument, bool hasArgument)
{
	if (hasArgument && argument == "on") {
		m_telemetryOutput = true;
		Serial.println("Serial telemetry output: on (blocks appear with the next 5 s cycle; "
		               "\"telemetry off\" stops them)");
	} else if (hasArgument && argument == "off") {
		m_telemetryOutput = false;
		Serial.println("Serial telemetry output: off (MQTT publishing and Modbus polling continue; "
		               "\"status\" shows the latest values)");
	} else {
		Serial.printf("Usage: telemetry on|off (currently %s)\n", m_telemetryOutput ? "on" : "off");
	}
}

void SerialConsole::executeLine()
{
	// Any command other than "config reset confirm" cancels a pending reset.
	const bool resetArmed = m_resetArmed && millis() - m_resetArmedMs <= RESET_CONFIRM_TIMEOUT_MS;
	m_resetArmed = false;

	String command;
	String arguments;
	bool hasArguments = false;
	splitWord(String(m_line), command, arguments, hasArguments);

	if (command.isEmpty()) {
		printSpacingError();
	} else if (command == "help") {
		if (hasArguments) {
			Serial.println("Error: help takes no argument (input not shown)");
		} else {
			printHelp();
		}
	} else if (command == "status") {
		if (hasArguments) {
			Serial.println("Error: status takes no argument (input not shown)");
		} else {
			printStatus();
		}
	} else if (command == "telemetry") {
		setTelemetryOutput(arguments, hasArguments);
	} else if (command == "config") {
		if (hasArguments) {
			executeConfig(arguments, resetArmed);
		} else {
			Serial.println("Usage: config show|status|set|clear|save|discard|reset. Type \"help\".");
		}
	} else {
		Serial.println("Unknown command (input not shown). Type \"help\".");
	}
}

// Syntax: config <subcommand>[ <key>[ <value>]]. The value is the rest of
// the line after the single space following the key, taken verbatim.
void SerialConsole::executeConfig(const String &arguments, bool resetArmed)
{
	String subcommand;
	String rest;
	bool hasRest = false;
	splitWord(arguments, subcommand, rest, hasRest);

	if (subcommand.isEmpty()) {
		printSpacingError();
		return;
	}

	if (subcommand == "set" || subcommand == "clear") {
		const bool isSet = subcommand == "set";
		if (!hasRest) {
			Serial.println(isSet ? "Usage: config set <key> <value>" : "Usage: config clear <key>");
			return;
		}

		String name;
		String value;
		bool hasValue = false;
		splitWord(rest, name, value, hasValue);

		Key key;
		if (name.isEmpty()) {
			printSpacingError();
		} else if (!deviceconfig::findKey(name, key)) {
			Serial.println("Error: unknown key (input not shown). Type \"help\" for the list.");
		} else if (!isSet) {
			if (hasValue) {
				Serial.println("Error: config clear takes only a key (input not shown)");
			} else {
				clearValue(key);
			}
		} else if (value.isEmpty()) {
			Serial.printf("Error: missing value for %s%s\n", deviceconfig::keyName(key),
			              deviceconfig::isClearable(key) ? "; use \"config clear\" to empty it" : "");
		} else {
			setValue(key, value);
		}
		return;
	}

	if (subcommand == "reset") {
		if (!hasRest) {
			reset(false, resetArmed);
		} else if (rest == "confirm") {
			reset(true, resetArmed);
		} else {
			Serial.println("Error: use \"config reset\" or \"config reset confirm\" "
			               "(input not shown)");
		}
		return;
	}

	const bool takesNoArgument = subcommand == "show" || subcommand == "status" ||
	                             subcommand == "save" || subcommand == "discard";
	if (!takesNoArgument) {
		Serial.println("Unknown config subcommand (input not shown). Type \"help\".");
	} else if (hasRest) {
		Serial.println("Error: this config subcommand takes no argument (input not shown)");
	} else if (subcommand == "show") {
		printConfig();
	} else if (subcommand == "status") {
		printStatus();
	} else if (subcommand == "save") {
		save();
	} else {
		discard();
	}
}

void SerialConsole::setValue(Key key, const String &value)
{
	deviceconfig::Config draft = m_hasDraft ? m_draft : m_active;

	const char *reason = deviceconfig::setValue(draft, key, value);
	if (reason == nullptr) {
		reason = deviceconfig::validateKey(draft, key);
	}
	if (reason != nullptr) {
		Serial.printf("Error: %s %s. Draft not changed.\n", deviceconfig::keyName(key), reason);
		return;
	}

	updateDraft(draft, key);
}

void SerialConsole::clearValue(Key key)
{
	const char *name = deviceconfig::keyName(key);
	if (!deviceconfig::isClearable(key)) {
		Serial.printf("Error: %s cannot be empty; use \"config set %s <value>\"\n", name, name);
		return;
	}

	deviceconfig::Config draft = m_hasDraft ? m_draft : m_active;
	deviceconfig::clearValue(draft, key);

	const char *reason = deviceconfig::validateKey(draft, key);
	if (reason != nullptr) {
		Serial.printf("Error: %s %s. Draft not changed.\n", name, reason);
		return;
	}

	updateDraft(draft, key);
}

void SerialConsole::updateDraft(const deviceconfig::Config &draft, Key key)
{
	m_draft = draft;
	m_hasDraft = !(m_draft == m_active);

	Serial.printf("Draft: %s = %s\n", deviceconfig::keyName(key),
	              deviceconfig::displayValue(m_draft, key).c_str());

	if (!m_hasDraft) {
		Serial.println("Draft matches the active configuration; nothing to save.");
		return;
	}

	Key badKey;
	const char *reason = deviceconfig::validate(m_draft, badKey);
	if (reason != nullptr) {
		Serial.printf("Note: draft is not valid yet: %s %s\n", deviceconfig::keyName(badKey),
		              reason);
	} else {
		Serial.println("Type \"config save\" to store and apply, \"config discard\" to cancel.");
	}
}

void SerialConsole::save()
{
	if (!m_hasDraft) {
		Serial.println("Nothing to save: no unsaved changes");
		return;
	}

	Key badKey;
	const char *reason = deviceconfig::validate(m_draft, badKey);
	if (reason != nullptr) {
		Serial.printf("Save failed: %s %s. Nothing was stored or applied; draft kept.\n",
		              deviceconfig::keyName(badKey), reason);
		return;
	}

	const char *error = deviceconfig::save(m_draft);
	if (error != nullptr) {
		Serial.printf("Save failed: %s. Active configuration not changed; draft kept.\n", error);
		return;
	}

	const deviceconfig::Config previous = m_active;
	m_active = m_draft;
	m_draft = deviceconfig::Config();
	m_hasDraft = false;
	m_loadStatus = deviceconfig::LoadStatus::Stored;

	Serial.println("Configuration saved to NVS");
	applyChanges(previous);
}

void SerialConsole::discard()
{
	if (!m_hasDraft) {
		Serial.println("No unsaved changes");
		return;
	}

	m_draft = deviceconfig::Config();
	m_hasDraft = false;
	Serial.println("Unsaved changes discarded");
}

void SerialConsole::reset(bool confirmed, bool resetArmed)
{
	if (!confirmed) {
		m_resetArmed = true;
		m_resetArmedMs = millis();
		Serial.println("This removes the stored configuration and restores the defaults from "
		               "secrets.h and Config.h.");
		Serial.println("Unsaved changes are discarded; changed Wi-Fi, MQTT and Modbus settings "
		               "are applied immediately.");
		Serial.println("Type \"config reset confirm\" within 30 s to proceed; any other command "
		               "cancels.");
		return;
	}

	if (!resetArmed) {
		Serial.println("Error: run \"config reset\" first, then \"config reset confirm\" within 30 s");
		return;
	}

	const char *error = deviceconfig::clear();
	if (error != nullptr) {
		Serial.printf("Reset failed: %s. Stored and active configuration not changed.\n", error);
		return;
	}

	const deviceconfig::Config previous = m_active;
	m_active = deviceconfig::defaults();
	m_draft = deviceconfig::Config();
	m_hasDraft = false;
	m_loadStatus = deviceconfig::LoadStatus::NotStored;

	Serial.println("Stored configuration removed; defaults restored");
	applyChanges(previous);

	Key badKey;
	const char *reason = deviceconfig::validate(m_active, badKey);
	if (reason != nullptr) {
		Serial.printf("Warning: default configuration is not valid: %s %s\n",
		              deviceconfig::keyName(badKey), reason);
	}
}

void SerialConsole::applyChanges(const deviceconfig::Config &previous)
{
	bool applied = false;

	if (!(previous.wifi == m_active.wifi)) {
		m_publisher.applyWifiSettings(m_active.wifi);
		Serial.println("Applied: Wi-Fi settings (reconnecting)");
		applied = true;
	}

	if (!(previous.mqtt == m_active.mqtt) || !(previous.deviceId == m_active.deviceId)) {
		m_publisher.applyMqttSettings(m_active.mqtt, m_active.deviceId);
		Serial.println("Applied: MQTT settings and device id (reconnecting)");
		applied = true;
	}

	if (!(previous.modbus == m_active.modbus)) {
		if (previous.modbus.slaveId != m_active.modbus.slaveId) {
			m_modbusClient.setSlaveId(m_active.modbus.slaveId);
		}
		if (previous.modbus.baud != m_active.modbus.baud) {
			m_modbusClient.setBaudRate(m_active.modbus.baud);
		}
		Serial.printf("Applied: Modbus slave %u, %lu baud\n",
		              static_cast<unsigned>(m_active.modbus.slaveId),
		              static_cast<unsigned long>(m_active.modbus.baud));
		applied = true;
	}

	if (!applied) {
		Serial.println("No runtime changes to apply");
	}
}

void SerialConsole::printHelp()
{
	Serial.println("HomePowerGuard Serial console. One command per line, words separated by");
	Serial.println("exactly one space. Changes go to a draft until \"config save\".");
	Serial.println();
	Serial.println("Commands:");
	Serial.println("  help                      show this help");
	Serial.println("  status                    Wi-Fi, MQTT, Modbus, time status and latest values");
	Serial.println("  telemetry on|off          print the periodic telemetry blocks to Serial");
	Serial.println("                            (default: off; MQTT and Modbus are not affected)");
	Serial.println("  config show               show configuration (passwords masked) and draft");
	Serial.println("  config status             same as status");
	Serial.println("  config set <key> <value>  change a value in the draft");
	Serial.println("  config clear <key>        empty a value in the draft (only keys that may be");
	Serial.println("                            empty: wifi.password, mqtt.user, mqtt.password)");
	Serial.println("  config save               validate the whole draft, store it in NVS and");
	Serial.println("                            apply the changed parts");
	Serial.println("  config discard            drop the draft");
	Serial.println("  config reset              restore defaults; must be confirmed with");
	Serial.println("                            \"config reset confirm\" within 30 s");
	Serial.println();
	Serial.println("Keys:");
	for (size_t i = 0; i < deviceconfig::KEY_COUNT; ++i) {
		const Key key = deviceconfig::keyAt(i);
		Serial.printf("  %-16s %s\n", deviceconfig::keyName(key), deviceconfig::keyDescription(key));
	}
	Serial.println();
	Serial.println("Value rule: <value> is everything after \"config set <key> \" up to the end of");
	Serial.println("the line, taken literally; spaces inside are kept, leading/trailing spaces are");
	Serial.println("rejected. Printable ASCII only, max 160 characters per line. Typed characters");
	Serial.println("are echoed by the device; password values are shown as '*' and never printed.");
	Serial.println("Enter may be CR, LF or CRLF. Backspace/Delete erase the last character.");
	Serial.println();
	Serial.println("Examples:");
	Serial.println("  config set wifi.ssid My Home WiFi");
	Serial.println("  config set wifi.password <wifi-password>");
	Serial.println("  config set mqtt.host 192.168.1.20");
	Serial.println("  config set mqtt.port 1883");
	Serial.println("  config set mqtt.user hpg-device");
	Serial.println("  config set mqtt.password <mqtt-password>");
	Serial.println("  config show");
	Serial.println("  config save");
}

void SerialConsole::printConfig()
{
	const bool stored = m_loadStatus == deviceconfig::LoadStatus::Stored;
	Serial.printf("Configuration (source: %s):\n", stored ? "NVS" : "defaults");
	for (size_t i = 0; i < deviceconfig::KEY_COUNT; ++i) {
		const Key key = deviceconfig::keyAt(i);
		Serial.printf("  %-16s %s\n", deviceconfig::keyName(key),
		              deviceconfig::displayValue(m_active, key).c_str());
	}

	if (m_hasDraft) {
		Serial.println("Unsaved changes (\"config save\" to store and apply, \"config discard\" "
		               "to cancel):");
		for (size_t i = 0; i < deviceconfig::KEY_COUNT; ++i) {
			const Key key = deviceconfig::keyAt(i);
			if (!deviceconfig::sameValue(m_active, m_draft, key)) {
				Serial.printf("  %-16s %s -> %s%s\n", deviceconfig::keyName(key),
				              deviceconfig::displayValue(m_active, key).c_str(),
				              deviceconfig::displayValue(m_draft, key).c_str(),
				              deviceconfig::isSecret(key) ? " (changed)" : "");
			}
		}
	} else {
		Serial.println("Unsaved changes: none");
	}

	if (m_loadStatus == deviceconfig::LoadStatus::StoredInvalid) {
		Serial.println("Note: the stored configuration is invalid and was ignored; "
		               "\"config reset\" removes it.");
	} else if (m_loadStatus == deviceconfig::LoadStatus::NvsError) {
		Serial.println("Note: NVS could not be opened at boot; defaults are in use.");
	}
}

void SerialConsole::printStatus()
{
	const wl_status_t wifiStatus = WiFi.status();
	if (wifiStatus == WL_CONNECTED) {
		Serial.printf("Wi-Fi:  connected to \"%s\", IP %s, RSSI %d dBm\n", WiFi.SSID().c_str(),
		              WiFi.localIP().toString().c_str(), static_cast<int>(WiFi.RSSI()));
	} else {
		Serial.printf("Wi-Fi:  not connected (status %d), configured SSID \"%s\"\n",
		              static_cast<int>(wifiStatus), m_active.wifi.ssid.c_str());
	}

	const deviceconfig::MqttSettings &mqtt = m_active.mqtt;
	if (m_publisher.isConnected()) {
		Serial.printf("MQTT:   connected to %s:%u as \"%s\" (%s)\n", mqtt.host.c_str(),
		              static_cast<unsigned>(mqtt.port), m_active.deviceId.c_str(),
		              mqtt.username.isEmpty() ? "anonymous" : "with credentials");
	} else {
		Serial.printf("MQTT:   not connected (state %d), broker %s:%u\n", m_publisher.mqttState(),
		              mqtt.host.c_str(), static_cast<unsigned>(mqtt.port));
	}
	Serial.printf("        topic %s\n", m_publisher.topic().c_str());

	Serial.printf("Modbus: slave %u, %lu baud, 8-N-1\n",
	              static_cast<unsigned>(m_modbusClient.slaveId()),
	              static_cast<unsigned long>(m_modbusClient.baudRate()));
	if (m_modbusClient.hasValidResponse()) {
		Serial.printf("        last valid response %lu ms ago, last request: %s\n",
		              static_cast<unsigned long>(millis() - m_modbusClient.lastValidResponseMs()),
		              modbus::toString(m_modbusClient.lastStatus()));
	} else {
		Serial.println("        no valid response since boot");
	}
	Serial.printf("        valid responses %lu, failed requests %lu\n",
	              static_cast<unsigned long>(m_modbusClient.validResponseCount()),
	              static_cast<unsigned long>(m_modbusClient.failedRequestCount()));

	Serial.printf("Time:   %s\n", timesync::isSynced() ? "synchronized (NTP)" : "not synchronized");
	Serial.printf("Serial telemetry output: %s\n", m_telemetryOutput ? "on" : "off");

	if (!m_hasCycle) {
		Serial.println("Latest cycle: none yet");
	} else {
		const telemetry::Reading &r = m_lastReading;
		Serial.printf("Latest cycle: %lu ms ago, publish result: %s, measured_at %s\n",
		              static_cast<unsigned long>(millis() - m_lastCycleMs),
		              telemetry::toString(m_lastResult),
		              r.measuredAt.isEmpty() ? "<no time sync>" : r.measuredAt.c_str());
		printValue("battery SOC", r.batterySocOk, r.batterySocPct, "%", "");
		printValue("battery power", r.batteryPowerOk, r.batteryPowerW, "W", " (+discharge / -charge)");
		printValue("grid voltage", r.gridVoltageOk, r.gridVoltageV, "V", "");
		if (r.gridRelayOk) {
			Serial.printf("  grid relay:    %s\n", r.gridRelayOn ? "ON" : "OFF");
		} else {
			Serial.println("  grid relay:    n/a (not read in the latest cycle)");
		}
		printValue("PV power", r.pvPowerOk, r.pvPowerW, "W", " (unconfirmed register)");
		printValue("grid power", r.gridPowerOk, r.gridPowerW, "W", " (unconfirmed register)");
		printValue("load power", r.loadPowerOk, r.loadPowerW, "W", " (unconfirmed register)");
	}

	Serial.printf("Config: source %s (load status: %s), unsaved changes: %s\n",
	              m_loadStatus == deviceconfig::LoadStatus::Stored ? "NVS" : "defaults",
	              deviceconfig::toString(m_loadStatus), m_hasDraft ? "yes" : "no");
}

} // namespace console
