#pragma once

#include <cstdint>
#include <optional>

#include <application/IngestedMeasurement.h>

namespace database {
class MeasurementRepository;
}

namespace application {

class TelemetryIngestionService
{
public:
    explicit TelemetryIngestionService(
        database::MeasurementRepository& repository);

    std::optional<std::int64_t> ingest(
        const IngestedMeasurement& measurement) const;

private:
    database::MeasurementRepository& m_repository;
};

} // namespace application
