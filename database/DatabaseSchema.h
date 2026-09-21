#pragma once

class PostgresConnection;

class DatabaseSchema
{
public:
    explicit DatabaseSchema(PostgresConnection& connection);

    bool initialize();

private:
    PostgresConnection& m_connection;
};
