#include <Arduino.h>

namespace {
constexpr int RS485_TX_PIN = 17;
constexpr int RS485_RX_PIN = 18;
constexpr int RS485_DE_PIN = 21;

constexpr uint32_t MODBUS_BAUD = 9600;
constexpr uint8_t MODBUS_SLAVE_ID = 1;
constexpr uint16_t TEST_REGISTER = 0;

constexpr uint32_t RESPONSE_TIMEOUT_MS = 500;

uint16_t modbusCrc16(const uint8_t *data, size_t length)
{
	uint16_t crc = 0xFFFF;

	for (size_t i = 0; i < length; ++i) {
		crc ^= data[i];

		for (int bit = 0; bit < 8; ++bit) {
			if (crc & 1) {
				crc = (crc >> 1) ^ 0xA001;
			} else {
				crc >>= 1;
			}
		}
	}

	return crc;
}

void printBytes(const char *label, const uint8_t *data, size_t length)
{
	Serial.print(label);

	for (size_t i = 0; i < length; ++i) {
		Serial.printf(" %02X", data[i]);
	}

	Serial.println();
}

bool readRegister(uint16_t address, uint16_t& value)
{
    uint8_t request[] = {
        MODBUS_SLAVE_ID,
        0x03,
        static_cast<uint8_t>(address >> 8),
        static_cast<uint8_t>(address & 0xFF),
        0x00,
        0x01,
        0x00,
        0x00
    };

    const uint16_t crc = modbusCrc16(request, 6);
    request[6] = static_cast<uint8_t>(crc & 0xFF);
    request[7] = static_cast<uint8_t>(crc >> 8);

    while (Serial1.available() > 0)
    {
        Serial1.read();
    }

    delay(5);

    digitalWrite(RS485_DE_PIN, HIGH);
    delayMicroseconds(50);

    Serial1.write(request, sizeof(request));
    Serial1.flush();

    digitalWrite(RS485_DE_PIN, LOW);

    uint8_t response[16];
    size_t length = 0;
    const uint32_t start = millis();

    while (millis() - start < RESPONSE_TIMEOUT_MS)
    {
        while (Serial1.available() > 0 &&
               length < sizeof(response))
        {
            response[length++] =
                static_cast<uint8_t>(Serial1.read());
        }

        if (length >= 5)
        {
            if (response[1] == 0x83)
            {
                break;
            }

            if (response[1] == 0x03 &&
                length >= 5 + response[2])
            {
                break;
            }
        }

        delay(1);
    }

    if (length < 5 ||
        response[0] != MODBUS_SLAVE_ID)
    {
        return false;
    }

    const uint16_t receivedCrc =
        static_cast<uint16_t>(response[length - 2]) |
        (static_cast<uint16_t>(response[length - 1]) << 8);

    if (receivedCrc != modbusCrc16(response, length - 2))
    {
        return false;
    }

    if (response[1] != 0x03 ||
        response[2] != 2 ||
        length != 7)
    {
        return false;
    }

    value = (static_cast<uint16_t>(response[3]) << 8) |
            response[4];

    return true;
}

} // namespace

void setup()
{
	Serial.begin(115200);

	const unsigned long start = millis();
	while (!Serial && millis() - start < 10000) {
		delay(100);
	}

	pinMode(RS485_DE_PIN, OUTPUT);
	digitalWrite(RS485_DE_PIN, LOW);

	Serial1.begin(MODBUS_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

	Serial.println();
	Serial.println("HomePowerGuard Modbus RTU test");
	Serial.println("Slave: 1, function: 03, register: 0");
	Serial.println("Read-only test; relay control is not used.");
}

void loop()
{
	uint16_t gridVoltage = 0;
	uint16_t gridStatus = 0;
	uint16_t batterySoc = 0;
	uint16_t batteryPowerRaw = 0;

	const bool voltageOk = readRegister(150, gridVoltage);
	delay(100);

	const bool gridOk = readRegister(194, gridStatus);
	delay(100);

	const bool socOk = readRegister(184, batterySoc);
	delay(100);

	const bool powerOk = readRegister(190, batteryPowerRaw);

	Serial.println("--- Deye telemetry ---");

	if (gridOk) {
		Serial.printf("Grid: %s\n", (gridStatus & 1) ? "ON" : "OFF");
	} else {
		Serial.println("Grid: read error");
	}

	if (voltageOk) {
		Serial.printf("Grid voltage: %.1f V\n", gridVoltage * 0.1);
	}

	if (socOk) {
		Serial.printf("Battery SOC: %u%%\n", batterySoc);
	}

	if (powerOk) {
		const int16_t batteryPower = static_cast<int16_t>(batteryPowerRaw);

		if (batteryPower > 50) {
			Serial.printf("Battery: discharging %d W\n", batteryPower);
		} else if (batteryPower < -50) {
			Serial.printf("Battery: charging %d W\n", -static_cast<int>(batteryPower));
		} else {
			Serial.println("Battery: idle");
		}
	}

	Serial.println();
	delay(3000);
}
