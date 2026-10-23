/******************************************************************************
**
** File      MeasurementQueryService.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <database/MeasurementRecord.h>

namespace database {
class MeasurementRepository;
}

namespace application {

class MeasurementQueryService
{
  public:
	explicit MeasurementQueryService(database::MeasurementRepository &repository);

	std::vector<database::MeasurementRecord> findByDevice(const std::string &deviceId,
	                                                      std::size_t limit) const;

  private:
	database::MeasurementRepository &m_repository;
};

} // namespace application
