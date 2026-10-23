#include "DatabaseSchema.h"

#include "MigrationRunner.h"
#include "Migrations.h"

namespace database {

DatabaseSchema::DatabaseSchema(PostgresConnection& connection)
    : m_connection(connection)
{
}

bool DatabaseSchema::initialize()
{
    MigrationRunner runner(m_connection, allMigrations());

    const auto result = runner.run();

    m_errorMessage = result.error;

    return result.ok;
}

const std::string& DatabaseSchema::errorMessage() const
{
    return m_errorMessage;
}

} // namespace database
