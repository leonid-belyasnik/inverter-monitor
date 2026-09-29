#pragma once

#include <chrono>

namespace domain {

struct Measurement
{
	std::chrono::system_clock::time_point measuredAt;
    double pvPower;
    double gridPower;
    double batteryPower;
    double loadPower;
    double batterySoc;
};

} // namespace domain
