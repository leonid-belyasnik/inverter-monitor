/******************************************************************************
**
** File      IngestedMeasurement.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <domain/DeviceId.h>
#include <domain/Measurement.h>

namespace application {

struct IngestedMeasurement
{
	domain::DeviceId deviceId;
	domain::Measurement measurement;
};

} // namespace application
