/******************************************************************************
**
** File      DatabaseSchema.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "DatabaseSchema.h"

#include "MigrationRunner.h"
#include "Migrations.h"

namespace database {

DatabaseSchema::DatabaseSchema(PostgresConnection &connection) : m_connection(connection) {}

bool DatabaseSchema::initialize()
{
	MigrationRunner runner(m_connection, allMigrations());

	const auto result = runner.run();

	m_errorMessage = result.error;
	m_errorKind = result.errorKind;
	m_sqlState = result.sqlState;

	return result.ok;
}

const std::string &DatabaseSchema::errorMessage() const
{
	return m_errorMessage;
}

ErrorKind DatabaseSchema::errorKind() const
{
	return m_errorKind;
}

const std::string &DatabaseSchema::sqlState() const
{
	return m_sqlState;
}

} // namespace database
