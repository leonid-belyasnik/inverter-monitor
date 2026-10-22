#pragma once

#include <chrono>
#include <optional>

namespace domain {

// pvPower, gridPower, and loadPower are empty when the source did not
// provide a value; they must never be substituted with zero.
struct Measurement
{
	std::chrono::system_clock::time_point measuredAt;
    std::optional<double> pvPower;
    std::optional<double> gridPower;
    double batteryPower;
    std::optional<double> loadPower;
    double batterySoc;
};

} // namespace domain
