#include "DatabaseSchema.h"

#include "PostgresConnection.h"
#include "PostgresResult.h"

namespace database {

DatabaseSchema::DatabaseSchema(PostgresConnection& connection)
    : m_connection(connection)
{
}

bool DatabaseSchema::initialize()
{
    const auto createResult = m_connection.execute(
        "CREATE TABLE IF NOT EXISTS measurements ("
        "    id              bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,"
        "    device_id       text NOT NULL,"
        "    measured_at     timestamptz NOT NULL,"
        "    pv_power        double precision,"
        "    grid_power      double precision,"
        "    battery_power   double precision,"
        "    load_power      double precision,"
        "    battery_soc     double precision"
        ")"
    );

    if (!createResult.isOk()) {
        return false;
    }

    const auto migrationResult = m_connection.execute(
        "ALTER TABLE measurements "
        "ADD COLUMN IF NOT EXISTS device_id text NOT NULL DEFAULT 'unknown'"
    );

    if (!migrationResult.isOk()) {
        return false;
    }

    const auto defaultResult = m_connection.execute(
        "ALTER TABLE measurements ALTER COLUMN device_id DROP DEFAULT"
    );

    return defaultResult.isOk();
}

} // namespace database
