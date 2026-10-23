#pragma once

#include <string>

namespace database {

class PostgresConnection;

class DatabaseSchema
{
public:
    explicit DatabaseSchema(PostgresConnection& connection);

    bool initialize();

    const std::string& errorMessage() const;

private:
    PostgresConnection& m_connection;
    std::string m_errorMessage;
};

} // namespace database
