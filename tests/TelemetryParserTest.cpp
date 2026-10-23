/******************************************************************************
**
** File      TelemetryParserTest.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include <chrono>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include <domain/Measurement.h>
#include <protocol/TelemetryParser.h>

TEST(TelemetryParserTest, ParsesValidTelemetry)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 38,
    "measured_at": "2026-09-24T18:50:00.316Z",
    "measurement": {
        "pv_power_w": 1924.66,
        "grid_power_w": -200.00,
        "battery_power_w": -449.73,
        "load_power_w": 1274.93,
        "battery_soc_pct": 75.16,
        "grid_voltage_v": 229.8,
        "grid_relay_on": true
    }
}
)json";

	const protocol::TelemetryParser parser;

	const auto measurement = parser.parse(payload);

	ASSERT_TRUE(measurement.has_value());

	EXPECT_DOUBLE_EQ(measurement->pvPower.value(), 1924.66);
	EXPECT_DOUBLE_EQ(measurement->gridPower.value(), -200.00);
	EXPECT_DOUBLE_EQ(measurement->batteryPower, -449.73);
	EXPECT_DOUBLE_EQ(measurement->loadPower.value(), 1274.93);
	EXPECT_DOUBLE_EQ(measurement->batterySoc, 75.16);
	EXPECT_DOUBLE_EQ(measurement->gridVoltage.value(), 229.8);
	EXPECT_EQ(measurement->gridRelayOn, std::optional<bool>(true));

	const auto expectedTime =
	    std::chrono::system_clock::from_time_t(1790275800) + std::chrono::milliseconds(316);

	EXPECT_EQ(measurement->measuredAt, expectedTime);
}

TEST(TelemetryParserTest, ParsesTimestampWithTwoFractionalDigits)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 39,
    "measured_at": "2026-09-24T18:50:00.31Z",
    "measurement": {
        "pv_power_w": 100.0,
        "grid_power_w": 200.0,
        "battery_power_w": 300.0,
        "load_power_w": 400.0,
        "battery_soc_pct": 50.0
    }
}
)json";

	const protocol::TelemetryParser parser;

	const auto measurement = parser.parse(payload);

	ASSERT_TRUE(measurement.has_value());

	const auto expectedTime =
	    std::chrono::system_clock::from_time_t(1790275800) + std::chrono::milliseconds(310);

	EXPECT_EQ(measurement->measuredAt, expectedTime);
}

TEST(TelemetryParserTest, RejectsInvalidJson)
{
	const protocol::TelemetryParser parser;

	const auto measurement = parser.parse("{ invalid json");

	EXPECT_FALSE(measurement.has_value());
}

TEST(TelemetryParserTest, RejectsMissingMeasurement)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 38,
    "measured_at": "2026-09-24T18:50:00.316Z"
}
)json";

	const protocol::TelemetryParser parser;

	const auto measurement = parser.parse(payload);

	EXPECT_FALSE(measurement.has_value());
}

TEST(TelemetryParserTest, AcceptsAbsentOptionalPowerFields)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 1,
    "measured_at": "2026-09-24T18:50:00.316Z",
    "measurement": {
        "battery_power_w": -203.0,
        "battery_soc_pct": 98.0
    }
}
)json";

	const protocol::TelemetryParser parser;

	const auto measurement = parser.parse(payload);

	ASSERT_TRUE(measurement.has_value());

	EXPECT_FALSE(measurement->pvPower.has_value());
	EXPECT_FALSE(measurement->gridPower.has_value());
	EXPECT_FALSE(measurement->loadPower.has_value());
	EXPECT_FALSE(measurement->gridVoltage.has_value());
	EXPECT_FALSE(measurement->gridRelayOn.has_value());
	EXPECT_DOUBLE_EQ(measurement->batteryPower, -203.0);
	EXPECT_DOUBLE_EQ(measurement->batterySoc, 98.0);
}

TEST(TelemetryParserTest, AcceptsNullOptionalPowerFields)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 2,
    "measured_at": "2026-09-24T18:50:00.316Z",
    "measurement": {
        "pv_power_w": null,
        "grid_power_w": null,
        "battery_power_w": 150.0,
        "load_power_w": null,
        "battery_soc_pct": 60.0
    }
}
)json";

	const protocol::TelemetryParser parser;

	const auto measurement = parser.parse(payload);

	ASSERT_TRUE(measurement.has_value());

	EXPECT_FALSE(measurement->pvPower.has_value());
	EXPECT_FALSE(measurement->gridPower.has_value());
	EXPECT_FALSE(measurement->loadPower.has_value());
	EXPECT_FALSE(measurement->gridVoltage.has_value());
	EXPECT_DOUBLE_EQ(measurement->batteryPower, 150.0);
	EXPECT_DOUBLE_EQ(measurement->batterySoc, 60.0);
}

TEST(TelemetryParserTest, RejectsNonNumericOptionalPowerField)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 3,
    "measured_at": "2026-09-24T18:50:00.316Z",
    "measurement": {
        "pv_power_w": "100",
        "battery_power_w": 150.0,
        "battery_soc_pct": 60.0
    }
}
)json";

	const protocol::TelemetryParser parser;

	EXPECT_FALSE(parser.parse(payload).has_value());
}

TEST(TelemetryParserTest, KeepsZeroGridVoltageAsValue)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 7,
    "measured_at": "2026-09-24T18:50:00.316Z",
    "measurement": {
        "battery_power_w": 150.0,
        "battery_soc_pct": 60.0,
        "grid_voltage_v": 0
    }
}
)json";

	const protocol::TelemetryParser parser;

	const auto measurement = parser.parse(payload);

	ASSERT_TRUE(measurement.has_value());
	ASSERT_TRUE(measurement->gridVoltage.has_value());
	EXPECT_DOUBLE_EQ(measurement->gridVoltage.value(), 0.0);
}

TEST(TelemetryParserTest, AcceptsNullGridVoltage)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 8,
    "measured_at": "2026-09-24T18:50:00.316Z",
    "measurement": {
        "battery_power_w": 150.0,
        "battery_soc_pct": 60.0,
        "grid_voltage_v": null
    }
}
)json";

	const protocol::TelemetryParser parser;

	const auto measurement = parser.parse(payload);

	ASSERT_TRUE(measurement.has_value());
	EXPECT_FALSE(measurement->gridVoltage.has_value());
}

TEST(TelemetryParserTest, RejectsNonNumericGridVoltage)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 9,
    "measured_at": "2026-09-24T18:50:00.316Z",
    "measurement": {
        "battery_power_w": 150.0,
        "battery_soc_pct": 60.0,
        "grid_voltage_v": "231.5"
    }
}
)json";

	const protocol::TelemetryParser parser;

	EXPECT_FALSE(parser.parse(payload).has_value());
}

// Messages published before grid_voltage_v and grid_relay_on were added.
TEST(TelemetryParserTest, AcceptsLegacyMessageWithoutGridVoltageAndRelay)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 42,
    "measured_at": "2026-10-21T12:34:56.789Z",
    "measurement": {
        "pv_power_w": 1820,
        "grid_power_w": -200,
        "battery_power_w": -450,
        "load_power_w": 1170,
        "battery_soc_pct": 76
    }
}
)json";

	const protocol::TelemetryParser parser;

	const auto measurement = parser.parse(payload);

	ASSERT_TRUE(measurement.has_value());
	EXPECT_DOUBLE_EQ(measurement->pvPower.value(), 1820.0);
	EXPECT_DOUBLE_EQ(measurement->gridPower.value(), -200.0);
	EXPECT_DOUBLE_EQ(measurement->batteryPower, -450.0);
	EXPECT_DOUBLE_EQ(measurement->loadPower.value(), 1170.0);
	EXPECT_DOUBLE_EQ(measurement->batterySoc, 76.0);
	EXPECT_FALSE(measurement->gridVoltage.has_value());
	EXPECT_FALSE(measurement->gridRelayOn.has_value());
}

TEST(TelemetryParserTest, KeepsGridRelayOffAsValue)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 10,
    "measured_at": "2026-09-24T18:50:00.316Z",
    "measurement": {
        "battery_power_w": 150.0,
        "battery_soc_pct": 60.0,
        "grid_voltage_v": 187.3,
        "grid_relay_on": false
    }
}
)json";

	const protocol::TelemetryParser parser;

	const auto measurement = parser.parse(payload);

	ASSERT_TRUE(measurement.has_value());
	EXPECT_EQ(measurement->gridRelayOn, std::optional<bool>(false));
	EXPECT_DOUBLE_EQ(measurement->gridVoltage.value(), 187.3);
}

TEST(TelemetryParserTest, AcceptsNullGridRelay)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 11,
    "measured_at": "2026-09-24T18:50:00.316Z",
    "measurement": {
        "battery_power_w": 150.0,
        "battery_soc_pct": 60.0,
        "grid_relay_on": null
    }
}
)json";

	const protocol::TelemetryParser parser;

	const auto measurement = parser.parse(payload);

	ASSERT_TRUE(measurement.has_value());
	EXPECT_FALSE(measurement->gridRelayOn.has_value());
}

TEST(TelemetryParserTest, RejectsNonBooleanGridRelay)
{
	for (const std::string value : {"1", "0", "\"true\"", "\"ON\""}) {
		const std::string payload =
		    R"json({"schema_version":1,"seq":12,"measured_at":"2026-09-24T18:50:00.316Z",)json"
		    R"json("measurement":{"battery_power_w":150.0,"battery_soc_pct":60.0,"grid_relay_on":)json" +
		    value + "}}";

		const protocol::TelemetryParser parser;

		EXPECT_FALSE(parser.parse(payload).has_value()) << value;
	}
}

TEST(TelemetryParserTest, RejectsMissingBatterySoc)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 4,
    "measured_at": "2026-09-24T18:50:00.316Z",
    "measurement": {
        "battery_power_w": 150.0
    }
}
)json";

	const protocol::TelemetryParser parser;

	EXPECT_FALSE(parser.parse(payload).has_value());
}

TEST(TelemetryParserTest, RejectsNullBatteryPower)
{
	const std::string payload = R"json(
{
    "schema_version": 1,
    "seq": 5,
    "measured_at": "2026-09-24T18:50:00.316Z",
    "measurement": {
        "battery_power_w": null,
        "battery_soc_pct": 60.0
    }
}
)json";

	const protocol::TelemetryParser parser;

	EXPECT_FALSE(parser.parse(payload).has_value());
}
