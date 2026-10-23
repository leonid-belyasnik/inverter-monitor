/******************************************************************************
**
** File      ModbusClient.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <Arduino.h>

namespace modbus {

enum class ReadStatus
{
	Ok,
	Timeout,           // no (complete) response within the timeout window
	FrameError,        // malformed / unexpected frame shape
	CrcError,          // CRC mismatch
	ExceptionResponse, // valid Modbus exception reply (function | 0x80)
};

struct ReadResult
{
	ReadStatus status = ReadStatus::Timeout;
	uint16_t value = 0;        // valid only when status == Ok
	uint8_t exceptionCode = 0; // valid only when status == ExceptionResponse
};

const char *toString(ReadStatus status);

// Minimal Modbus RTU master for function 0x03 (Read Holding Registers) over
// an RS485 transceiver with a direction-enable (DE) pin. Read-only: no write
// function codes are implemented, by design.
class ModbusClient
{
  public:
	ModbusClient(HardwareSerial &serial, uint8_t slaveId, uint8_t dePin);

	// Configures the DE pin and starts the UART. Call once from setup().
	void begin(uint32_t baud, int rxPin, int txPin);

	// Reads a single holding register. A failed read never returns a stale
	// or guessed value: callers must check result.status before using
	// result.value.
	ReadResult readHoldingRegister(uint16_t address);

  private:
	HardwareSerial &m_serial;
	uint8_t m_slaveId;
	uint8_t m_dePin;
};

} // namespace modbus
