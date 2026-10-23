/******************************************************************************
**
** File      MeasurementQueryService.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "MeasurementQueryService.h"

#include <database/MeasurementRepository.h>

namespace application {

MeasurementQueryService::MeasurementQueryService(database::MeasurementRepository &repository)
    : m_repository(repository)
{
}

std::vector<database::MeasurementRecord>
MeasurementQueryService::findByDevice(const std::string &deviceId, const std::size_t limit) const
{
	return m_repository.findByDevice(domain::DeviceId{deviceId}, limit);
}

} // namespace application
