/******************************************************************************
**
** File      MeasurementRepository.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <database/MeasurementRecord.h>

#include <domain/DeviceId.h>
#include <domain/Measurement.h>

namespace database {

class PostgresConnection;

struct DatabaseError
{
	std::string message;
	// Empty when the server did not report a SQLSTATE.
	std::string sqlState;
};

class MeasurementRepository
{
  public:
	explicit MeasurementRepository(PostgresConnection &connection);

	// Returns 0 on failure; the cause is stored in error when it is not null.
	std::int64_t save(const domain::DeviceId &deviceId, const domain::Measurement &measurement,
	                  DatabaseError *error = nullptr);

	std::vector<MeasurementRecord> findByDevice(const domain::DeviceId &deviceId,
	                                            std::size_t limit);

  private:
	PostgresConnection &m_connection;
};

} // namespace database
