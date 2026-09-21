#include "DatabaseSchema.h"

#include "PostgresConnection.h"
#include "PostgresResult.h"

DatabaseSchema::DatabaseSchema(PostgresConnection& connection)
    : m_connection(connection)
{
}

bool DatabaseSchema::initialize()
{
    const auto result = m_connection.execute(
        "CREATE TABLE IF NOT EXISTS measurements ("
        "    id              bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,"
        "    measured_at     timestamptz NOT NULL,"
        "    pv_power        double precision,"
        "    grid_power      double precision,"
        "    battery_power   double precision,"
        "    load_power      double precision,"
        "    battery_soc     double precision"
        ")"
    );

    return result.isOk();
}
