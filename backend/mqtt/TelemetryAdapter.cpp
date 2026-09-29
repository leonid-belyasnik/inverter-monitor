#include "TelemetryAdapter.h"

#include <string>

namespace mqtt {

TelemetryAdapter::TelemetryAdapter(
    const protocol::TelemetryParser& parser)
    : m_parser(parser)
{
}

std::optional<application::IngestedMeasurement> TelemetryAdapter::adapt(
    const std::string_view topic,
    const std::string_view payload) const
{
    const auto deviceId = deviceIdFromTopic(topic);

    if (!deviceId) {
        return std::nullopt;
    }

    const auto measurement = m_parser.parse(payload);

    if (!measurement) {
        return std::nullopt;
    }

    return application::IngestedMeasurement{
        .deviceId = *deviceId,
        .measurement = *measurement
    };
}

std::optional<domain::DeviceId> TelemetryAdapter::deviceIdFromTopic(
    const std::string_view topic)
{
    constexpr std::string_view prefix = "v1/devices/";
    constexpr std::string_view suffix = "/telemetry";

    if (!topic.starts_with(prefix) || !topic.ends_with(suffix)) {
        return std::nullopt;
    }

    const auto deviceId = topic.substr(
        prefix.size(),
        topic.size() - prefix.size() - suffix.size());

    if (deviceId.empty() ||
        deviceId.find('/') != std::string_view::npos ||
        deviceId.find_first_of("+#") != std::string_view::npos) {
        return std::nullopt;
    }

    return domain::DeviceId{std::string(deviceId)};
}

} // namespace mqtt
