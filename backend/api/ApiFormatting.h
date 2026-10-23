/******************************************************************************
**
** File      ApiFormatting.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <chrono>
#include <string>

#include <boost/json/object.hpp>

#include <domain/Measurement.h>

namespace api {

std::string formatTimestamp(const std::chrono::system_clock::time_point &timePoint);

// Measurement JSON shared by REST and WebSocket. Empty optional values are
// serialized as null.
boost::json::object formatMeasurement(const domain::Measurement &measurement);

} // namespace api
