#pragma once

#include <cstdint>

#include <domain/Measurement.h>

class PostgresConnection;

class MeasurementRepository
{
public:
    explicit MeasurementRepository(PostgresConnection& connection);

    std::int64_t save(const Measurement& measurement);

private:
    PostgresConnection& m_connection;
};
