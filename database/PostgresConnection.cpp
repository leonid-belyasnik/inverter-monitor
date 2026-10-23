#include "PostgresConnection.h"

#include <libpq-fe.h>

#include "PostgresResult.h"

namespace database {

namespace {

// libpq returns no result when the query could not be sent, for example when
// the connection is lost; the reason is then kept on the connection.
PostgresResult makeResult(PGconn* connection, PGresult* result)
{
    if (result) {
        return PostgresResult(result);
    }

    return PostgresResult(
        nullptr,
        connection ? PQerrorMessage(connection) : "No connection");
}

} // namespace

void PostgresConnection::ConnectionDeleter::operator()(PGconn* connection) const
{
    if (connection) {
    	PQfinish(connection);	
    }
}

PostgresConnection::PostgresConnection(const char* connectionString)
    : m_connection(PQconnectdb(connectionString))
{
}

bool PostgresConnection::isOk() const
{
    std::lock_guard lock(m_mutex);
    return m_connection && PQstatus(m_connection.get()) == CONNECTION_OK;
}

std::string PostgresConnection::errorMessage() const
{
    std::lock_guard lock(m_mutex);
    return m_connection ? PQerrorMessage(m_connection.get()) : "No connection";
}

PostgresResult PostgresConnection::execute(const char* query) const
{
    std::lock_guard lock(m_mutex);
    return makeResult(m_connection.get(), PQexec(m_connection.get(), query));
}

PostgresResult PostgresConnection::executeParams(
    const char* query,
    const int parameterCount,
    const char* const* parameters
) const
{
    std::lock_guard lock(m_mutex);
    return makeResult(
        m_connection.get(),
        PQexecParams(
            m_connection.get(),
            query,
            parameterCount,
            nullptr,
            parameters,
            nullptr,
            nullptr,
            0
        )
    );
}

} // namespace database
