#include "PostgresResult.h"

#include <libpq-fe.h>

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
    return m_result && PQresultStatus(m_result.get()) == PGRES_TUPLES_OK;
}

const char* PostgresResult::errorMessage() const
{
    return m_result ? PQresultErrorMessage(m_result.get()) : "No result";
}

PGresult* PostgresResult::get() const
{
    return m_result.get();
}
