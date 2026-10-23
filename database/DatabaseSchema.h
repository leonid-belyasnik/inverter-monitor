#pragma once

#include <string>

#include "ErrorClassification.h"

namespace database {

class PostgresConnection;

class DatabaseSchema
{
  public:
	explicit DatabaseSchema(PostgresConnection &connection);

	bool initialize();

	const std::string &errorMessage() const;
	ErrorKind errorKind() const;
	const std::string &sqlState() const;

  private:
	PostgresConnection &m_connection;
	std::string m_errorMessage;
	ErrorKind m_errorKind = ErrorKind::Permanent;
	std::string m_sqlState;
};

} // namespace database
