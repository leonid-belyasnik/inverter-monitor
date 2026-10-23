/******************************************************************************
**
** File      TelemetryParser.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "TelemetryParser.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <boost/json.hpp>

namespace {

std::optional<std::chrono::system_clock::time_point> parseTimestamp(const std::string_view value)
{
	std::tm tm{};
	std::istringstream stream{std::string(value)};

	stream >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

	if (stream.fail()) {
		return std::nullopt;
	}

	if (stream.peek() != '.') {
		return std::nullopt;
	}

	stream.get();

	std::string fractional;
	stream >> fractional;

	if (stream.fail() || fractional.empty() || fractional.back() != 'Z') {
		return std::nullopt;
	}

	fractional.pop_back();

	if (fractional.empty() || fractional.size() > 3) {
		return std::nullopt;
	}

	while (fractional.size() < 3) {
		fractional += '0';
	}

	const int milliseconds = std::stoi(fractional);

	const std::time_t seconds = timegm(&tm);

	if (seconds == static_cast<std::time_t>(-1)) {
		return std::nullopt;
	}

	return std::chrono::system_clock::from_time_t(seconds) +
	       std::chrono::milliseconds(milliseconds);
}

// Absent and null fields yield an empty optional. Any other non-numeric
// value throws, so the whole payload is rejected.
std::optional<double> optionalNumber(const boost::json::object &object, const std::string_view key)
{
	const auto *value = object.if_contains(key);

	if (value == nullptr || value->is_null()) {
		return std::nullopt;
	}

	return value->to_number<double>();
}

// Absent and null fields yield an empty optional. Any other non-boolean
// value throws, so the whole payload is rejected.
std::optional<bool> optionalBool(const boost::json::object &object, const std::string_view key)
{
	const auto *value = object.if_contains(key);

	if (value == nullptr || value->is_null()) {
		return std::nullopt;
	}

	return value->as_bool();
}

} // namespace

namespace protocol {

std::optional<domain::Measurement> TelemetryParser::parse(const std::string_view payload) const
{
	try {
		const auto root = boost::json::parse(payload);

		if (!root.is_object()) {
			return std::nullopt;
		}

		const auto &object = root.as_object();

		const auto &measurement = object.at("measurement").as_object();

		domain::Measurement result;

		result.pvPower = optionalNumber(measurement, "pv_power_w");

		result.gridPower = optionalNumber(measurement, "grid_power_w");

		result.batteryPower = measurement.at("battery_power_w").to_number<double>();

		result.loadPower = optionalNumber(measurement, "load_power_w");

		result.batterySoc = measurement.at("battery_soc_pct").to_number<double>();

		result.gridVoltage = optionalNumber(measurement, "grid_voltage_v");

		result.gridRelayOn = optionalBool(measurement, "grid_relay_on");

		const auto measuredAt = parseTimestamp(object.at("measured_at").as_string());

		if (!measuredAt) {
			return std::nullopt;
		}

		result.measuredAt = *measuredAt;

		return result;
	} catch (const std::exception &) {
		return std::nullopt;
	}
}

} // namespace protocol
