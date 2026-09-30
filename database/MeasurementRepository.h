#pragma once

#include <cstdint>
#include <vector>

#include <database/MeasurementRecord.h>

#include <domain/DeviceId.h>
#include <domain/Measurement.h>

namespace database {

class PostgresConnection;

class MeasurementRepository
{
public:
    explicit MeasurementRepository(PostgresConnection& connection);

    std::int64_t save(
        const domain::DeviceId& deviceId,
        const domain::Measurement& measurement);

    std::vector<MeasurementRecord> findByDevice(
        const domain::DeviceId& deviceId,
        std::size_t limit);

private:
    PostgresConnection& m_connection;
};

} // namespace database
