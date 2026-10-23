#pragma once

#include <functional>
#include <string>
#include <string_view>

struct mosquitto;
struct mosquitto_message;

namespace mqtt {

class MqttClient
{
  public:
	using MessageHandler = std::function<void(std::string_view topic, std::string_view payload)>;

	MqttClient(std::string clientId, std::string host, int port, MessageHandler messageHandler);

	~MqttClient();

	MqttClient(const MqttClient &) = delete;
	MqttClient &operator=(const MqttClient &) = delete;

	bool connect();
	bool subscribe(std::string topic, int qos = 1);

	bool startLoop();
	void stopLoop();

	const std::string &errorMessage() const;

  private:
	static void onConnect(mosquitto *client, void *userdata, int result);

	static void onMessage(mosquitto *client, void *userdata, const mosquitto_message *message);

	static void onDisconnect(mosquitto *client, void *userdata, int result);

	std::string m_clientId;
	std::string m_host;
	int m_port;
	MessageHandler m_messageHandler;

	mosquitto *m_client = nullptr;
	std::string m_subscriptionTopic;

	std::string m_errorMessage;
};

} // namespace mqtt
