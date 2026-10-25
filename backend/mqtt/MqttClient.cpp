/******************************************************************************
**
** File      MqttClient.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "MqttClient.h"

#include <mosquitto.h>

#include <iostream>
#include <utility>

namespace mqtt {

MqttClient::MqttClient(std::string clientId, std::string host, const int port,
                       MessageHandler messageHandler)
    : m_clientId(std::move(clientId)), m_host(std::move(host)), m_port(port),
      m_messageHandler(std::move(messageHandler))
{
	mosquitto_lib_init();

	m_client = mosquitto_new(m_clientId.c_str(), true, this);

	if (!m_client) {
		m_errorMessage = "Failed to create MQTT client";
		return;
	}

	mosquitto_connect_callback_set(m_client, &MqttClient::onConnect);
	mosquitto_message_callback_set(m_client, &MqttClient::onMessage);
	mosquitto_disconnect_callback_set(m_client, &MqttClient::onDisconnect);

	mosquitto_reconnect_delay_set(m_client, 1, 30, true);
}

MqttClient::~MqttClient()
{
	if (m_client) {
		mosquitto_loop_stop(m_client, true);
		mosquitto_disconnect(m_client);
		mosquitto_destroy(m_client);
	}

	mosquitto_lib_cleanup();
}

bool MqttClient::setCredentials(const std::string &username, const std::string &password)
{
	if (!m_client) {
		return false;
	}

	const int result = mosquitto_username_pw_set(m_client, username.c_str(),
	                                             password.empty() ? nullptr : password.c_str());

	if (result != MOSQ_ERR_SUCCESS) {
		m_errorMessage = mosquitto_strerror(result);
		return false;
	}

	return true;
}

bool MqttClient::connect()
{
	if (!m_client) {
		return false;
	}

	const int result = mosquitto_connect(m_client, m_host.c_str(), m_port, 60);

	if (result != MOSQ_ERR_SUCCESS) {
		m_errorMessage = mosquitto_strerror(result);
		return false;
	}

	return true;
}

bool MqttClient::subscribe(std::string topic, const int qos)
{
	if (!m_client) {
		return false;
	}

	m_subscriptionTopic = std::move(topic);

	const int result = mosquitto_subscribe(m_client, nullptr, m_subscriptionTopic.c_str(), qos);

	if (result != MOSQ_ERR_SUCCESS) {
		m_errorMessage = mosquitto_strerror(result);
		return false;
	}

	return true;
}

bool MqttClient::startLoop()
{
	if (!m_client) {
		return false;
	}

	const int result = mosquitto_loop_start(m_client);

	if (result != MOSQ_ERR_SUCCESS) {
		m_errorMessage = mosquitto_strerror(result);
		return false;
	}

	return true;
}

void MqttClient::stopLoop()
{
	if (m_client) {
		mosquitto_loop_stop(m_client, true);
	}
}

const std::string &MqttClient::errorMessage() const
{
	return m_errorMessage;
}

void MqttClient::onConnect(mosquitto *client, void *userdata, const int result)
{
	auto *self = static_cast<MqttClient *>(userdata);

	if (result != MOSQ_ERR_SUCCESS) {
		self->m_errorMessage = mosquitto_connack_string(result);

		std::cerr << "MQTT connection failed: " << self->m_errorMessage << '\n';

		return;
	}

	std::cout << "Connected to MQTT broker\n";

	if (!self->m_subscriptionTopic.empty()) {
		const int subscribeResult =
		    mosquitto_subscribe(client, nullptr, self->m_subscriptionTopic.c_str(), 1);

		if (subscribeResult != MOSQ_ERR_SUCCESS) {
			self->m_errorMessage = mosquitto_strerror(subscribeResult);

			std::cerr << "MQTT subscribe failed: " << self->m_errorMessage << '\n';
		}
	}
}

void MqttClient::onMessage(mosquitto *, void *userdata, const mosquitto_message *message)
{
	auto *self = static_cast<MqttClient *>(userdata);

	if (!message || !message->topic || !message->payload) {
		return;
	}

	const auto payloadSize = static_cast<std::size_t>(message->payloadlen);

	if (self->m_messageHandler) {
		self->m_messageHandler(
		    message->topic,
		    std::string_view(static_cast<const char *>(message->payload), payloadSize));
	}
}

void MqttClient::onDisconnect(mosquitto *, void *, const int result)
{
	if (result == MOSQ_ERR_SUCCESS) {
		std::cout << "Disconnected from MQTT broker\n";
		return;
	}

	std::cerr << "MQTT disconnected: " << mosquitto_strerror(result) << '\n';
}

} // namespace mqtt
