/******************************************************************************
**
** File      MqttClientTest.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include <string>

#include <gtest/gtest.h>

#include <mqtt/MqttClient.h>

// setCredentials() only stores the credentials in the libmosquitto handle, so
// these tests do not need a running broker.
TEST(MqttClientTest, AcceptsUsernameAndPassword)
{
	mqtt::MqttClient client("mqtt-client-test", "localhost", 1883, nullptr);

	EXPECT_TRUE(client.setCredentials("backend", "test-password"));
	EXPECT_TRUE(client.errorMessage().empty());
}

TEST(MqttClientTest, AcceptsUsernameWithoutPassword)
{
	mqtt::MqttClient client("mqtt-client-test", "localhost", 1883, nullptr);

	EXPECT_TRUE(client.setCredentials("backend", ""));
	EXPECT_TRUE(client.errorMessage().empty());
}

TEST(MqttClientTest, RejectsMalformedUtf8UsernameWithoutLeakingCredentials)
{
	mqtt::MqttClient client("mqtt-client-test", "localhost", 1883, nullptr);
	const std::string username = "bad\xC3\x28user";
	const std::string password = "secret-test-password";

	EXPECT_FALSE(client.setCredentials(username, password));
	EXPECT_FALSE(client.errorMessage().empty());
	EXPECT_EQ(client.errorMessage().find(username), std::string::npos);
	EXPECT_EQ(client.errorMessage().find(password), std::string::npos);
}
