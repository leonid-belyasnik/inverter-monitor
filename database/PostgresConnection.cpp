#include "PostgresConnection.h"

#include <libpq-fe.h>

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>

#include "PostgresResult.h"

namespace database {

namespace {

struct ConnectionDefault
{
    const char* keyword;
    const char* value;
};

// Keepalives detect a dead idle connection after about 60 s; tcp_user_timeout
// (Linux) drops a connection whose sent data stays unacknowledged for 60 s.
// libpq ignores keepalive and tcp_user_timeout settings where the platform does
// not support them and for Unix-domain sockets. They do not bound the time a
// server takes to execute a query.
constexpr ConnectionDefault connectionDefaults[] = {
    {"connect_timeout", "5"},
    {"keepalives", "1"},
    {"keepalives_idle", "30"},
    {"keepalives_interval", "10"},
    {"keepalives_count", "3"},
    {"tcp_user_timeout", "60000"},
};

struct ConnectionOptionsDeleter
{
    void operator()(PQconninfoOption* options) const { PQconninfoFree(options); }
};

// Same rules as libpq's integer option parsing.
bool isValidIntegerOption(const char* value)
{
    char* end = nullptr;
    errno = 0;
    const long number = std::strtol(value, &end, 10);

    if (end == value || errno != 0 || number < INT_MIN || number > INT_MAX) {
        return false;
    }

    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) {
        ++end;
    }

    return *end == '\0';
}

// libpq returns no result when the query could not be sent, for example when
// the connection is lost; the reason is then kept on the connection.
PostgresResult makeResult(PGconn* connection, PGresult* result)
{
    if (result) {
        return PostgresResult(result);
    }

    return PostgresResult(
        nullptr,
        connection ? PQerrorMessage(connection) : "No connection");
}

} // namespace

ConnectionParameters resolveConnectionParameters(
    const char* connectionString,
    const EnvironmentLookup& getEnvironment
)
{
    ConnectionParameters parameters;

    char* parseError = nullptr;
    const std::unique_ptr<PQconninfoOption, ConnectionOptionsDeleter> options(
        PQconninfoParse(connectionString ? connectionString : "", &parseError));

    if (!options) {
        parameters.error = parseError ? parseError : "Failed to parse connection string";
        PQfreemem(parseError);
        return parameters;
    }

    for (const auto* option = options.get(); option->keyword; ++option) {
        if (option->val) {
            parameters.values.emplace_back(option->keyword, option->val);
        }
    }

    for (const auto& setting : connectionDefaults) {
        const auto* option = options.get();

        while (option->keyword && std::strcmp(option->keyword, setting.keyword) != 0) {
            ++option;
        }

        if (!option->keyword) {
            continue;
        }

        const char* configured = option->val;

        if (!configured && option->envvar) {
            configured = getEnvironment(option->envvar);
        }

        if (!configured) {
            parameters.values.emplace_back(setting.keyword, setting.value);
            continue;
        }

        if (std::strcmp(setting.keyword, "connect_timeout") == 0 &&
            !isValidIntegerOption(configured)) {
            parameters.error = std::string("Invalid connect_timeout value \"") + configured + '"';
            parameters.values.clear();
            return parameters;
        }
    }

    return parameters;
}

void PostgresConnection::ConnectionDeleter::operator()(PGconn* connection) const
{
    if (connection) {
        PQfinish(connection);
    }
}

PostgresConnection::PostgresConnection(const char* connectionString)
{
    const auto parameters = resolveConnectionParameters(
        connectionString, [](const char* name) { return std::getenv(name); });

    if (!parameters.error.empty()) {
        m_parameterError = parameters.error;
        m_failureKind = classifyConnectionFailure({.invalidParameters = true});
        return;
    }

    std::vector<const char*> keywords;
    std::vector<const char*> values;

    for (const auto& [keyword, value] : parameters.values) {
        keywords.push_back(keyword.c_str());
        values.push_back(value.c_str());
    }

    keywords.push_back(nullptr);
    values.push_back(nullptr);

    m_connection.reset(PQconnectdbParams(keywords.data(), values.data(), 0));

    if (m_connection && PQstatus(m_connection.get()) != CONNECTION_OK) {
        m_failureKind = classifyConnectionFailure({
            .passwordNeeded = PQconnectionNeedsPassword(m_connection.get()) != 0,
        });
    }
}

bool PostgresConnection::isOk() const
{
    std::lock_guard lock(m_mutex);
    return m_connection && PQstatus(m_connection.get()) == CONNECTION_OK;
}

std::string PostgresConnection::errorMessage() const
{
    std::lock_guard lock(m_mutex);
    if (m_connection) {
        return PQerrorMessage(m_connection.get());
    }

    return m_parameterError.empty() ? "No connection" : m_parameterError;
}

ErrorKind PostgresConnection::failureKind() const
{
    return m_failureKind;
}

PostgresResult PostgresConnection::execute(const char* query) const
{
    std::lock_guard lock(m_mutex);
    return makeResult(m_connection.get(), PQexec(m_connection.get(), query));
}

PostgresResult PostgresConnection::executeParams(
    const char* query,
    const int parameterCount,
    const char* const* parameters
) const
{
    std::lock_guard lock(m_mutex);
    return makeResult(
        m_connection.get(),
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

} // namespace database
