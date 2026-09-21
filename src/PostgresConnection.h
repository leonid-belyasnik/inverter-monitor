#pragma once

#include <memory>

#include <libpq-fe.h>

class PostgresConnection
{
public:
    explicit PostgresConnection(const char* connectionString);

    bool isOk() const;
    const char* errorMessage() const;

    PGconn* get() const;

private:
    struct ConnectionDeleter
    {
        void operator()(PGconn* connection) const;
    };

    using ConnectionPtr = std::unique_ptr<PGconn, ConnectionDeleter>;

    ConnectionPtr m_connection;
};
