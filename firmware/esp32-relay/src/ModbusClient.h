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

	// Called repeatedly while a request waits for the response, so the
	// caller can service other input (e.g. the Serial console echo). The
	// callback must not start another Modbus request or change the Modbus
	// configuration.
	void setIdleCallback(void (*callback)());

	// Take effect from the next request. Must not be called while a
	// request is in progress (single-threaded use only).
	void setSlaveId(uint8_t slaveId);
	void setBaudRate(uint32_t baud);

	uint8_t slaveId() const;
	uint32_t baudRate() const;

	// Reads a single holding register. A failed read never returns a stale
	// or guessed value: callers must check result.status before using
	// result.value.
	ReadResult readHoldingRegister(uint16_t address);

	// Diagnostics since boot. A response counts as valid when it passed
	// address/CRC/frame checks, including Modbus exception replies.
	ReadStatus lastStatus() const;
	bool hasValidResponse() const;
	uint32_t lastValidResponseMs() const;
	uint32_t validResponseCount() const;
	uint32_t failedRequestCount() const;

  private:
	ReadResult readOnce(uint16_t address);

	HardwareSerial &m_serial;
	uint8_t m_slaveId;
	uint8_t m_dePin;
	uint32_t m_baud = 0;
	void (*m_idleCallback)() = nullptr;

	ReadStatus m_lastStatus = ReadStatus::Timeout;
	bool m_hasValidResponse = false;
	uint32_t m_lastValidResponseMs = 0;
	uint32_t m_validResponseCount = 0;
	uint32_t m_failedRequestCount = 0;
};

} // namespace modbus
