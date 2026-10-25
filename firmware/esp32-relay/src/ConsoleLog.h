/******************************************************************************
**
** File      ConsoleLog.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

namespace consolelog {

// Background (non-command) output to the USB Serial port. While the user is
// typing a command, SerialConsole calls hold(): messages are then kept in a
// small buffer instead of being written into the middle of the typed line,
// and are written by release(). Whole messages only; if the buffer is full,
// further messages are dropped and counted. Single-threaded: call from the
// Arduino loop() task only.
void printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
void println(const char *text);

void hold();
bool holding();

// Stops holding and writes the buffered messages (and a note if some were
// dropped).
void release();

} // namespace consolelog
