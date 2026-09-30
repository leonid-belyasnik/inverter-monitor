#pragma once

#include <domain/DeviceId.h>
#include <domain/Measurement.h>

namespace database {

struct MeasurementRecord
{
    domain::DeviceId deviceId;
    domain::Measurement measurement;
};

} // namespace database
