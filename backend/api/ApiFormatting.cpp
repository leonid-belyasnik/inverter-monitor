/******************************************************************************
**
** File      ApiFormatting.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "ApiFormatting.h"

#include <iomanip>
#include <optional>
#include <sstream>

#include <boost/json/value.hpp>

namespace api {

namespace {

boost::json::value optionalValue(const std::optional<double> &value)
{
	if (!value) {
		return nullptr;
	}

	return *value;
}

boost::json::value optionalValue(const std::optional<bool> &value)
{
	if (!value) {
		return nullptr;
	}

	return *value;
}

} // namespace

std::string formatTimestamp(const std::chrono::system_clock::time_point &timePoint)
{
	const auto time = std::chrono::system_clock::to_time_t(timePoint);

	std::tm tm{};
	gmtime_r(&time, &tm);

	const auto milliseconds =
	    std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()) %
	    std::chrono::seconds(1);

	std::ostringstream stream;

	stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
	       << milliseconds.count() << 'Z';

	return stream.str();
}

boost::json::object formatMeasurement(const domain::Measurement &measurement)
{
	boost::json::object object;

	object["measured_at"] = formatTimestamp(measurement.measuredAt);
	object["pv_power"] = optionalValue(measurement.pvPower);
	object["grid_power"] = optionalValue(measurement.gridPower);
	object["battery_power"] = measurement.batteryPower;
	object["load_power"] = optionalValue(measurement.loadPower);
	object["battery_soc"] = measurement.batterySoc;
	object["grid_voltage"] = optionalValue(measurement.gridVoltage);
	object["grid_relay_on"] = optionalValue(measurement.gridRelayOn);

	return object;
}

} // namespace api
