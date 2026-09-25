#include <chrono>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include <database/DatabaseSchema.h>
#include <database/MeasurementRepository.h>
#include <database/PostgresConnection.h>
#include <database/PostgresResult.h>
#include <domain/Measurement.h>

TEST(MeasurementRepositoryTest, SavesAndReadsMeasurement)
{
    PostgresConnection connection("");

    ASSERT_TRUE(connection.isOk())
        << "Connection failed: "
        << connection.errorMessage();

    DatabaseSchema schema(connection);

    ASSERT_TRUE(schema.initialize())
        << "Schema initialization failed";

    const auto beginResult = connection.execute("BEGIN");

    ASSERT_TRUE(beginResult.isOk())
        << "Failed to begin transaction";

    MeasurementRepository repository(connection);

    const Measurement measurement{
        .measuredAt = std::chrono::system_clock::now(),
        .pvPower = 1234.0,
        .gridPower = -100.0,
        .batteryPower = 500.0,
        .loadPower = 1634.0,
        .batterySoc = 75.0
    };

    const auto measurementId = repository.save(measurement);

    ASSERT_NE(measurementId, 0)
        << "Failed to save measurement";

    const std::string query =
        "SELECT pv_power, grid_power, battery_power, load_power, battery_soc "
        "FROM measurements "
        "WHERE id = " + std::to_string(measurementId);

    const auto result = connection.execute(query.c_str());

    ASSERT_TRUE(result.isOk())
        << "Failed to read measurement";

    ASSERT_EQ(result.rowCount(), 1)
        << "Expected exactly one measurement";

    EXPECT_DOUBLE_EQ(
        std::stod(result.value(0, 0)),
        measurement.pvPower);

    EXPECT_DOUBLE_EQ(
        std::stod(result.value(0, 1)),
        measurement.gridPower);

    EXPECT_DOUBLE_EQ(
        std::stod(result.value(0, 2)),
        measurement.batteryPower);

    EXPECT_DOUBLE_EQ(
        std::stod(result.value(0, 3)),
        measurement.loadPower);

    EXPECT_DOUBLE_EQ(
        std::stod(result.value(0, 4)),
        measurement.batterySoc);

    const auto rollbackResult = connection.execute("ROLLBACK");

    ASSERT_TRUE(rollbackResult.isOk())
        << "Failed to rollback transaction";
}
