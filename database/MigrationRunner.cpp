#include "MigrationRunner.h"

#include <algorithm>
#include <optional>
#include <set>
#include <utility>

#include "PostgresConnection.h"
#include "PostgresResult.h"

namespace database {

namespace {

std::string trimmed(std::string text)
{
    while (!text.empty() && (text.back() == '\n' || text.back() == ' ')) {
        text.pop_back();
    }

    return text;
}

std::string migrationLabel(const Migration& migration)
{
    return "Migration " + std::to_string(migration.version) + " (" + migration.name + ")";
}

std::optional<std::set<int>> readAppliedVersions(
    const PostgresConnection& connection,
    std::string& error
)
{
    const auto existsResult = connection.execute(
        "SELECT to_regclass('schema_migrations') IS NOT NULL"
    );

    if (!existsResult.isOk() || existsResult.rowCount() != 1) {
        error = "Failed to check schema_migrations: " + trimmed(existsResult.errorMessage());
        return std::nullopt;
    }

    std::set<int> versions;

    if (std::string(existsResult.value(0, 0)) != "t") {
        return versions;
    }

    const auto versionsResult = connection.execute(
        "SELECT version FROM schema_migrations ORDER BY version"
    );

    if (!versionsResult.isOk()) {
        error = "Failed to read schema_migrations: " + trimmed(versionsResult.errorMessage());
        return std::nullopt;
    }

    for (int row = 0; row < versionsResult.rowCount(); ++row) {
        versions.insert(std::stoi(versionsResult.value(row, 0)));
    }

    return versions;
}

std::string checkHistory(const std::set<int>& applied, const std::vector<Migration>& migrations)
{
    if (applied.empty()) {
        return {};
    }

    const int latestKnown = migrations.empty() ? 0 : migrations.back().version;

    for (const int version : applied) {
        const bool known = std::any_of(
            migrations.begin(), migrations.end(),
            [version](const Migration& migration) { return migration.version == version; }
        );

        if (!known) {
            return "Database schema version " + std::to_string(version) +
                   " is not known to this backend (latest known version " +
                   std::to_string(latestKnown) + ")";
        }
    }

    const int latestApplied = *applied.rbegin();

    for (const auto& migration : migrations) {
        if (migration.version < latestApplied && !applied.contains(migration.version)) {
            return "Database schema history has a gap: version " +
                   std::to_string(migration.version) + " is not applied, but version " +
                   std::to_string(latestApplied) + " is";
        }
    }

    return {};
}

} // namespace

MigrationRunner::MigrationRunner(
    PostgresConnection& connection,
    std::vector<Migration> migrations,
    const std::chrono::milliseconds lockTimeout
)
    : m_connection(connection)
    , m_migrations(std::move(migrations))
    , m_lockTimeout(lockTimeout)
{
}

std::string MigrationRunner::validate(const std::vector<Migration>& migrations)
{
    int previousVersion = 0;

    for (const auto& migration : migrations) {
        if (migration.version <= 0) {
            return "Migration version must be positive, got " + std::to_string(migration.version);
        }

        if (migration.version <= previousVersion) {
            return "Migration versions must be unique and strictly increasing: " +
                   std::to_string(migration.version) + " follows " +
                   std::to_string(previousVersion);
        }

        if (migration.name.empty()) {
            return "Migration " + std::to_string(migration.version) + " has an empty name";
        }

        if (migration.sql.empty()) {
            return migrationLabel(migration) + " has empty SQL";
        }

        previousVersion = migration.version;
    }

    return {};
}

MigrationResult MigrationRunner::run()
{
    MigrationResult result;

    if (const auto validationError = validate(m_migrations); !validationError.empty()) {
        result.error = "Invalid migration list: " + validationError;
        return result;
    }

    auto applied = readAppliedVersions(m_connection, result.error);

    if (!applied) {
        return result;
    }

    if (result.error = checkHistory(*applied, m_migrations); !result.error.empty()) {
        return result;
    }

    const auto setLockTimeout =
        "SET LOCAL lock_timeout = '" + std::to_string(m_lockTimeout.count()) + "ms'";
    const auto acquireLock =
        "SELECT pg_advisory_xact_lock(" + std::to_string(advisoryLockKey) + ")";

    for (const auto& migration : m_migrations) {
        if (applied->contains(migration.version)) {
            continue;
        }

        const auto label = migrationLabel(migration);

        auto fail = [&](const std::string& step, const std::string& reason) {
            m_connection.execute("ROLLBACK");
            result.error = label + " failed " + step + ": " + reason;
            return result;
        };

        if (const auto begin = m_connection.execute("BEGIN"); !begin.isOk()) {
            return fail("to begin transaction", trimmed(begin.errorMessage()));
        }

        if (const auto timeout = m_connection.execute(setLockTimeout.c_str()); !timeout.isOk()) {
            return fail("to set lock timeout", trimmed(timeout.errorMessage()));
        }

        if (const auto lock = m_connection.execute(acquireLock.c_str()); !lock.isOk()) {
            return fail("to acquire migration lock", trimmed(lock.errorMessage()));
        }

        const auto createTable = m_connection.execute(
            "CREATE TABLE IF NOT EXISTS schema_migrations ("
            "    version     integer PRIMARY KEY,"
            "    name        text NOT NULL,"
            "    applied_at  timestamptz NOT NULL DEFAULT now()"
            ")"
        );

        if (!createTable.isOk()) {
            return fail("to create schema_migrations", trimmed(createTable.errorMessage()));
        }

        std::string readError;
        applied = readAppliedVersions(m_connection, readError);

        if (!applied) {
            return fail("to re-read applied versions", readError);
        }

        if (const auto historyError = checkHistory(*applied, m_migrations); !historyError.empty()) {
            return fail("history check", historyError);
        }

        if (applied->contains(migration.version)) {
            if (const auto commit = m_connection.execute("COMMIT"); !commit.isOk()) {
                return fail("to commit", trimmed(commit.errorMessage()));
            }
            continue;
        }

        if (const auto apply = m_connection.execute(migration.sql.c_str()); !apply.isOk()) {
            return fail("to execute SQL", trimmed(apply.errorMessage()));
        }

        const auto versionText = std::to_string(migration.version);
        const char* parameters[] = {versionText.c_str(), migration.name.c_str()};

        const auto record = m_connection.executeParams(
            "INSERT INTO schema_migrations (version, name) VALUES ($1::integer, $2)",
            2,
            parameters
        );

        if (!record.isOk()) {
            return fail("to record version", trimmed(record.errorMessage()));
        }

        if (const auto commit = m_connection.execute("COMMIT"); !commit.isOk()) {
            return fail("to commit", trimmed(commit.errorMessage()));
        }

        applied->insert(migration.version);
        ++result.appliedCount;
    }

    result.currentVersion = applied->empty() ? 0 : *applied->rbegin();
    result.ok = true;
    return result;
}

} // namespace database
