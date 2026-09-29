#include <string>

#include <gtest/gtest.h>

#include <mqtt/TelemetryAdapter.h>
#include <protocol/TelemetryParser.h>

TEST(TelemetryAdapterTest, ParsesDeviceIdFromValidTelemetryTopic)
{
    const auto deviceId = mqtt::TelemetryAdapter::deviceIdFromTopic(
        "v1/devices/deye-001/telemetry");

    ASSERT_TRUE(deviceId.has_value());
    EXPECT_EQ(deviceId->value, "deye-001");
}

TEST(TelemetryAdapterTest, RejectsInvalidTelemetryTopic)
{
    EXPECT_FALSE(mqtt::TelemetryAdapter::deviceIdFromTopic(
        "v1/devices/deye-001/status").has_value());
    EXPECT_FALSE(mqtt::TelemetryAdapter::deviceIdFromTopic(
        "v1/devices//telemetry").has_value());
    EXPECT_FALSE(mqtt::TelemetryAdapter::deviceIdFromTopic(
        "v1/devices/deye-001/other/telemetry").has_value());
}

TEST(TelemetryAdapterTest, AdaptsValidTopicAndPayloadForIngestion)
{
    const std::string payload = R"json(
{
    "measured_at": "2026-09-24T18:50:00.316Z",
    "measurement": {
        "pv_power_w": 1924.66,
        "grid_power_w": -200.00,
        "battery_power_w": -449.73,
        "load_power_w": 1274.93,
        "battery_soc_pct": 75.16
    }
}
)json";

    const protocol::TelemetryParser parser;
    const mqtt::TelemetryAdapter adapter(parser);
    const auto ingested = adapter.adapt(
        "v1/devices/deye-001/telemetry",
        payload);

    ASSERT_TRUE(ingested.has_value());
    EXPECT_EQ(ingested->deviceId.value, "deye-001");
    EXPECT_DOUBLE_EQ(ingested->measurement.pvPower, 1924.66);
    EXPECT_DOUBLE_EQ(ingested->measurement.gridPower, -200.00);
    EXPECT_DOUBLE_EQ(ingested->measurement.batteryPower, -449.73);
    EXPECT_DOUBLE_EQ(ingested->measurement.loadPower, 1274.93);
    EXPECT_DOUBLE_EQ(ingested->measurement.batterySoc, 75.16);
}
