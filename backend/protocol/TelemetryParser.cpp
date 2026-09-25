#include "TelemetryParser.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <boost/json.hpp>

namespace {

std::optional<std::chrono::system_clock::time_point>
parseTimestamp(const std::string_view value)
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

    int milliseconds = 0;
    stream >> milliseconds;

    if (stream.fail() || stream.peek() != 'Z') {
        return std::nullopt;
    }

    const std::time_t seconds = timegm(&tm);

    if (seconds == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }

    return std::chrono::system_clock::from_time_t(seconds)
        + std::chrono::milliseconds(milliseconds);
}

} // namespace

std::optional<Measurement> TelemetryParser::parse(
    const std::string_view payload) const
{
    try {
        const auto root = boost::json::parse(payload);

        if (!root.is_object()) {
            return std::nullopt;
        }

        const auto& object = root.as_object();

        const auto& measurement =
            object.at("measurement").as_object();

        Measurement result;

        result.pvPower =
            measurement.at("pv_power_w").to_number<double>();

        result.gridPower =
            measurement.at("grid_power_w").to_number<double>();

        result.batteryPower =
            measurement.at("battery_power_w").to_number<double>();

        result.loadPower =
            measurement.at("load_power_w").to_number<double>();

        result.batterySoc =
            measurement.at("battery_soc_pct").to_number<double>();

        const auto measuredAt =
            parseTimestamp(object.at("measured_at").as_string());

        if (!measuredAt) {
            return std::nullopt;
        }

        result.measuredAt = *measuredAt;

        return result;
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}