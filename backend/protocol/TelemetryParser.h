#pragma once

#include <string_view>
#include <optional>

#include <domain/Measurement.h>

class TelemetryParser
{
public:
    std::optional<Measurement> parse(
        const std::string_view payload) const;
};
