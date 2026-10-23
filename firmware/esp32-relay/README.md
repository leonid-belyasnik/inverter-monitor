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
- `DEVICE_ID` — used in the topic and as the MQTT client id. It must not contain `/`, `+`, or `#`.

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

Every telemetry cycle prints the verified readings and the publish result. Example:

```text
--- Deye telemetry ---
Grid voltage: 217.5 V
Grid relay: ON
Battery SOC: 98%
Battery power: -203 W (+discharge / -charge)
Published seq=1: {"schema_version":1,"seq":1,"measured_at":"2026-10-22T09:15:00.120Z","measurement":{"battery_power_w":-203,"battery_soc_pct":98}}
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

With `DIAGNOSTIC_MODE_ENABLED = true`, the firmware scans the candidate registers every 15 s. Each register is read individually; a failure does not stop the scan. Example:

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

`Grid relay: ON` represents the inverter's grid-relay status. It does not independently guarantee that usable grid voltage is present.

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
    "battery_soc_pct": 76
  }
}
```

If a gate is set back to `false`, or its read fails in a cycle, that field is omitted from the payload.

- `battery_soc_pct` and `battery_power_w` are required. If either fails to read, nothing is published for that cycle (`incomplete_reading`).
- `pv_power_w`, `grid_power_w`, and `load_power_w` are optional. Each is included only when its register gate is enabled and the read succeeded in the current cycle; otherwise it is omitted. The backend stores omitted fields as `NULL` and returns them as `null`.
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
- Readings are not buffered while offline.
- Relay control and inverter-setting writes are not implemented.

Before adding relay control, verify the relay GPIO and active level against the board schematic.

## Safety Checklist Before Flashing

1. `include/secrets.h` contains the intended network and broker, and is not staged in Git (`git status` must not list it).
2. `include/RegisterMap.h` gates match the intended state. They are currently `true` for unconfirmed registers (see Beta Limitations); `REGISTER_MAP.md` must say so.
3. The build succeeds without warnings in `src/`.
4. The firmware sends only function `0x03` requests (no write function codes exist in `ModbusClient`).
5. After flashing, the serial monitor shows valid verified readings before the board is left unattended.
