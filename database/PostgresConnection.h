#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "ErrorClassification.h"

struct pg_conn;

namespace database {

using PGconn = pg_conn;

class PostgresResult;

using EnvironmentLookup = std::function<const char *(const char *)>;

struct ConnectionParameters
{
	// Keyword/value pairs for PQconnectdbParams.
	std::vector<std::pair<std::string, std::string>> values;
	// Set when the connection string cannot be parsed or connect_timeout is invalid.
	std::string error;
};

// Returns the options set in connectionString plus defaults for connect_timeout
// (5 s), TCP keepalives and tcp_user_timeout. A default is skipped when the
// option is set in connectionString, when its libpq environment variable (for
// example PGCONNECT_TIMEOUT) is set, or when this libpq does not know the option.
// Options left unset are still resolved by libpq from its environment variables.
ConnectionParameters resolveConnectionParameters(const char *connectionString,
                                                 const EnvironmentLookup &getEnvironment);

class PostgresConnection
{
  public:
	explicit PostgresConnection(const char *connectionString);

	bool isOk() const;
	std::string errorMessage() const;

	// Classifies the failed connection attempt; meaningful only when isOk() is false.
	ErrorKind failureKind() const;

	PostgresResult execute(const char *query) const;
	PostgresResult executeParams(const char *query, int parameterCount,
	                             const char *const *parameters) const;

  private:
	struct ConnectionDeleter
	{
		void operator()(PGconn *connection) const;
	};

	using ConnectionPtr = std::unique_ptr<PGconn, ConnectionDeleter>;

	ConnectionPtr m_connection;
	std::string m_parameterError;
	ErrorKind m_failureKind = ErrorKind::Transient;
	mutable std::mutex m_mutex;
};

} // namespace database
