#include "Migrations.h"

namespace database {

const std::vector<Migration> &allMigrations()
{
	static const std::vector<Migration> migrations{
	    {1, "create_measurements",
	     R"sql(
CREATE TABLE IF NOT EXISTS measurements (
    id              bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    measured_at     timestamptz NOT NULL,
    pv_power        double precision,
    grid_power      double precision,
    battery_power   double precision,
    load_power      double precision,
    battery_soc     double precision
)
)sql"},
	    {2, "add_measurements_device_id",
	     R"sql(
ALTER TABLE measurements
    ADD COLUMN IF NOT EXISTS device_id text NOT NULL DEFAULT 'unknown';
ALTER TABLE measurements ALTER COLUMN device_id DROP DEFAULT
)sql"},
	    {3, "add_measurements_grid_voltage",
	     R"sql(
ALTER TABLE measurements
    ADD COLUMN IF NOT EXISTS grid_voltage double precision
)sql"},
	    {4, "add_measurements_grid_relay_on",
	     R"sql(
ALTER TABLE measurements
    ADD COLUMN grid_relay_on boolean
)sql"},
	};

	return migrations;
}

} // namespace database
