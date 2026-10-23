#include <database/MeasurementRepository.h>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <optional>

#include <database/PostgresConnection.h>
#include <database/PostgresResult.h>

namespace database {

namespace
{

std::string formatTimestamp(
    const std::chrono::system_clock::time_point time)
{
    const auto seconds =
        std::chrono::time_point_cast<std::chrono::seconds>(time);

    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            time - seconds).count();

    const auto timeValue =
        std::chrono::system_clock::to_time_t(seconds);

    std::tm tm{};
    gmtime_r(&timeValue, &tm);

    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
           << '.'
           << std::setfill('0')
           << std::setw(3)
           << milliseconds
           << 'Z';

    return stream.str();
}

std::string formatDouble(const double value)
{
    std::ostringstream stream;
    stream << std::setprecision(17) << value;

    return stream.str();
}

std::optional<std::string> formatOptionalDouble(const std::optional<double>& value)
{
    if (!value) {
        return std::nullopt;
    }

    return formatDouble(*value);
}

// A null pointer is passed to libpq as SQL NULL.
const char* parameterValue(const std::optional<std::string>& value)
{
    return value ? value->c_str() : nullptr;
}

std::optional<double> optionalDouble(
    const PostgresResult& result,
    const int row,
    const int column)
{
    if (result.isNull(row, column)) {
        return std::nullopt;
    }

    return std::stod(result.value(row, column));
}

std::optional<std::chrono::system_clock::time_point>
parseTimestamp(const char* value)
{
    std::tm tm{};
    int milliseconds = 0;

    std::istringstream stream{value};

    stream >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    if (stream.fail()) {
        return std::nullopt;
    }

    if (stream.peek() == '.') {
        stream.get();

        std::string fractional;
        stream >> fractional;

        if (fractional.empty()) {
            return std::nullopt;
        }

        const auto plusPosition = fractional.find('+');

        if (plusPosition != std::string::npos) {
            fractional = fractional.substr(0, plusPosition);
        }

        if (fractional.empty() || fractional.size() > 3) {
            return std::nullopt;
        }

        while (fractional.size() < 3) {
            fractional += '0';
        }

        milliseconds = std::stoi(fractional);
    }

    const std::time_t seconds = timegm(&tm);

    if (seconds == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }

    return std::chrono::system_clock::from_time_t(seconds)
        + std::chrono::milliseconds(milliseconds);
}

}

MeasurementRepository::MeasurementRepository(PostgresConnection& connection)
    : m_connection(connection)
{
}

std::int64_t MeasurementRepository::save(
    const domain::DeviceId& deviceId,
    const domain::Measurement& measurement)
{
    const std::string measuredAt = formatTimestamp(measurement.measuredAt);
    const auto pvPower = formatOptionalDouble(measurement.pvPower);
    const auto gridPower = formatOptionalDouble(measurement.gridPower);
    const std::string batteryPower = formatDouble(measurement.batteryPower);
    const auto loadPower = formatOptionalDouble(measurement.loadPower);
    const std::string batterySoc = formatDouble(measurement.batterySoc);
    const auto gridVoltage = formatOptionalDouble(measurement.gridVoltage);

    const char* values[] = {
        deviceId.value.c_str(),
        measuredAt.c_str(),
        parameterValue(pvPower),
        parameterValue(gridPower),
        batteryPower.c_str(),
        parameterValue(loadPower),
        batterySoc.c_str(),
        parameterValue(gridVoltage)
    };

    const auto result = m_connection.executeParams(
        "INSERT INTO measurements "
        "(device_id, measured_at, pv_power, grid_power, battery_power, load_power, battery_soc, grid_voltage) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8) "
        "RETURNING id",
        8,
        values
    );

    if (!result.isOk() || result.rowCount() != 1)
    {
        return 0;
    }

    return std::stoll(result.value(0, 0));
}

std::vector<MeasurementRecord> MeasurementRepository::findByDevice(
    const domain::DeviceId& deviceId,
    const std::size_t limit)
{
    const std::string limitValue = std::to_string(limit);

    const char* values[] = {
        deviceId.value.c_str(),
        limitValue.c_str()
    };

    const auto result = m_connection.executeParams(
        "SELECT device_id, measured_at, pv_power, grid_power, "
        "battery_power, load_power, battery_soc, grid_voltage "
        "FROM measurements "
        "WHERE device_id = $1 "
        "ORDER BY measured_at DESC "
        "LIMIT $2",
        2,
        values
    );

    std::vector<MeasurementRecord> records;

    if (!result.isOk()) {
        return records;
    }

    for (int row = 0; row < result.rowCount(); ++row) {
        MeasurementRecord record;

        record.deviceId = domain::DeviceId{
            result.value(row, 0)};

        const auto measuredAt = parseTimestamp(result.value(row, 1));

        if (!measuredAt) {
            continue;
        }

        record.measurement.measuredAt = *measuredAt;
        record.measurement.pvPower =
            optionalDouble(result, row, 2);
        record.measurement.gridPower =
            optionalDouble(result, row, 3);
        record.measurement.batteryPower =
            std::stod(result.value(row, 4));
        record.measurement.loadPower =
            optionalDouble(result, row, 5);
        record.measurement.batterySoc =
            std::stod(result.value(row, 6));
        record.measurement.gridVoltage =
            optionalDouble(result, row, 7);

        records.push_back(std::move(record));
    }

    return records;
}

} // namespace database
