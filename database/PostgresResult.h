#pragma once

#include <memory>

struct pg_result;
using PGresult = struct pg_result;

class PostgresResult
{
public:
    explicit PostgresResult(PGresult* result);

    bool isOk() const;
    const char* errorMessage() const;

    PGresult* get() const;

private:
    struct ResultDeleter
    {
        void operator()(PGresult* result) const;
    };

    using ResultPtr = std::unique_ptr<PGresult, ResultDeleter>;

    ResultPtr m_result;
};
