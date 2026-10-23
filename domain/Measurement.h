/******************************************************************************
**
** File      Measurement.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <chrono>
#include <optional>

namespace domain {

// pvPower, gridPower, loadPower, gridVoltage, and gridRelayOn are empty when the
// source did not provide a value; they must never be substituted with zero or false.
// gridRelayOn is the inverter's grid relay state, not proof of grid presence.
struct Measurement
{
	std::chrono::system_clock::time_point measuredAt;
	std::optional<double> pvPower;
	std::optional<double> gridPower;
	double batteryPower;
	std::optional<double> loadPower;
	double batterySoc;
	std::optional<double> gridVoltage;
	std::optional<bool> gridRelayOn;
};

} // namespace domain
