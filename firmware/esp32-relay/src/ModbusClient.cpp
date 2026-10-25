/******************************************************************************
**
** File      ModbusClient.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "ModbusClient.h"

#include "Config.h"

namespace modbus {

namespace {

uint16_t crc16(const uint8_t *data, size_t length)
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

} // namespace

const char *toString(ReadStatus status)
{
	switch (status) {
		case ReadStatus::Ok:
			return "ok";
		case ReadStatus::Timeout:
			return "timeout";
		case ReadStatus::FrameError:
			return "frame_error";
		case ReadStatus::CrcError:
			return "crc_error";
		case ReadStatus::ExceptionResponse:
			return "modbus_exception";
	}

	return "unknown";
}

ModbusClient::ModbusClient(HardwareSerial &serial, uint8_t slaveId, uint8_t dePin)
    : m_serial(serial), m_slaveId(slaveId), m_dePin(dePin)
{
}

void ModbusClient::begin(uint32_t baud, int rxPin, int txPin)
{
	pinMode(m_dePin, OUTPUT);
	digitalWrite(m_dePin, LOW);

	m_serial.begin(baud, SERIAL_8N1, rxPin, txPin);
	m_baud = baud;
}

void ModbusClient::setIdleCallback(void (*callback)())
{
	m_idleCallback = callback;
}

void ModbusClient::setSlaveId(uint8_t slaveId)
{
	m_slaveId = slaveId;
}

void ModbusClient::setBaudRate(uint32_t baud)
{
	m_serial.updateBaudRate(baud);
	m_baud = baud;
}

uint8_t ModbusClient::slaveId() const
{
	return m_slaveId;
}

uint32_t ModbusClient::baudRate() const
{
	return m_baud;
}

ReadStatus ModbusClient::lastStatus() const
{
	return m_lastStatus;
}

bool ModbusClient::hasValidResponse() const
{
	return m_hasValidResponse;
}

uint32_t ModbusClient::lastValidResponseMs() const
{
	return m_lastValidResponseMs;
}

uint32_t ModbusClient::validResponseCount() const
{
	return m_validResponseCount;
}

uint32_t ModbusClient::failedRequestCount() const
{
	return m_failedRequestCount;
}

ReadResult ModbusClient::readHoldingRegister(uint16_t address)
{
	const ReadResult result = readOnce(address);

	m_lastStatus = result.status;
	if (result.status == ReadStatus::Ok || result.status == ReadStatus::ExceptionResponse) {
		m_hasValidResponse = true;
		m_lastValidResponseMs = millis();
		++m_validResponseCount;
	} else {
		++m_failedRequestCount;
	}

	return result;
}

ReadResult ModbusClient::readOnce(uint16_t address)
{
	ReadResult result;

	uint8_t request[8] = {m_slaveId,
	                      0x03,
	                      static_cast<uint8_t>(address >> 8),
	                      static_cast<uint8_t>(address & 0xFF),
	                      0x00,
	                      0x01,
	                      0x00,
	                      0x00};

	const uint16_t requestCrc = crc16(request, 6);
	request[6] = static_cast<uint8_t>(requestCrc & 0xFF);
	request[7] = static_cast<uint8_t>(requestCrc >> 8);

	while (m_serial.available() > 0) {
		m_serial.read();
	}

	delay(5);

	digitalWrite(m_dePin, HIGH);
	delayMicroseconds(50);

	m_serial.write(request, sizeof(request));
	m_serial.flush();

	digitalWrite(m_dePin, LOW);

	uint8_t response[16];
	size_t length = 0;
	const uint32_t start = millis();

	while (millis() - start < config::MODBUS_RESPONSE_TIMEOUT_MS) {
		while (m_serial.available() > 0 && length < sizeof(response)) {
			response[length++] = static_cast<uint8_t>(m_serial.read());
		}

		if (length >= 5) {
			if (response[1] == 0x83) {
				break;
			}

			if (response[1] == 0x03 && length >= 5 + response[2]) {
				break;
			}
		}

		if (m_idleCallback != nullptr) {
			m_idleCallback();
		}

		delay(1);
	}

	if (length < 5 || response[0] != m_slaveId) {
		result.status = ReadStatus::Timeout;
		return result;
	}

	const uint16_t receivedCrc = static_cast<uint16_t>(response[length - 2]) |
	                             (static_cast<uint16_t>(response[length - 1]) << 8);

	if (receivedCrc != crc16(response, length - 2)) {
		result.status = ReadStatus::CrcError;
		return result;
	}

	if (response[1] == 0x83) {
		if (length != 5) {
			result.status = ReadStatus::FrameError;
			return result;
		}

		result.status = ReadStatus::ExceptionResponse;
		result.exceptionCode = response[2];
		return result;
	}

	if (response[1] != 0x03 || response[2] != 2 || length != 7) {
		result.status = ReadStatus::FrameError;
		return result;
	}

	result.status = ReadStatus::Ok;
	result.value = (static_cast<uint16_t>(response[3]) << 8) | response[4];
	return result;
}

} // namespace modbus
