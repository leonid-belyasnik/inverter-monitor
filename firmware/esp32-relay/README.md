# ESP32-S3 Relay Firmware

Firmware for the Waveshare ESP32-S3-Relay-1CH board, part of HomePowerGuard.

The firmware reads telemetry from a Deye hybrid inverter over isolated RS485 using Modbus RTU and publishes it to the HomePowerGuard MQTT broker. It is read-only: relay control and inverter-setting writes are not implemented.

This is a **beta**. It publishes battery SOC and battery power from verified registers. PV, grid, and load power are also published, but **temporarily from unconfirmed registers** (PV 186 + 187, grid 169, load 178) that have not been checked against the physical inverter. See [Beta Limitations](#beta-limitations) and [REGISTER_MAP.md](REGISTER_MAP.md).

## Hardware

- Board: Waveshare ESP32-S3-Relay-1CH
- MCU: ESP32-S3
- Flash: 16 MB
- PSRAM: 8 MB
- Framework: Arduino
- Build system: PlatformIO
- Serial monitor: 115200 baud
- Modbus RTU: 9600 baud, 8-N-1
- Modbus slave address: `1`

PlatformIO uses the generic `esp32-s3-devkitc-1` board definition. Hardware-specific settings are configured in `platformio.ini`.

## Source Layout

| File | Purpose |
|---|---|
| `include/Config.h` | Non-secret settings: pins, Modbus, intervals, diagnostic mode, NTP servers |
| `include/secrets.example.h` | Template for Wi-Fi/MQTT credentials and device id |
| `include/secrets.h` | Local copy of the template with real values (gitignored) |
| `include/RegisterMap.h` | Verified and candidate registers, production gates |
| `src/ModbusClient.*` | Modbus RTU master, function `0x03` only |
| `src/TimeSync.*` | SNTP setup and UTC timestamp formatting |
| `src/TelemetryPublisher.*` | Wi-Fi/MQTT connection handling and JSON publishing |
| `src/DeviceConfig.*` | Runtime configuration: keys, validation, NVS storage |
| `src/SerialConsole.*` | Serial `help` / `status` / `telemetry` / `config ...` commands, echo and line editing |
| `src/ConsoleLog.*` | Background Serial output, held back while a command is typed |
| `src/main.cpp` | Poll loop and diagnostic scan |

## Configuration

Create the local secrets file before the first build:

```bash
cp firmware/esp32-relay/include/secrets.example.h firmware/esp32-relay/include/secrets.h
```

Edit `secrets.h` and set:

- `WIFI_SSID`, `WIFI_PASSWORD`;
- `MQTT_HOST`, `MQTT_PORT` (default `1883`, matching `docker/mosquitto/config/mosquitto.conf`);
- `MQTT_USERNAME`, `MQTT_PASSWORD` (leave empty for anonymous access);
- `DEVICE_ID` — used in the topic and as the MQTT client id (letters, digits, `.`, `-`, `_`).

All `secrets.h` values, together with `MODBUS_SLAVE_ID` and `MODBUS_BAUD` from `Config.h`, are defaults. A configuration saved on the device (see [Serial Console Configuration](#serial-console-configuration)) takes precedence over them.

`secrets.h` is listed in `.gitignore`. Never commit it. The build fails if it is missing.

Other settings are in `include/Config.h`:

| Setting | Default | Meaning |
|---|---|---|
| `TELEMETRY_INTERVAL_MS` | `5000` | Poll and publish interval |
| `DIAGNOSTIC_MODE_ENABLED` | `true` | Print candidate registers to serial |
| `DIAGNOSTIC_INTERVAL_MS` | `15000` | Candidate scan interval |
| `MODBUS_RESPONSE_TIMEOUT_MS` | `500` | Per-request timeout |
| `WIFI_CONNECT_TIMEOUT_MS` | `15000` | Time before a Wi-Fi attempt is restarted |
| `MQTT_RETRY_INTERVAL_MS` | `5000` | Minimum gap between MQTT connect attempts |
| `NTP_SERVER_PRIMARY` / `_SECONDARY` | `pool.ntp.org` / `time.google.com` | SNTP servers |

## Serial Console Configuration

Wi-Fi, MQTT, device id, and Modbus settings can be viewed and changed in the Serial Monitor without reflashing. Type `help` for the full command list, keys, and examples.

### Storage

- The configuration is stored with the Arduino `Preferences` library in the NVS partition of the flash (namespace `hpg-config`, key `cfg`) as one record, so all values are replaced together.
- At boot the firmware loads the record. If none exists, the defaults from `secrets.h` and `Config.h` are used. The boot log shows the source, without passwords:

  ```text
  Device id: deye-esp32-001
  Configuration: NVS (load status: stored)
  Wi-Fi SSID "HomeNet"; MQTT broker 192.168.1.20:1883, with credentials; Modbus slave 1, 9600 baud
  ```

  `load status` is `stored`, `not_stored` (defaults used), `stored_invalid` (record unreadable or invalid; defaults used, record left untouched), or `nvs_error` (NVS could not be opened; defaults used).
- A normal firmware upload does not erase NVS, so the saved configuration survives reflashing. Erasing the whole flash (`pio run -t erase`) removes it.
- **NVS does not encrypt data.** The Wi-Fi and MQTT passwords are stored in plain form; anyone with physical access to the board can read them from the flash. NVS encryption and flash encryption are not enabled in this firmware.

### Commands

One command per line, words separated by exactly one space. Changes go to a *draft* and take effect only after `config save`.

| Command | Effect |
|---|---|
| `help` | Full list of commands and keys with allowed values, value rule, examples |
| `status` | Wi-Fi, MQTT, Modbus, and time-sync state, the Serial telemetry mode, and the latest measurements |
| `telemetry on` / `telemetry off` | Start or stop printing the periodic telemetry blocks to Serial (default: off) |
| `config show` | Active configuration and its source (passwords masked), then unsaved changes |
| `config status` | Same as `status` |
| `config set <key> <value>` | Change one value in the draft |
| `config clear <key>` | Empty a value in the draft (only `wifi.password`, `mqtt.user`, `mqtt.password`) |
| `config save` | Validate the whole draft, write it to NVS, read it back, and apply the changed parts |
| `config discard` | Drop the draft |
| `config reset` | Ask to restore the defaults; `config reset confirm` within 30 s performs it |

### Keys and Allowed Values

| Key | Allowed values | Default |
|---|---|---|
| `wifi.ssid` | 1–32 printable ASCII characters | `WIFI_SSID` |
| `wifi.password` | 8–63 printable ASCII characters; empty = open network | `WIFI_PASSWORD` |
| `mqtt.host` | Host name or IPv4 address, 1–64 characters: letters, digits, `.`, `-`, `_` | `MQTT_HOST` |
| `mqtt.port` | 1–65535 | `MQTT_PORT` |
| `mqtt.user` | 0–64 printable ASCII characters; empty = anonymous; required if `mqtt.password` is set | `MQTT_USERNAME` |
| `mqtt.password` | 0–64 printable ASCII characters | `MQTT_PASSWORD` |
| `device.id` | 1–64 characters: letters, digits, `.`, `-`, `_` | `DEVICE_ID` |
| `modbus.slave_id` | 1–247 | `MODBUS_SLAVE_ID` (`1`) |
| `modbus.baud` | 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200 (always 8-N-1) | `MODBUS_BAUD` (`9600`) |

Parsing rules:

- `<value>` is everything after `config set <key> ` up to the end of the line, taken literally. Spaces inside a value (SSID, passwords, username) are kept; leading or trailing spaces are rejected, so `config set wifi.ssid  Home` (two spaces) is an error, not `" Home"`.
- An empty value is never accepted by `config set`; use `config clear <key>`.
- Numbers are plain decimal digits.
- Lines longer than 160 characters or containing non-ASCII or control characters are rejected as a whole. Backspace is supported.
- Each value is checked when it is set; an invalid value is rejected and the draft is not changed. Rules that involve two keys (`mqtt.user` required for `mqtt.password`) are reported as a note and checked again by `config save`.

### First Setup

Example (values are placeholders):

```text
help
config set wifi.ssid My Home WiFi
config set wifi.password <wifi-password>
config set mqtt.host 192.168.1.20
config set mqtt.port 1883
config set mqtt.user hpg-device
config set mqtt.password <mqtt-password>
config show
config save
config status
```

Each `config set` is confirmed, e.g. `Draft: mqtt.host = 192.168.1.20` or `Draft: mqtt.password = <set>`. `config show` lists the draft as `key  old -> new`; for passwords only `<set>`/`<empty>` and `(changed)` are shown.

### Changing and Saving

Set only the values to change and run `config save`. Other values keep their active values. `config save`:

1. validates the whole draft; on error it prints `Save failed: <key> <reason>` and neither stores nor applies anything;
2. writes the record to NVS and reads it back;
3. applies only the groups that changed:
   - Wi-Fi: closes the MQTT session, disconnects Wi-Fi, and connects to the new network (`Wi-Fi settings changed, connecting to SSID "..."`);
   - MQTT or device id: closes the MQTT session (`MQTT settings changed, disconnecting from current broker`), prints the new topic, and connects immediately with the new broker, credentials, and client id;
   - Modbus: the new slave id and baud rate are used from the next request.

Changing `device.id` changes the MQTT topic `v1/devices/<device.id>/telemetry`; the backend then stores the data under the new id.

### Discard and Reset

- `config discard` drops the draft. The active and stored configuration are not touched.
- `config reset` prints what will happen and must be confirmed with `config reset confirm` within 30 s; any other command cancels it. The reset removes the stored record, restores the defaults from `secrets.h` and `Config.h`, discards the draft, and applies the changed groups as above. Rebuilding with new `secrets.h` values has no effect while a record is stored; run `config reset` first.

### Errors

- Validation errors: `Error: <key> <reason>. Draft not changed.` or, on save, `Save failed: <key> <reason>`.
- NVS errors on save (`NVS open failed`, `NVS write failed`, `NVS read-back verification failed`): the active configuration is not changed and the draft is kept. If NVS could not be opened or written, the previously stored record also stays unchanged.
- NVS error on reset: `Reset failed: <reason>`; nothing changes.
- Unknown commands, unknown keys, and malformed lines are reported without repeating the input.

### Diagnostics

`config status` example:

```text
Wi-Fi:  connected to "HomeNet", IP 192.168.1.50, RSSI -61 dBm
MQTT:   connected to 192.168.1.20:1883 as "deye-esp32-001" (with credentials)
        topic v1/devices/deye-esp32-001/telemetry
Modbus: slave 1, 9600 baud, 8-N-1
        last valid response 1830 ms ago, last request: ok
        valid responses 412, failed requests 3
Time:   synchronized (NTP)
Config: source NVS (load status: stored), unsaved changes: no
```

- Wi-Fi `status` codes are the `WiFi.status()` values listed in [Serial Monitor](#serial-monitor).
- MQTT `state` is PubSubClient `state()` (0 = connected, negative = disconnected or connection failed, positive = refused by the broker, e.g. 4 = bad credentials, 5 = not authorized).
- Modbus counts every request since boot (telemetry and diagnostic scan). A Modbus exception reply counts as a valid response because the inverter answered.

### Safe Password Entry

- The firmware echoes what you type, but shows the characters of a password value as `*` (for `config set <key> <value>` the value is masked unless `<key>` is a known non-secret key, so a mistyped key is masked too). It never prints passwords. `config show`, `status`, and `config status` show passwords only as `<set>`/`<empty>`. If the command word itself is mistyped (for example `confg set ...`), the line is echoed as typed, so type the command word carefully before the password.
- `pio device monitor` does not echo locally by default, so the characters you see come from the device. Do not add `--echo` or logging filters such as `log2file` while entering a password: they show or store the typed text on the computer.
- Close the monitor (or clear its scrollback) after configuration when other people can access the computer.
- The USB serial port has no authentication: anyone with physical USB access can read the non-secret configuration and change all settings.

### Commands and the Poll Loop

Serial input is read without waiting. Typed characters are echoed immediately, even while a Modbus request is in progress. A command is executed when the line is complete and the firmware is back in its main loop, i.e. after the current telemetry cycle (normally well under a second, a few seconds if the inverter does not answer). A command never interrupts a Modbus request, and a saved configuration is applied between requests. MQTT publishing, Modbus polling, time sync, and Wi-Fi/MQTT reconnection run the same way whether or not the Serial telemetry output is on.

### Typing in the Serial Monitor

- The device echoes typed characters. Enter may be CR, LF, or CRLF; one Enter runs exactly one command. Backspace and Delete erase the last character. Lines are limited to 160 printable ASCII characters; longer or non-ASCII lines are rejected as a whole.
- While you type a line, other messages (Wi-Fi events, MQTT reconnect attempts, error summaries) are held back in a 1.5 KB buffer and printed after Enter, so they do not split the line. If you stop typing for 10 s, they are printed on a new line and your text is shown again when you continue. If more than the buffer holds arrives, the rest is dropped and `[N message(s) dropped while typing]` is printed.
- Commands pasted in bulk work as long as each line is at most 160 characters; the receive buffer is 1 KB.

### Settings That Require Reflashing

| Setting | Location | Reason |
|---|---|---|
| RS485 pins (`RS485_TX_PIN`, `RS485_RX_PIN`, `RS485_DE_PIN`) | `Config.h` | Board wiring; not a runtime setting |
| Modbus frame format 8-N-1, `MODBUS_RESPONSE_TIMEOUT_MS`, `MODBUS_INTER_FRAME_DELAY_MS` | `Config.h`, `ModbusClient` | Fixed protocol timing |
| `TELEMETRY_INTERVAL_MS`, `DIAGNOSTIC_INTERVAL_MS`, `DIAGNOSTIC_MODE_ENABLED` | `Config.h` | Compile-time constants |
| `WIFI_CONNECT_TIMEOUT_MS`, `MQTT_RETRY_INTERVAL_MS` | `Config.h` | Compile-time constants |
| NTP servers | `Config.h` | Compile-time constants |
| Register addresses and publication gates | `RegisterMap.h` | Require hardware verification |
| MQTT topic format, QoS, TLS (not supported) | `TelemetryPublisher` | Protocol contract with the backend |

## Build

Run from the repository root:

```bash
pio run -d firmware/esp32-relay
```

PlatformIO downloads the `PubSubClient` and `ArduinoJson` (v6) libraries on the first build.

## Upload

Stop the serial monitor before uploading.

Try the normal upload first:

```bash
pio run -d firmware/esp32-relay -t upload --upload-port /dev/ttyACM0
```

The serial port may change after reconnecting the board.

### Manual bootloader mode

If PlatformIO reports `Failed to connect to ESP32-S3: No serial data received`:

1. Hold **BOOT**.
2. Briefly press and release **RESET**, keeping BOOT pressed.
3. Release **BOOT**.
4. Run the upload command again.
5. After a successful upload, press **RESET** once to start the new firmware.

The manual procedure is a workaround for automatic bootloader entry not working reliably over USB passthrough.

## Serial Monitor

Open the monitor:

```bash
pio device monitor --port /dev/ttyACM0 --baud 115200
```

Exit with `Ctrl+C`.

By default the periodic telemetry blocks are **not** printed, so the console stays usable for typing. Type `telemetry on` to print them and `telemetry off` to stop; `status` shows the state and the latest measurements at any time. This only affects Serial output: Modbus polling and MQTT publishing continue unchanged, and `telemetry on` does not start an extra read or publish.

With `telemetry off`, errors stay visible but are not repeated every cycle: Modbus read errors are printed when they start (`Modbus read errors started: ...`) and when they end (`Modbus reads recovered`); `Telemetry not published: ...` is printed when its reason changes, and `Telemetry publishing resumed` when publishing works again. Wi-Fi/MQTT connection messages and configuration warnings are always printed.

With `telemetry on`, every telemetry cycle prints the verified readings and the publish result. Example:

```text
--- Deye telemetry ---
Grid voltage: 217.5 V
Grid relay: ON
Battery SOC: 98%
Battery power: -203 W (+discharge / -charge)
Published seq=1: {"schema_version":1,"seq":1,"measured_at":"2026-10-22T09:15:00.120Z","measurement":{"battery_power_w":-203,"battery_soc_pct":98,"grid_voltage_v":217.5,"grid_relay_on":true}}
```

Actual values depend on the inverter's operating conditions.

Possible publish results:

| Result | Meaning |
|---|---|
| `published` | Payload sent; the line `Published seq=...` shows it |
| `incomplete_reading` | Battery SOC or battery power failed to read in this cycle |
| `no_timestamp` | Clock not yet synchronized via NTP |
| `wifi_not_connected` | Wi-Fi is down |
| `mqtt_not_connected` | Wi-Fi is up, MQTT session is down |
| `publish_failed` | Serialization or MQTT publish failed |

Checks run in table order, so `no_timestamp` can hide a Wi-Fi outage. Each "not published" line therefore also shows `wifi_status` (`WiFi.status()`: 3 = connected, 1 = no SSID, 4 = connect failed, 5 = connection lost, 6 = disconnected), `mqtt_state` (PubSubClient `state()`), and `time_synced`.

"Not published" messages are rate-limited to one per 10 s, except `publish_failed`, which is always logged.

Wi-Fi events are logged from `loop()` as `Wi-Fi event: STA_CONNECTED | STA_GOT_IP | STA_LOST_IP | STA_DISCONNECTED, reason=<code> (<name>)`, followed by `status=<WiFi.status()>`. IP and RSSI are shown only after `STA_GOT_IP`. Arduino-ESP32 2.x has no separate reconnect-started event; the firmware's own retry prints `Wi-Fi not connected, retrying`, and its `WiFi.disconnect()` produces `reason=8 (ASSOC_LEAVE)`. The first NTP sync is logged once as `Time synchronized (NTP): <timestamp>`.

After a disconnect with `reason=201 (NO_AP_FOUND)`, `2 (AUTH_EXPIRE)` or `202 (AUTH_FAIL)`, the firmware runs one asynchronous Wi-Fi scan (not while connected, at most once per 60 s) and prints each network's SSID, channel, RSSI and auth type, followed by `Configured SSID: FOUND` or `NOT FOUND`. The scan does not block `loop()`, changes no Wi-Fi settings and does not select a channel. The configured password is never printed; neighbouring SSIDs are.

### Diagnostic Mode

With `DIAGNOSTIC_MODE_ENABLED = true` and `telemetry on`, the firmware scans the candidate registers every 15 s. With `telemetry off` the scan does not run (it only prints diagnostics), and after `telemetry on` the first scan comes 15 s later. Each register is read individually; a failure does not stop the scan. Example:

```text
--- Candidate register scan (UNVERIFIED, not published) ---
reg 169: raw=65336 signed=-200 hex=0xFF38 status=ok | Total grid power ...
reg 179: status=modbus_exception code=0x02 | No public source found ...
```

Diagnostic values are never published. Use them to verify registers as described in [REGISTER_MAP.md](REGISTER_MAP.md#verification-procedure).

## RS485 Wiring

The inverter is a Deye SUN-6K-SG05LP1-EU-AM2-P.

Use the inverter's designated RS485/Modbus connector, not its Ethernet/LAN port.

| Inverter RJ45 pin | Signal | ESP32-S3-Relay-1CH |
|---|---|---|
| 1 | B | `B-` |
| 2 | A | `A+` |

The current installation uses a 30 m Ethernet cable and the white-orange twisted pair:

- White-orange → `B-`
- Orange → `A+`

Only the differential pair is connected for the current test. Do not connect additional ground wires to the board's power ground unless the RS485 isolation circuitry has been checked against the schematic.

## Modbus Telemetry

The firmware uses Modbus RTU function `0x03` (Read Holding Registers) only.

| Register | Measurement | Conversion | Status |
|---|---|---|---|
| `150` | Grid voltage | Raw value × 0.1 V | Verified |
| `184` | Battery state of charge | Raw value in % | Verified |
| `190` | Battery power | Signed 16-bit W; `+` discharge, `−` charge | Verified (discharge sign pending) |
| `194` | Grid relay status | Bit 0: `1` = ON, `0` = OFF | Verified |

PV (186 + 187), grid (169), and load (178) power registers are **unconfirmed** and published temporarily. See [REGISTER_MAP.md](REGISTER_MAP.md) for candidates, sources, and the verification procedure.

Registers are read individually. Each response is checked for slave address, function code, byte count, length, and CRC. Modbus exception replies (function `0x83`) are reported with their exception code. A failed read is reported as failed for that cycle; the firmware never reuses a previous value.

`Grid relay: ON` represents the inverter's grid-relay status. It does not independently guarantee that usable grid voltage is present, and `OFF` does not by itself prove that the external grid has no voltage (the inverter may keep the relay open, for example, while the voltage is out of range).

## MQTT Telemetry

Topic:

```text
v1/devices/<DEVICE_ID>/telemetry
```

Payload (`schema_version` 1, same schema as `simulator/main.cpp`, parsed by `backend/protocol/TelemetryParser.cpp`). With the current register gates (all enabled; PV, grid, and load values are unconfirmed):

```json
{
  "schema_version": 1,
  "seq": 42,
  "measured_at": "2026-10-21T12:34:56.789Z",
  "measurement": {
    "pv_power_w": 1820,
    "grid_power_w": -200,
    "battery_power_w": -450,
    "load_power_w": 1170,
    "battery_soc_pct": 76,
    "grid_voltage_v": 230.4,
    "grid_relay_on": true
  }
}
```

If a gate is set back to `false`, or its read fails in a cycle, that field is omitted from the payload.

- `battery_soc_pct` and `battery_power_w` are required. If either fails to read, nothing is published for that cycle (`incomplete_reading`).
- `pv_power_w`, `grid_power_w`, and `load_power_w` are optional. Each is included only when its register gate is enabled and the read succeeded in the current cycle; otherwise it is omitted. The backend stores omitted fields as `NULL` and returns them as `null`.
- `grid_voltage_v` (register 150 × 0.1 V, number) and `grid_relay_on` (register 194 bit 0, JSON boolean) are optional. Each is included only when its register was read successfully in the current cycle; otherwise it is omitted, never sent as `0` or `false`. `grid_relay_on` is the relay state, not a grid-presence flag.
- Missing values are never replaced by zero or an estimate, because the backend would store them as real measurements.
- A message is published only when the clock is synchronized.
- `seq` starts at 1 after each boot and increases by one per published message.
- `measured_at` is the UTC time at the start of the poll cycle.
- Messages are published with QoS 0, not retained (PubSubClient supports QoS 0 publishing only).

## Time Synchronization

The firmware starts SNTP in UTC at boot. Until the clock reports a time after 2024-01-01, it is treated as unsynchronized and no telemetry is published (`no_timestamp`). Timestamps use the format `YYYY-MM-DDTHH:MM:SS.mmmZ`.

## Connection Handling

- Wi-Fi: if not connected within `WIFI_CONNECT_TIMEOUT_MS`, the attempt is restarted.
- MQTT: one connection attempt at most every `MQTT_RETRY_INTERVAL_MS`, only while Wi-Fi is connected.
- Modbus polling continues regardless of network state, so serial diagnostics remain available offline.
- Readings taken while disconnected are not buffered.
- After `config save` or `config reset` changes Wi-Fi, MQTT, or device id settings, an open MQTT session is closed and Wi-Fi/MQTT reconnect immediately with the new settings.

## Verified Hardware Test

The following have been observed on the physical inverter:

- Successful Modbus RTU communication over RS485.
- Valid responses and CRC.
- Grid voltage readings consistent with the inverter display.
- Battery SOC and charging power readings.
- Grid disconnection at low input voltage and reconnection after voltage recovery.

The 30 m cable worked with the current wiring and Modbus settings.

## USB Troubleshooting in VirtualBox

The board uses the ESP32-S3 native USB Serial/JTAG interface.

VirtualBox may disconnect the USB device after upload or reset. A successful flash may also be followed by an error during the final reset.

If the serial monitor shows no output:

1. Open **VirtualBox → Devices → USB**.
2. Attach **Espressif USB JTAG/serial debug unit** to the VM.
3. Check the current serial port:

   ```bash
   ls -l /dev/ttyACM*
   ```

4. Reopen the serial monitor using the current port.
5. If necessary, press RESET while the monitor is open.

Do not assume the previous port name is still valid. Missing serial output does not necessarily mean the firmware is damaged.

## Beta Limitations

- `pv_power_w`, `grid_power_w`, and `load_power_w` are published **temporarily from unconfirmed registers**: `PV_POWER_VERIFIED`, `GRID_POWER_VERIFIED`, and `LOAD_POWER_VERIFIED` in `include/RegisterMap.h` were set to `true` by decision, not after physical verification. Scale, sign, and meaning may be wrong; do not use these values for automation. Set a flag back to `false` to stop publishing that field.
- The sign convention for `grid_power_w` is not yet defined by the backend.
- `battery_power_w` is published with `+` = discharge, `−` = charge. The sign has only been observed while charging; the discharge sign still requires hardware verification.
- MQTT uses plain TCP without TLS, matching the bundled Mosquitto configuration.
- The configuration stored in NVS, including the Wi-Fi and MQTT passwords, is not encrypted.
- Readings are not buffered while offline.
- Relay control and inverter-setting writes are not implemented.

Before adding relay control, verify the relay GPIO and active level against the board schematic.

## Safety Checklist Before Flashing

1. `include/secrets.h` contains the intended network and broker, and is not staged in Git (`git status` must not list it). A configuration stored in NVS overrides `secrets.h`; check it with `config show` after flashing.
2. `include/RegisterMap.h` gates match the intended state. They are currently `true` for unconfirmed registers (see Beta Limitations); `REGISTER_MAP.md` must say so.
3. The build succeeds without warnings in `src/`.
4. The firmware sends only function `0x03` requests (no write function codes exist in `ModbusClient`).
5. After flashing, `status` (or `telemetry on`) in the serial monitor shows valid verified readings before the board is left unattended.
