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
