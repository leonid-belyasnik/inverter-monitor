/******************************************************************************
**
** File      TimeSync.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "TimeSync.h"

#include <sys/time.h>
#include <time.h>

#include "Config.h"

namespace timesync {

void begin()
{
	// UTC offset/DST both zero: the firmware always works in UTC, matching
	// the backend's expected measured_at timestamp format.
	configTime(0, 0, config::NTP_SERVER_PRIMARY, config::NTP_SERVER_SECONDARY);
}

bool isSynced()
{
	return time(nullptr) >= config::MIN_VALID_EPOCH_SECONDS;
}

String nowIso8601Utc()
{
	if (!isSynced()) {
		return String();
	}

	struct timeval tv;
	gettimeofday(&tv, nullptr);

	struct tm tmValue;
	gmtime_r(&tv.tv_sec, &tmValue);

	char buffer[32];
	strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tmValue);

	const int milliseconds = static_cast<int>(tv.tv_usec / 1000);

	char result[40];
	snprintf(result, sizeof(result), "%s.%03dZ", buffer, milliseconds);

	return String(result);
}

} // namespace timesync
