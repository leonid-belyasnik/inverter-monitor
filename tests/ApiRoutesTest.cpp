#include <chrono>
#include <string>

#include <gtest/gtest.h>

#include <boost/beast/http.hpp>
#include <boost/json.hpp>

#include <application/MeasurementQueryService.h>
#include <database/DatabaseSchema.h>
#include <database/MeasurementRepository.h>
#include <database/PostgresConnection.h>
#include <database/PostgresResult.h>

#include <api/ApiFormatting.h>
#include <api/ApiRoutes.h>
#include <api/HttpRouter.h>

TEST(ApiRoutesTest, ReturnsMeasurementsByDevice)
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

    const domain::DeviceId deviceId{"api-test-device"};

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

    application::MeasurementQueryService queryService(repository);

    api::HttpRouter router;
    api::ApiRoutes::registerRoutes(router, queryService);

    api::HttpRouter::Request request{
        boost::beast::http::verb::get,
        "/api/v1/devices/api-test-device/measurements?limit=1",
        11
    };

    const auto response = router.handle(request);

    ASSERT_EQ(
        response.result(),
        boost::beast::http::status::ok);

    const auto json =
        boost::json::parse(response.body());

    const auto& object = json.as_object();

    ASSERT_EQ(object.at("device_id").as_string(), "api-test-device");

    const auto& measurements =
        object.at("measurements").as_array();

    ASSERT_EQ(measurements.size(), 1);

    const auto& measurement =
        measurements.front().as_object();

    EXPECT_DOUBLE_EQ(
        measurement.at("pv_power").as_double(),
        2000.0);

    EXPECT_DOUBLE_EQ(
        measurement.at("grid_power").as_double(),
        -200.0);

    EXPECT_DOUBLE_EQ(
        measurement.at("battery_power").as_double(),
        400.0);

    EXPECT_DOUBLE_EQ(
        measurement.at("load_power").as_double(),
        2200.0);

    EXPECT_DOUBLE_EQ(
        measurement.at("battery_soc").as_double(),
        71.0);

    const auto rollbackResult = connection.execute("ROLLBACK");

    ASSERT_TRUE(rollbackResult.isOk())
        << "Failed to rollback transaction";
}

TEST(ApiRoutesTest, ReturnsNullForMissingOptionalFields)
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

    const domain::DeviceId deviceId{"api-null-test-device"};

    const domain::Measurement measurement{
        .measuredAt = std::chrono::system_clock::from_time_t(1779706200),
        .pvPower = std::nullopt,
        .gridPower = std::nullopt,
        .batteryPower = -203.0,
        .loadPower = std::nullopt,
        .batterySoc = 98.0
    };

    ASSERT_NE(repository.save(deviceId, measurement), 0);

    application::MeasurementQueryService queryService(repository);

    api::HttpRouter router;
    api::ApiRoutes::registerRoutes(router, queryService);

    api::HttpRouter::Request request{
        boost::beast::http::verb::get,
        "/api/v1/devices/api-null-test-device/measurements",
        11
    };

    const auto response = router.handle(request);

    ASSERT_EQ(
        response.result(),
        boost::beast::http::status::ok);

    const auto json =
        boost::json::parse(response.body());

    const auto& measurements =
        json.as_object().at("measurements").as_array();

    ASSERT_EQ(measurements.size(), 1);

    const auto& item =
        measurements.front().as_object();

    EXPECT_TRUE(item.at("pv_power").is_null());
    EXPECT_TRUE(item.at("grid_power").is_null());
    EXPECT_TRUE(item.at("load_power").is_null());
    EXPECT_DOUBLE_EQ(item.at("battery_power").as_double(), -203.0);
    EXPECT_DOUBLE_EQ(item.at("battery_soc").as_double(), 98.0);

    const auto rollbackResult = connection.execute("ROLLBACK");

    ASSERT_TRUE(rollbackResult.isOk())
        << "Failed to rollback transaction";
}

// formatMeasurement() also builds the WebSocket "data" object.
TEST(ApiRoutesTest, FormatsMeasurementWithAndWithoutOptionalFields)
{
    const domain::Measurement complete{
        .measuredAt = std::chrono::system_clock::from_time_t(1779706200)
            + std::chrono::milliseconds(316),
        .pvPower = 1000.0,
        .gridPower = -100.0,
        .batteryPower = 300.0,
        .loadPower = 1200.0,
        .batterySoc = 70.0
    };

    const auto completeJson = api::formatMeasurement(complete);

    EXPECT_EQ(completeJson.size(), 6);
    EXPECT_EQ(completeJson.at("measured_at").as_string(), "2026-05-25T10:50:00.316Z");
    EXPECT_DOUBLE_EQ(completeJson.at("pv_power").as_double(), 1000.0);
    EXPECT_DOUBLE_EQ(completeJson.at("grid_power").as_double(), -100.0);
    EXPECT_DOUBLE_EQ(completeJson.at("battery_power").as_double(), 300.0);
    EXPECT_DOUBLE_EQ(completeJson.at("load_power").as_double(), 1200.0);
    EXPECT_DOUBLE_EQ(completeJson.at("battery_soc").as_double(), 70.0);

    const domain::Measurement partial{
        .measuredAt = std::chrono::system_clock::from_time_t(1779706200),
        .pvPower = std::nullopt,
        .gridPower = std::nullopt,
        .batteryPower = -203.0,
        .loadPower = std::nullopt,
        .batterySoc = 98.0
    };

    const auto partialJson = api::formatMeasurement(partial);

    EXPECT_EQ(partialJson.size(), 6);
    EXPECT_TRUE(partialJson.at("pv_power").is_null());
    EXPECT_TRUE(partialJson.at("grid_power").is_null());
    EXPECT_TRUE(partialJson.at("load_power").is_null());
    EXPECT_DOUBLE_EQ(partialJson.at("battery_power").as_double(), -203.0);
    EXPECT_DOUBLE_EQ(partialJson.at("battery_soc").as_double(), 98.0);
}
