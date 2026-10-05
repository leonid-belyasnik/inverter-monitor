#include "TelemetryIngestionService.h"

#include <exception>
#include <utility>

#include <database/MeasurementRepository.h>

namespace application {

TelemetryIngestionService::TelemetryIngestionService(
    database::MeasurementRepository& repository,
    MeasurementHandler handler)
    : m_repository(repository)
    , m_handler(std::move(handler))
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

        if (m_handler) {
            m_handler(measurement);
        }

        return id;
    }
    catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace application
