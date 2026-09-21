#pragma once

#include <domain/Measurement.h>

class PostgresConnection;

class MeasurementRepository
{
public:
    explicit MeasurementRepository(PostgresConnection& connection);

    bool save(const Measurement& measurement);

private:
    PostgresConnection& m_connection;
};
