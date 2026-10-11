# ESP32-S3 Relay Firmware

Firmware for the Waveshare ESP32-S3-Relay-1CH board, part of HomePowerGuard.

## Development Environment

- PlatformIO Core
- Arduino framework
- PlatformIO board: `esp32-s3-devkitc-1`
- Serial speed: `115200`
- Flash size: 16 MB
- PSRAM: 8 MB

The PlatformIO board definition is generic. The actual hardware is Waveshare ESP32-S3-Relay-1CH.

## Build

Run from the repository root:

```bash
pio run -d firmware/esp32-relay
```

## Upload

Use the current serial port:

```bash
pio run -d firmware/esp32-relay -t upload --upload-port /dev/ttyACM0
```

The port may differ after reconnecting the board.

## Verify Firmware Output

Open the serial monitor at 115200 baud:

```bash
pio device monitor --port /dev/ttyACM0 --baud 115200
```

The current test firmware prints a startup message and `alive` approximately every two seconds.

Exit the monitor with `Ctrl+C`.

## USB Troubleshooting in VirtualBox

The board uses the ESP32-S3 native USB Serial/JTAG interface.

After uploading firmware or pressing RESET, VirtualBox may disconnect the USB device. The firmware upload can succeed even if PlatformIO reports an error during the final reset.

If the serial monitor shows no output:

1. Open **VirtualBox → Devices → USB**.
2. Check whether **Espressif USB JTAG/serial debug unit** is attached.
3. If necessary, select the device to reconnect it to the virtual machine.
4. Check the current serial port:

   ```bash
   ls -l /dev/ttyACM*
   ```

5. Open the serial monitor using the current port.

The port name can change after reconnecting, for example from `/dev/ttyACM0` to `/dev/ttyACM1`.

Do not assume the previous port name is still valid. If the monitor was opened before reconnecting the board, exit it with `Ctrl+C` and start it again.

Missing serial output does not necessarily mean the firmware is damaged. Check USB passthrough and the current port before reflashing.

## Current Status

- Firmware builds successfully.
- Firmware has been uploaded and its flash hash verified.
- The `alive` message has been observed repeatedly in the serial monitor.
- USB serial output works after reconnecting the device in VirtualBox.
- Relay control has not yet been implemented.

Before adding relay control, verify the relay GPIO and active level against the board schematic.
