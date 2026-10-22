#include "PostgresResult.h"

#include <libpq-fe.h>

namespace database {

void PostgresResult::ResultDeleter::operator()(PGresult* result) const
{
    if (result)
        PQclear(result);
}

PostgresResult::PostgresResult(PGresult* result)
    : m_result(result)
{
}

bool PostgresResult::isOk() const
{
    if (!m_result) {
    	return false;
    }

    const auto resultStatus = PQresultStatus(m_result.get());

    return resultStatus == PGRES_TUPLES_OK ||
           resultStatus == PGRES_COMMAND_OK;
}

int PostgresResult::rowCount() const
{
    return PQntuples(m_result.get());
}

int PostgresResult::columnCount() const
{
    return PQnfields(m_result.get());
}

const char* PostgresResult::value(const int row, const int column) const
{
    return PQgetvalue(m_result.get(), row, column);
}

bool PostgresResult::isNull(const int row, const int column) const
{
    return PQgetisnull(m_result.get(), row, column) == 1;
}

const char* PostgresResult::errorMessage() const
{
    return m_result ? PQresultErrorMessage(m_result.get()) : "No result";
}

PGresult* PostgresResult::get() const
{
    return m_result.get();
}

} // namespace database
