#pragma once

#include <chrono>

struct Measurement
{
	std::chrono::system_clock::time_point measuredAt;
    double pvPower;
    double gridPower;
    double batteryPower;
    double loadPower;
    double batterySoc;
};
