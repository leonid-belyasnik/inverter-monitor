#pragma once

#include <cstdint>
#include <optional>
#include <functional>

#include <application/IngestedMeasurement.h>

namespace database {
class MeasurementRepository;
}

namespace application {

class TelemetryIngestionService
{
public:
    using MeasurementHandler =
    std::function<void(const IngestedMeasurement&)>;

    explicit TelemetryIngestionService(
    database::MeasurementRepository& repository,
    MeasurementHandler handler);

    std::optional<std::int64_t> ingest(
        const IngestedMeasurement& measurement) const;

private:
    database::MeasurementRepository& m_repository;
    MeasurementHandler m_handler;
};

} // namespace application
