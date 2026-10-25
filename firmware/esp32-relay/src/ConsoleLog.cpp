/******************************************************************************
**
** File      ConsoleLog.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "ConsoleLog.h"

#include <Arduino.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace consolelog {

namespace {

// Longest message is the "Published seq=...: {json}" line (about 420 bytes).
constexpr size_t MESSAGE_BUFFER_SIZE = 512;
constexpr size_t DEFERRED_BUFFER_SIZE = 1536;

char s_deferred[DEFERRED_BUFFER_SIZE];
size_t s_deferredLength = 0;
unsigned s_dropped = 0;
bool s_holding = false;

void write(const char *text, size_t length)
{
	if (length == 0) {
		return;
	}

	if (!s_holding) {
		Serial.write(reinterpret_cast<const uint8_t *>(text), length);
		return;
	}

	if (length > DEFERRED_BUFFER_SIZE - s_deferredLength) {
		++s_dropped;
		return;
	}

	memcpy(s_deferred + s_deferredLength, text, length);
	s_deferredLength += length;
}

} // namespace

void printf(const char *format, ...)
{
	char message[MESSAGE_BUFFER_SIZE];

	va_list arguments;
	va_start(arguments, format);
	const int length = vsnprintf(message, sizeof(message), format, arguments);
	va_end(arguments);

	if (length <= 0) {
		return;
	}

	write(message, static_cast<size_t>(length) < sizeof(message) ? static_cast<size_t>(length)
	                                                              : sizeof(message) - 1);
}

void println(const char *text)
{
	printf("%s\n", text);
}

void hold()
{
	s_holding = true;
}

bool holding()
{
	return s_holding;
}

void release()
{
	s_holding = false;

	if (s_deferredLength != 0) {
		const size_t length = s_deferredLength;
		s_deferredLength = 0;
		Serial.write(reinterpret_cast<const uint8_t *>(s_deferred), length);
	}

	if (s_dropped != 0) {
		const unsigned dropped = s_dropped;
		s_dropped = 0;
		printf("[%u message(s) dropped while typing]\n", dropped);
	}
}

} // namespace consolelog
