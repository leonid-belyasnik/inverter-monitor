#pragma once

#include <string_view>
#include <optional>

#include <domain/Measurement.h>

namespace protocol {

class TelemetryParser
{
public:
    std::optional<domain::Measurement> parse(
        const std::string_view payload) const;
};

} // namespace protocol
