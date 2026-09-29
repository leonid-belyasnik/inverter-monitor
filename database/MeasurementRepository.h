#pragma once

#include <cstdint>

#include <domain/Measurement.h>

namespace database {

class PostgresConnection;

class MeasurementRepository
{
public:
    explicit MeasurementRepository(PostgresConnection& connection);

    std::int64_t save(const domain::Measurement& measurement);

private:
    PostgresConnection& m_connection;
};

} // namespace database
