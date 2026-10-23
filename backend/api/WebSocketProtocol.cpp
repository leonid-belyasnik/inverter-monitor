#include "WebSocketProtocol.h"

#include <boost/json.hpp>

namespace api {

std::optional<SubscribeCommand> parseSubscribeCommand(const std::string &message)
{
	try {
		const auto value = boost::json::parse(message);

		if (!value.is_object())
			return std::nullopt;

		const auto &object = value.as_object();

		const auto *type = object.if_contains("type");
		const auto *deviceId = object.if_contains("device_id");

		if (!type || !deviceId)
			return std::nullopt;

		if (!type->is_string() || !deviceId->is_string())
			return std::nullopt;

		if (type->as_string() != "subscribe")
			return std::nullopt;

		return SubscribeCommand{std::string(deviceId->as_string())};
	} catch (const boost::json::system_error &) {
		return std::nullopt;
	}
}

} // namespace api
