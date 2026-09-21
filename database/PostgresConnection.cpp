#include "PostgresConnection.h"

#include <libpq-fe.h>

#include "PostgresResult.h"

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
    return m_connection && PQstatus(m_connection.get()) == CONNECTION_OK;
}

const char* PostgresConnection::errorMessage() const
{
    return m_connection ? PQerrorMessage(m_connection.get()) : "No connection";
}

PostgresResult PostgresConnection::execute(const char* query) const
{
    return PostgresResult(PQexec(m_connection.get(), query));
}

PostgresResult PostgresConnection::executeParams(
    const char* query,
    const int parameterCount,
    const char* const* parameters
) const
{
    return PostgresResult(
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
