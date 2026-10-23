#include "TelemetryIngestionService.h"

#include <exception>
#include <string>
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
    const IngestedMeasurement& measurement,
    IngestionError* error) const
{
    const auto report = [error](
        const IngestionError::Stage stage,
        std::string message,
        std::string sqlState = {})
    {
        if (error) {
            *error = IngestionError{stage, std::move(message), std::move(sqlState)};
        }
    };

    std::int64_t id = 0;

    try {
        database::DatabaseError databaseError;

        id = m_repository.save(
            measurement.deviceId,
            measurement.measurement,
            &databaseError);

        if (id == 0) {
            report(
                IngestionError::Stage::Persistence,
                std::move(databaseError.message),
                std::move(databaseError.sqlState));
            return std::nullopt;
        }
    }
    catch (const std::exception& exception) {
        report(IngestionError::Stage::Persistence, exception.what());
        return std::nullopt;
    }

    if (m_handler) {
        try {
            m_handler(measurement);
        }
        catch (const std::exception& exception) {
            report(IngestionError::Stage::Handler, exception.what());
        }
    }

    return id;
}

} // namespace application
