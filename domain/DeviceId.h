/******************************************************************************
**
** File      DeviceId.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <string>

namespace domain {

struct DeviceId
{
	std::string value;

	bool operator==(const DeviceId &) const = default;
};

} // namespace domain
