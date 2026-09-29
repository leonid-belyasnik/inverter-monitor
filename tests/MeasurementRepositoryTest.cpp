#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include <application/TelemetryIngestionService.h>
#include <database/DatabaseSchema.h>
#include <database/MeasurementRepository.h>
#include <database/PostgresConnection.h>
#include <database/PostgresResult.h>
#include <mqtt/TelemetryAdapter.h>
#include <protocol/TelemetryParser.h>

TEST(MeasurementRepositoryTest, IngestsAndPersistsMeasurementWithDeviceId)
{
    database::PostgresConnection connection("");

    ASSERT_TRUE(connection.isOk())
        << "Connection failed: "
        << connection.errorMessage();

    database::DatabaseSchema schema(connection);

    ASSERT_TRUE(schema.initialize())
        << "Schema initialization failed";

    const auto beginResult = connection.execute("BEGIN");

    ASSERT_TRUE(beginResult.isOk())
        << "Failed to begin transaction";

    database::MeasurementRepository repository(connection);
    application::TelemetryIngestionService ingestion(repository);
    const protocol::TelemetryParser parser;
    const mqtt::TelemetryAdapter adapter(parser);

    const std::string payload = R"json(
{
    "measured_at": "2026-09-24T18:50:00.316Z",
    "measurement": {
        "pv_power_w": 1234.0,
        "grid_power_w": -100.0,
        "battery_power_w": 500.0,
        "load_power_w": 1634.0,
        "battery_soc_pct": 75.0
    }
}
)json";

    const auto ingested = adapter.adapt(
        "v1/devices/deye-001/telemetry",
        payload);

    ASSERT_TRUE(ingested.has_value());

    const auto measurementId = ingestion.ingest(*ingested);

    ASSERT_TRUE(measurementId.has_value())
        << "Failed to ingest measurement";

    const std::string query =
        "SELECT device_id, pv_power, grid_power, battery_power, load_power, battery_soc "
        "FROM measurements "
        "WHERE id = " + std::to_string(*measurementId);

    const auto result = connection.execute(query.c_str());

    ASSERT_TRUE(result.isOk())
        << "Failed to read measurement";

    ASSERT_EQ(result.rowCount(), 1)
        << "Expected exactly one measurement";

    EXPECT_STREQ(result.value(0, 0), "deye-001");
    EXPECT_DOUBLE_EQ(std::stod(result.value(0, 1)), 1234.0);
    EXPECT_DOUBLE_EQ(std::stod(result.value(0, 2)), -100.0);
    EXPECT_DOUBLE_EQ(std::stod(result.value(0, 3)), 500.0);
    EXPECT_DOUBLE_EQ(std::stod(result.value(0, 4)), 1634.0);
    EXPECT_DOUBLE_EQ(std::stod(result.value(0, 5)), 75.0);

    const auto rollbackResult = connection.execute("ROLLBACK");

    ASSERT_TRUE(rollbackResult.isOk())
        << "Failed to rollback transaction";
}
