#include <application/TelemetryIngestionService.h>

#include <exception>

#include <database/MeasurementRepository.h>

namespace application {

TelemetryIngestionService::TelemetryIngestionService(
    database::MeasurementRepository& repository)
    : m_repository(repository)
{
}

std::optional<std::int64_t> TelemetryIngestionService::ingest(
    const IngestedMeasurement& measurement) const
{
    try {
        const auto id = m_repository.save(
            measurement.deviceId,
            measurement.measurement);

        if (id == 0) {
            return std::nullopt;
        }

        return id;
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace application
