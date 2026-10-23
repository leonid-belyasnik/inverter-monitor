#pragma once

#include <optional>
#include <string_view>

#include <application/IngestedMeasurement.h>
#include <protocol/TelemetryParser.h>

namespace mqtt {

class TelemetryAdapter
{
  public:
	explicit TelemetryAdapter(const protocol::TelemetryParser &parser);

	std::optional<application::IngestedMeasurement> adapt(std::string_view topic,
	                                                      std::string_view payload) const;

	static std::optional<domain::DeviceId> deviceIdFromTopic(std::string_view topic);

  private:
	const protocol::TelemetryParser &m_parser;
};

} // namespace mqtt
