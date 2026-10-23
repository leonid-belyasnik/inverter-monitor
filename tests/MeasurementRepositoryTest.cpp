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
    bool handlerCalled = false;

    application::TelemetryIngestionService ingestion(
        repository,
        [&handlerCalled](const application::IngestedMeasurement &)
        {
            handlerCalled = true;
        });
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
        "battery_soc_pct": 75.0,
        "grid_voltage_v": 230.4
    }
}
)json";

    const auto ingested = adapter.adapt(
        "v1/devices/deye-001/telemetry",
        payload);

    ASSERT_TRUE(ingested.has_value());

    const auto measurementId = ingestion.ingest(*ingested);

    EXPECT_TRUE(handlerCalled);

    ASSERT_TRUE(measurementId.has_value())
        << "Failed to ingest measurement";

    const std::string query =
        "SELECT device_id, pv_power, grid_power, battery_power, load_power, battery_soc, grid_voltage "
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
    EXPECT_DOUBLE_EQ(std::stod(result.value(0, 6)), 230.4);

    const auto rollbackResult = connection.execute("ROLLBACK");

    ASSERT_TRUE(rollbackResult.isOk())
        << "Failed to rollback transaction";
}

TEST(MeasurementRepositoryTest, FindsMeasurementsByDevice)
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

    const domain::DeviceId deviceId{"deye-001"};

    const domain::Measurement measurement1{
        .measuredAt = std::chrono::system_clock::from_time_t(1779706200),
        .pvPower = 1000.0,
        .gridPower = -100.0,
        .batteryPower = 300.0,
        .loadPower = 1200.0,
        .batterySoc = 70.0
    };

    const domain::Measurement measurement2{
        .measuredAt = std::chrono::system_clock::from_time_t(1779706260),
        .pvPower = 2000.0,
        .gridPower = -200.0,
        .batteryPower = 400.0,
        .loadPower = 2200.0,
        .batterySoc = 71.0
    };

    ASSERT_NE(repository.save(deviceId, measurement1), 0);
    ASSERT_NE(repository.save(deviceId, measurement2), 0);

    const domain::DeviceId otherDevice{"deye-002"};

    ASSERT_NE(repository.save(otherDevice, measurement1), 0);

    const auto records = repository.findByDevice(deviceId, 100);

    ASSERT_EQ(records.size(), 2);

    EXPECT_EQ(records[0].deviceId.value, "deye-001");
    EXPECT_DOUBLE_EQ(records[0].measurement.pvPower.value(), 2000.0);
    EXPECT_DOUBLE_EQ(records[0].measurement.batterySoc, 71.0);

    EXPECT_EQ(records[1].deviceId.value, "deye-001");
    EXPECT_DOUBLE_EQ(records[1].measurement.pvPower.value(), 1000.0);
    EXPECT_DOUBLE_EQ(records[1].measurement.batterySoc, 70.0);

    const auto rollbackResult = connection.execute("ROLLBACK");

    ASSERT_TRUE(rollbackResult.isOk())
        << "Failed to rollback transaction";
}

TEST(MeasurementRepositoryTest, PersistsAndReadsMissingOptionalFieldsAsNull)
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

    const domain::DeviceId deviceId{"deye-null-test"};

    const domain::Measurement measurement{
        .measuredAt = std::chrono::system_clock::from_time_t(1779706200),
        .pvPower = std::nullopt,
        .gridPower = std::nullopt,
        .batteryPower = -203.0,
        .loadPower = std::nullopt,
        .batterySoc = 98.0,
        .gridVoltage = std::nullopt
    };

    const auto measurementId = repository.save(deviceId, measurement);

    ASSERT_NE(measurementId, 0);

    const std::string query =
        "SELECT pv_power, grid_power, battery_power, load_power, battery_soc, grid_voltage "
        "FROM measurements "
        "WHERE id = " + std::to_string(measurementId);

    const auto result = connection.execute(query.c_str());

    ASSERT_TRUE(result.isOk())
        << "Failed to read measurement";

    ASSERT_EQ(result.rowCount(), 1);

    EXPECT_TRUE(result.isNull(0, 0));
    EXPECT_TRUE(result.isNull(0, 1));
    EXPECT_FALSE(result.isNull(0, 2));
    EXPECT_TRUE(result.isNull(0, 3));
    EXPECT_FALSE(result.isNull(0, 4));
    EXPECT_TRUE(result.isNull(0, 5));
    EXPECT_DOUBLE_EQ(std::stod(result.value(0, 2)), -203.0);
    EXPECT_DOUBLE_EQ(std::stod(result.value(0, 4)), 98.0);

    const auto records = repository.findByDevice(deviceId, 100);

    ASSERT_EQ(records.size(), 1);

    EXPECT_FALSE(records[0].measurement.pvPower.has_value());
    EXPECT_FALSE(records[0].measurement.gridPower.has_value());
    EXPECT_FALSE(records[0].measurement.loadPower.has_value());
    EXPECT_FALSE(records[0].measurement.gridVoltage.has_value());
    EXPECT_DOUBLE_EQ(records[0].measurement.batteryPower, -203.0);
    EXPECT_DOUBLE_EQ(records[0].measurement.batterySoc, 98.0);

    const auto rollbackResult = connection.execute("ROLLBACK");

    ASSERT_TRUE(rollbackResult.isOk())
        << "Failed to rollback transaction";
}

TEST(MeasurementRepositoryTest, PersistsAndReadsGridVoltageIncludingZero)
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

    const domain::DeviceId deviceId{"deye-grid-voltage-test"};

    const domain::Measurement withVoltage{
        .measuredAt = std::chrono::system_clock::from_time_t(1779706200),
        .pvPower = std::nullopt,
        .gridPower = std::nullopt,
        .batteryPower = 100.0,
        .loadPower = std::nullopt,
        .batterySoc = 50.0,
        .gridVoltage = 231.5
    };

    const domain::Measurement zeroVoltage{
        .measuredAt = std::chrono::system_clock::from_time_t(1779706260),
        .pvPower = std::nullopt,
        .gridPower = std::nullopt,
        .batteryPower = 100.0,
        .loadPower = std::nullopt,
        .batterySoc = 50.0,
        .gridVoltage = 0.0
    };

    ASSERT_NE(repository.save(deviceId, withVoltage), 0);
    ASSERT_NE(repository.save(deviceId, zeroVoltage), 0);

    const auto records = repository.findByDevice(deviceId, 100);

    ASSERT_EQ(records.size(), 2);

    // Newest first: the zero value must stay a value, not become empty.
    ASSERT_TRUE(records[0].measurement.gridVoltage.has_value());
    EXPECT_DOUBLE_EQ(records[0].measurement.gridVoltage.value(), 0.0);

    ASSERT_TRUE(records[1].measurement.gridVoltage.has_value());
    EXPECT_DOUBLE_EQ(records[1].measurement.gridVoltage.value(), 231.5);

    const auto rollbackResult = connection.execute("ROLLBACK");

    ASSERT_TRUE(rollbackResult.isOk())
        << "Failed to rollback transaction";
}
