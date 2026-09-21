#include <database/MeasurementRepository.h>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include <database/PostgresConnection.h>
#include <database/PostgresResult.h>

namespace
{

std::string formatTimestamp(const std::chrono::system_clock::time_point time)
{
    const auto timeValue = std::chrono::system_clock::to_time_t(time);

    std::tm tm{};
    gmtime_r(&timeValue, &tm);

    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%d %H:%M:%SZ");

    return stream.str();
}

std::string formatDouble(const double value)
{
    std::ostringstream stream;
    stream << std::setprecision(17) << value;

    return stream.str();
}

}

MeasurementRepository::MeasurementRepository(PostgresConnection& connection)
    : m_connection(connection)
{
}

std::int64_t MeasurementRepository::save(const Measurement& measurement)
{
    const std::string measuredAt = formatTimestamp(measurement.measuredAt);
    const std::string pvPower = formatDouble(measurement.pvPower);
    const std::string gridPower = formatDouble(measurement.gridPower);
    const std::string batteryPower = formatDouble(measurement.batteryPower);
    const std::string loadPower = formatDouble(measurement.loadPower);
    const std::string batterySoc = formatDouble(measurement.batterySoc);

    const char* values[] = {
        measuredAt.c_str(),
        pvPower.c_str(),
        gridPower.c_str(),
        batteryPower.c_str(),
        loadPower.c_str(),
        batterySoc.c_str()
    };

    const auto result = m_connection.executeParams(
        "INSERT INTO measurements "
        "(measured_at, pv_power, grid_power, battery_power, load_power, battery_soc) "
        "VALUES ($1, $2, $3, $4, $5, $6) "
        "RETURNING id",
        6,
        values
    );

    if (!result.isOk() || result.rowCount() != 1)
    {
        return 0;
    }

    return std::stoll(result.value(0, 0));
}
