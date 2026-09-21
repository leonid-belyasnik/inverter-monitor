#include "PostgresConnection.h"

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

PGconn* PostgresConnection::get() const
{
    return m_connection.get();
}
