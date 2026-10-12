# ESP32-S3 Relay Firmware

Firmware for the Waveshare ESP32-S3-Relay-1CH board, part of HomePowerGuard.

The current firmware reads telemetry from a Deye hybrid inverter over isolated RS485 using Modbus RTU. Relay control is not implemented.

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

## Build

Run from the repository root:

```bash
pio run -d firmware/esp32-relay
```

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

The firmware periodically prints grid status, grid voltage, battery state of charge, and battery power.

Example:

```text
--- Deye telemetry ---
Grid: ON
Grid voltage: 217.5 V
Battery SOC: 98%
Battery: charging 203 W
```

Actual values depend on the inverter's operating conditions.

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

The firmware uses Modbus RTU function `0x03` (Read Holding Registers).

| Register | Measurement | Conversion |
|---|---|---|
| `150` | Grid voltage | Raw value × 0.1 V |
| `184` | Battery state of charge | Raw value in % |
| `190` | Battery power | Interpreted as signed 16-bit watts |
| `194` | Grid relay status | Bit 0: `1` = ON, `0` = OFF |

The firmware reads registers individually. It validates the Modbus response CRC and checks the response format.

Battery power is currently interpreted as:
- Positive: discharging
- Negative: charging
- Between -50 W and +50 W: idle

Verify the register scaling and power sign against the inverter display under different operating conditions before relying on these values for automation.

`Grid: ON` represents the inverter's grid-relay status. It does not independently guarantee that usable grid voltage is present.

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

## Current Status

- Modbus RTU communication with the inverter works.
- Grid voltage, grid-relay status, battery SOC, and battery power are read periodically.
- The physical RS485 link has been tested using a 30 m cable.
- Relay control has not been implemented.
- Low-grid-voltage warnings and integration with the HomePowerGuard backend are not implemented in this firmware yet.

Before adding relay control, verify the relay GPIO and active level against the board schematic.
