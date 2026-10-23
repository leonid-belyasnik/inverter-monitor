/******************************************************************************
**
** File      TelemetryParser.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <optional>
#include <string_view>

#include <domain/Measurement.h>

namespace protocol {

class TelemetryParser
{
  public:
	std::optional<domain::Measurement> parse(const std::string_view payload) const;
};

} // namespace protocol
