#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "Migration.h"

namespace database {

class PostgresConnection;

struct MigrationResult
{
    bool ok = false;
    int appliedCount = 0;
    int currentVersion = 0;
    std::string error;
};

// Applies pending migrations in ascending version order. Each migration and its
// schema_migrations record are committed in one transaction under a
// transaction-level advisory lock.
//
// PostgresConnection serializes individual calls only, not transactions, so
// run() must be called before the connection is shared with other threads.
class MigrationRunner
{
public:
    static constexpr std::int64_t advisoryLockKey = 0x48504731'4d494752; // "HPG1MIGR"
    static constexpr std::chrono::milliseconds defaultLockTimeout{30000};

    MigrationRunner(
        PostgresConnection& connection,
        std::vector<Migration> migrations,
        std::chrono::milliseconds lockTimeout = defaultLockTimeout
    );

    MigrationResult run();

    static std::string validate(const std::vector<Migration>& migrations);

private:
    PostgresConnection& m_connection;
    std::vector<Migration> m_migrations;
    std::chrono::milliseconds m_lockTimeout;
};

} // namespace database
