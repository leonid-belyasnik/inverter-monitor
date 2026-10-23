#pragma once

#include <cstdint>
#include <optional>
#include <functional>
#include <string>

#include <application/IngestedMeasurement.h>

namespace database {
class MeasurementRepository;
}

namespace application {

struct IngestionError
{
    enum class Stage
    {
        Persistence,
        Handler
    };

    Stage stage = Stage::Persistence;
    std::string message;
    // Empty when the server did not report a SQLSTATE.
    std::string sqlState;
};

class TelemetryIngestionService
{
public:
    using MeasurementHandler =
    std::function<void(const IngestedMeasurement&)>;

    explicit TelemetryIngestionService(
    database::MeasurementRepository& repository,
    MeasurementHandler handler);

    // Returns the measurement id once it is persisted, even if the handler
    // fails afterwards. Failures are described in error when it is not null.
    std::optional<std::int64_t> ingest(
        const IngestedMeasurement& measurement,
        IngestionError* error = nullptr) const;

private:
    database::MeasurementRepository& m_repository;
    MeasurementHandler m_handler;
};

} // namespace application
