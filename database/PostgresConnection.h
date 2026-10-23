#pragma once

#include <memory>
#include <mutex>
#include <string>

struct pg_conn;

namespace database {

using PGconn = pg_conn;

class PostgresResult;

class PostgresConnection
{
public:
    explicit PostgresConnection(const char* connectionString);

    bool isOk() const;
    std::string errorMessage() const;

    PostgresResult execute(const char* query) const;
    PostgresResult executeParams(
        const char* query,
        int parameterCount,
        const char* const* parameters
    ) const;

private:
    struct ConnectionDeleter
    {
        void operator()(PGconn* connection) const;
    };

    using ConnectionPtr = std::unique_ptr<PGconn, ConnectionDeleter>;

    ConnectionPtr m_connection;
    mutable std::mutex m_mutex;
};

} // namespace database
