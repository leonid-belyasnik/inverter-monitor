#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <unistd.h>

#include <database/DatabaseSchema.h>
#include <database/MigrationRunner.h>
#include <database/Migrations.h>
#include <database/PostgresConnection.h>
#include <database/PostgresResult.h>

namespace {

using database::Migration;
using database::MigrationRunner;
using database::PostgresConnection;

std::string uniqueSchemaName()
{
    static std::atomic_int counter{0};

    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();

    return "migration_test_" + std::to_string(::getpid()) + "_" +
           std::to_string(now) + "_" + std::to_string(counter++);
}

// Every test runs in its own PostgreSQL schema. search_path contains only that
// schema, so unqualified names never resolve to public.
class MigrationRunnerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_connection = std::make_unique<PostgresConnection>("");

        ASSERT_TRUE(m_connection->isOk())
            << "Connection failed: " << m_connection->errorMessage();

        m_publicSchemaMigrationsExisted = queryText(
            "SELECT to_regclass('public.schema_migrations') IS NOT NULL") == "t";

        m_schema = uniqueSchemaName();

        ASSERT_TRUE(execute(*m_connection, "CREATE SCHEMA " + m_schema));
        m_schemaCreated = true;

        ASSERT_TRUE(usePrivateSchema(*m_connection));
        ASSERT_EQ(queryText("SELECT current_schema()"), m_schema);
    }

    void TearDown() override
    {
        if (!m_schemaCreated) {
            return;
        }

        execute(*m_connection, "ROLLBACK");
        EXPECT_TRUE(execute(*m_connection, "DROP SCHEMA " + m_schema + " CASCADE"));

        EXPECT_EQ(
            queryText("SELECT to_regclass('public.schema_migrations') IS NOT NULL") == "t",
            m_publicSchemaMigrationsExisted)
            << "Migration test changed public.schema_migrations";
    }

    bool execute(PostgresConnection& connection, const std::string& sql)
    {
        const auto result = connection.execute(sql.c_str());

        EXPECT_TRUE(result.isOk()) << sql << ": " << result.errorMessage();

        return result.isOk();
    }

    bool execute(const std::string& sql)
    {
        return execute(*m_connection, sql);
    }

    bool usePrivateSchema(PostgresConnection& connection)
    {
        return execute(connection, "SET search_path TO " + m_schema);
    }

    std::unique_ptr<PostgresConnection> openPrivateConnection()
    {
        auto connection = std::make_unique<PostgresConnection>("");

        EXPECT_TRUE(connection->isOk()) << connection->errorMessage();
        EXPECT_TRUE(usePrivateSchema(*connection));

        return connection;
    }

    std::string queryText(const std::string& sql)
    {
        const auto result = m_connection->execute(sql.c_str());

        EXPECT_TRUE(result.isOk()) << sql << ": " << result.errorMessage();

        if (!result.isOk() || result.rowCount() != 1 || result.isNull(0, 0)) {
            return {};
        }

        return result.value(0, 0);
    }

    std::vector<std::string> queryColumn(const std::string& sql)
    {
        const auto result = m_connection->execute(sql.c_str());

        EXPECT_TRUE(result.isOk()) << sql << ": " << result.errorMessage();

        std::vector<std::string> values;

        for (int row = 0; result.isOk() && row < result.rowCount(); ++row) {
            values.emplace_back(result.isNull(row, 0) ? "NULL" : result.value(row, 0));
        }

        return values;
    }

    std::vector<std::string> appliedVersions()
    {
        return queryColumn("SELECT version FROM schema_migrations ORDER BY version");
    }

    bool tableExists(const std::string& table)
    {
        return queryText("SELECT to_regclass('" + m_schema + "." + table + "') IS NOT NULL") == "t";
    }

    std::vector<std::string> measurementColumns()
    {
        return queryColumn(
            "SELECT column_name || ' ' || data_type || ' ' || is_nullable || ' ' || "
            "coalesce(column_default, '-') "
            "FROM information_schema.columns "
            "WHERE table_schema = '" + m_schema + "' AND table_name = 'measurements' "
            "ORDER BY ordinal_position");
    }

    std::unique_ptr<PostgresConnection> m_connection;
    std::string m_schema;
    bool m_schemaCreated = false;
    bool m_publicSchemaMigrationsExisted = false;
};

const std::vector<std::string> expectedMeasurementColumns{
    "id bigint NO -",
    "measured_at timestamp with time zone NO -",
    "pv_power double precision YES -",
    "grid_power double precision YES -",
    "battery_power double precision YES -",
    "load_power double precision YES -",
    "battery_soc double precision YES -",
    "device_id text NO -",
    "grid_voltage double precision YES -",
    "grid_relay_on boolean YES -",
};

const std::vector<std::string> allVersions{"1", "2", "3", "4"};

// measurements as created before device_id was introduced (commit 7bf6053).
const char* const originalMeasurementsSql =
    "CREATE TABLE measurements ("
    "    id              bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,"
    "    measured_at     timestamptz NOT NULL,"
    "    pv_power        double precision,"
    "    grid_power      double precision,"
    "    battery_power   double precision,"
    "    load_power      double precision,"
    "    battery_soc     double precision"
    ")";

const char* const addDeviceIdSql =
    "ALTER TABLE measurements ADD COLUMN device_id text NOT NULL DEFAULT 'unknown';"
    "ALTER TABLE measurements ALTER COLUMN device_id DROP DEFAULT";

} // namespace

TEST_F(MigrationRunnerTest, CleanDatabaseAppliesAllMigrations)
{
    MigrationRunner runner(*m_connection, database::allMigrations());

    const auto result = runner.run();

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.appliedCount, 4);
    EXPECT_EQ(result.currentVersion, 4);
    EXPECT_EQ(appliedVersions(), allVersions);
    EXPECT_EQ(
        queryColumn("SELECT name FROM schema_migrations ORDER BY version"),
        (std::vector<std::string>{
            "create_measurements", "add_measurements_device_id", "add_measurements_grid_voltage",
            "add_measurements_grid_relay_on"}));
    EXPECT_EQ(measurementColumns(), expectedMeasurementColumns);

    ASSERT_TRUE(execute(
        "INSERT INTO measurements (device_id, measured_at, grid_voltage, grid_relay_on) "
        "VALUES ('device-a', now(), 230.4, true), ('device-b', now(), NULL, NULL)"));
    EXPECT_EQ(
        queryColumn("SELECT grid_voltage FROM measurements ORDER BY id"),
        (std::vector<std::string>{"230.4", "NULL"}));
    EXPECT_EQ(
        queryColumn("SELECT grid_relay_on FROM measurements ORDER BY id"),
        (std::vector<std::string>{"t", "NULL"}));
}

TEST_F(MigrationRunnerTest, UpgradesSchemaWithoutGridRelayOn)
{
    const auto& migrations = database::allMigrations();

    ASSERT_TRUE(
        MigrationRunner(*m_connection, {migrations[0], migrations[1], migrations[2]}).run().ok);
    ASSERT_TRUE(execute(
        "INSERT INTO measurements (device_id, measured_at, battery_soc, grid_voltage) "
        "VALUES ('deye-esp32-001', '2026-01-01T00:00:00Z', 55, 230.1)"));

    const auto result = MigrationRunner(*m_connection, migrations).run();

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.appliedCount, 1);
    EXPECT_EQ(result.currentVersion, 4);
    EXPECT_EQ(appliedVersions(), allVersions);
    EXPECT_EQ(measurementColumns(), expectedMeasurementColumns);
    EXPECT_EQ(
        queryColumn("SELECT device_id || ' ' || grid_voltage::text || ' ' || "
                    "coalesce(grid_relay_on::text, 'NULL') FROM measurements"),
        (std::vector<std::string>{"deye-esp32-001 230.1 NULL"}));
}

TEST_F(MigrationRunnerTest, UpgradesOriginalSchemaWithoutDeviceId)
{
    ASSERT_TRUE(execute(originalMeasurementsSql));
    ASSERT_TRUE(execute(
        "INSERT INTO measurements (measured_at, pv_power) "
        "VALUES ('2026-01-01T00:00:00Z', 100), ('2026-01-01T00:01:00Z', 200)"));

    MigrationRunner runner(*m_connection, database::allMigrations());

    const auto result = runner.run();

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.appliedCount, 4);
    EXPECT_EQ(appliedVersions(), allVersions);
    EXPECT_EQ(measurementColumns(), expectedMeasurementColumns);
    EXPECT_EQ(
        queryColumn("SELECT device_id FROM measurements ORDER BY id"),
        (std::vector<std::string>{"unknown", "unknown"}));
    EXPECT_EQ(
        queryColumn("SELECT grid_voltage FROM measurements ORDER BY id"),
        (std::vector<std::string>{"NULL", "NULL"}));
}

TEST_F(MigrationRunnerTest, UpgradesSchemaWithoutGridVoltage)
{
    ASSERT_TRUE(execute(originalMeasurementsSql));
    ASSERT_TRUE(execute(addDeviceIdSql));
    ASSERT_TRUE(execute(
        "INSERT INTO measurements (device_id, measured_at, battery_soc) "
        "VALUES ('deye-esp32-001', '2026-01-01T00:00:00Z', 55)"));

    MigrationRunner runner(*m_connection, database::allMigrations());

    const auto result = runner.run();

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.appliedCount, 4);
    EXPECT_EQ(appliedVersions(), allVersions);
    EXPECT_EQ(measurementColumns(), expectedMeasurementColumns);
    EXPECT_EQ(
        queryColumn("SELECT device_id || ' ' || coalesce(grid_voltage::text, 'NULL') "
                    "FROM measurements"),
        (std::vector<std::string>{"deye-esp32-001 NULL"}));
}

TEST_F(MigrationRunnerTest, PreservesExistingMeasurementValues)
{
    ASSERT_TRUE(execute(originalMeasurementsSql));
    ASSERT_TRUE(execute(addDeviceIdSql));
    ASSERT_TRUE(execute(
        "INSERT INTO measurements "
        "(device_id, measured_at, pv_power, grid_power, battery_power, load_power, battery_soc) "
        "VALUES "
        "('deye-esp32-001', '2026-01-01T00:00:00Z', 1200.5, -300, 0, 900.25, 87), "
        "('deye-esp32-001', '2026-01-01T00:01:00Z', NULL, 15, NULL, 410, NULL), "
        "('simulator-001', '2026-01-01T00:02:00Z', 0, 0, -50, 0, 100)"));

    const std::string snapshotSql =
        "SELECT concat_ws('|', id, device_id, "
        "to_char(measured_at AT TIME ZONE 'UTC', 'YYYY-MM-DD HH24:MI:SS'), "
        "coalesce(pv_power::text, 'NULL'), coalesce(grid_power::text, 'NULL'), "
        "coalesce(battery_power::text, 'NULL'), coalesce(load_power::text, 'NULL'), "
        "coalesce(battery_soc::text, 'NULL')) "
        "FROM measurements ORDER BY id";

    const auto before = queryColumn(snapshotSql);
    ASSERT_EQ(before.size(), 3u);

    MigrationRunner runner(*m_connection, database::allMigrations());

    const auto result = runner.run();

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(queryColumn(snapshotSql), before);
    EXPECT_EQ(queryText("SELECT count(*) FROM measurements WHERE grid_voltage IS NULL"), "3");
    EXPECT_EQ(queryText("SELECT count(*) FROM measurements WHERE grid_relay_on IS NULL"), "3");
}

TEST_F(MigrationRunnerTest, RepeatedRunDoesNotReapplyMigrations)
{
    ASSERT_TRUE(MigrationRunner(*m_connection, database::allMigrations()).run().ok);

    const std::string historySql =
        "SELECT version || ' ' || name || ' ' || applied_at::text "
        "FROM schema_migrations ORDER BY version";
    const auto historyBefore = queryColumn(historySql);

    const auto result = MigrationRunner(*m_connection, database::allMigrations()).run();

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.appliedCount, 0);
    EXPECT_EQ(result.currentVersion, 4);
    EXPECT_EQ(queryColumn(historySql), historyBefore);
    EXPECT_EQ(measurementColumns(), expectedMeasurementColumns);
}

TEST_F(MigrationRunnerTest, FailedMigrationIsRolledBackAndStopsLaterMigrations)
{
    const std::vector<Migration> failing{
        {1, "create_first", "CREATE TABLE first_table (id integer)"},
        {2, "failing_migration",
         "CREATE TABLE partial_table (id integer); SELECT missing_column FROM missing_table"},
        {3, "create_third", "CREATE TABLE third_table (id integer)"},
    };

    const auto result = MigrationRunner(*m_connection, failing).run();

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.appliedCount, 1);
    EXPECT_NE(result.error.find("Migration 2 (failing_migration)"), std::string::npos)
        << result.error;
    EXPECT_NE(result.error.find("missing_table"), std::string::npos) << result.error;
    EXPECT_EQ(appliedVersions(), (std::vector<std::string>{"1"}));
    EXPECT_TRUE(tableExists("first_table"));
    EXPECT_FALSE(tableExists("partial_table"));
    EXPECT_FALSE(tableExists("third_table"));
    EXPECT_EQ(
        queryText("SELECT xact_start = query_start FROM pg_stat_activity "
                  "WHERE pid = pg_backend_pid()"),
        "t")
        << "Connection should be outside of a transaction block";

    auto fixed = failing;
    fixed[1].sql = "CREATE TABLE partial_table (id integer)";

    const auto retry = MigrationRunner(*m_connection, fixed).run();

    ASSERT_TRUE(retry.ok) << retry.error;
    EXPECT_EQ(retry.appliedCount, 2);
    EXPECT_EQ(appliedVersions(), (std::vector<std::string>{"1", "2", "3"}));
    EXPECT_TRUE(tableExists("partial_table"));
    EXPECT_TRUE(tableExists("third_table"));
}

TEST_F(MigrationRunnerTest, RejectsUnknownHigherVersion)
{
    ASSERT_TRUE(MigrationRunner(*m_connection, database::allMigrations()).run().ok);
    ASSERT_TRUE(execute("INSERT INTO schema_migrations (version, name) VALUES (999, 'future')"));

    const std::vector<Migration> withNext{
        database::allMigrations()[0],
        database::allMigrations()[1],
        database::allMigrations()[2],
        database::allMigrations()[3],
        {5, "create_not_expected", "CREATE TABLE not_expected (id integer)"},
    };

    const auto result = MigrationRunner(*m_connection, withNext).run();

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("version 999 is not known"), std::string::npos) << result.error;
    EXPECT_FALSE(tableExists("not_expected"));
    EXPECT_EQ(
        appliedVersions(), (std::vector<std::string>{"1", "2", "3", "4", "999"}));

    database::DatabaseSchema schema(*m_connection);

    EXPECT_FALSE(schema.initialize());
    EXPECT_NE(schema.errorMessage().find("version 999"), std::string::npos)
        << schema.errorMessage();
}

TEST_F(MigrationRunnerTest, RejectsGapInMigrationHistory)
{
    ASSERT_TRUE(execute(
        "CREATE TABLE schema_migrations (version integer PRIMARY KEY, name text NOT NULL, "
        "applied_at timestamptz NOT NULL DEFAULT now())"));
    ASSERT_TRUE(execute(
        "INSERT INTO schema_migrations (version, name) "
        "VALUES (1, 'create_measurements'), (3, 'add_measurements_grid_voltage')"));

    const auto result = MigrationRunner(*m_connection, database::allMigrations()).run();

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("gap: version 2"), std::string::npos) << result.error;
    EXPECT_FALSE(tableExists("measurements"));
    EXPECT_EQ(appliedVersions(), (std::vector<std::string>{"1", "3"}));
}

TEST_F(MigrationRunnerTest, RejectsInvalidMigrationListBeforeTouchingDatabase)
{
    const std::vector<std::pair<std::vector<Migration>, std::string>> cases{
        {{{0, "zero", "SELECT 1"}}, "must be positive"},
        {{{-1, "negative", "SELECT 1"}}, "must be positive"},
        {{{1, "a", "SELECT 1"}, {1, "b", "SELECT 1"}}, "strictly increasing"},
        {{{2, "a", "SELECT 1"}, {1, "b", "SELECT 1"}}, "strictly increasing"},
        {{{1, "", "SELECT 1"}}, "empty name"},
        {{{1, "no_sql", ""}}, "empty SQL"},
    };

    for (const auto& [migrations, expectedError] : cases) {
        EXPECT_NE(MigrationRunner::validate(migrations).find(expectedError), std::string::npos)
            << expectedError;

        const auto result = MigrationRunner(*m_connection, migrations).run();

        EXPECT_FALSE(result.ok);
        EXPECT_NE(result.error.find("Invalid migration list"), std::string::npos) << result.error;
        EXPECT_FALSE(tableExists("schema_migrations"));
    }

    EXPECT_TRUE(MigrationRunner::validate(database::allMigrations()).empty());
}

TEST_F(MigrationRunnerTest, AppliesMigrationsInAscendingOrderAndOnlyPendingOnes)
{
    const std::vector<Migration> migrations{
        {1, "create_log", "CREATE TABLE run_log (id serial PRIMARY KEY, version integer)"},
        {5, "log_five", "INSERT INTO run_log (version) VALUES (5)"},
        {10, "log_ten", "INSERT INTO run_log (version) VALUES (10)"},
    };

    const auto first = MigrationRunner(*m_connection, {migrations[0], migrations[1]}).run();

    ASSERT_TRUE(first.ok) << first.error;
    EXPECT_EQ(first.currentVersion, 5);

    const auto second = MigrationRunner(*m_connection, migrations).run();

    ASSERT_TRUE(second.ok) << second.error;
    EXPECT_EQ(second.appliedCount, 1);
    EXPECT_EQ(second.currentVersion, 10);

    const auto third = MigrationRunner(*m_connection, migrations).run();

    ASSERT_TRUE(third.ok) << third.error;
    EXPECT_EQ(third.appliedCount, 0);
    EXPECT_EQ(
        queryColumn("SELECT version FROM run_log ORDER BY id"),
        (std::vector<std::string>{"5", "10"}));
    EXPECT_EQ(appliedVersions(), (std::vector<std::string>{"1", "5", "10"}));
}

TEST_F(MigrationRunnerTest, ReportsLockTimeoutWhileAnotherRunnerHoldsLock)
{
    auto holder = openPrivateConnection();

    ASSERT_TRUE(execute(*holder, "BEGIN"));
    ASSERT_TRUE(execute(
        *holder,
        "SELECT pg_advisory_xact_lock(" + std::to_string(MigrationRunner::advisoryLockKey) + ")"));

    const auto blocked = MigrationRunner(
        *m_connection, database::allMigrations(), std::chrono::milliseconds{200}).run();

    EXPECT_FALSE(blocked.ok);
    EXPECT_NE(blocked.error.find("Migration 1 (create_measurements)"), std::string::npos)
        << blocked.error;
    EXPECT_NE(blocked.error.find("migration lock"), std::string::npos) << blocked.error;
    EXPECT_NE(blocked.error.find("lock timeout"), std::string::npos) << blocked.error;
    EXPECT_FALSE(tableExists("schema_migrations"));
    EXPECT_FALSE(tableExists("measurements"));

    ASSERT_TRUE(execute(*holder, "ROLLBACK"));

    const auto released = MigrationRunner(
        *m_connection, database::allMigrations(), std::chrono::milliseconds{200}).run();

    ASSERT_TRUE(released.ok) << released.error;
    EXPECT_EQ(appliedVersions(), allVersions);
}

TEST_F(MigrationRunnerTest, ConcurrentRunnersApplyEachMigrationOnce)
{
    const std::vector<Migration> migrations{
        {1, "create_log", "CREATE TABLE run_log (version integer)"},
        {2, "slow_insert", "SELECT pg_sleep(0.5); INSERT INTO run_log VALUES (2)"},
        {3, "insert", "INSERT INTO run_log VALUES (3)"},
    };

    constexpr int runnerCount = 3;

    std::vector<std::unique_ptr<PostgresConnection>> connections;
    for (int i = 0; i < runnerCount; ++i) {
        connections.push_back(openPrivateConnection());
    }

    std::vector<database::MigrationResult> results(runnerCount);
    std::atomic_bool start{false};
    std::vector<std::thread> threads;

    for (int i = 0; i < runnerCount; ++i) {
        threads.emplace_back([&, i] {
            while (!start) {
                std::this_thread::yield();
            }
            results[i] = MigrationRunner(*connections[i], migrations).run();
        });
    }

    start = true;

    for (auto& thread : threads) {
        thread.join();
    }

    int appliedTotal = 0;
    for (const auto& result : results) {
        EXPECT_TRUE(result.ok) << result.error;
        EXPECT_EQ(result.currentVersion, 3);
        appliedTotal += result.appliedCount;
    }

    EXPECT_EQ(appliedTotal, 3);
    EXPECT_EQ(appliedVersions(), (std::vector<std::string>{"1", "2", "3"}));
    EXPECT_EQ(
        queryColumn("SELECT version FROM run_log ORDER BY version"),
        (std::vector<std::string>{"2", "3"}));
}

TEST_F(MigrationRunnerTest, DatabaseSchemaInitializesPrivateSchema)
{
    database::DatabaseSchema schema(*m_connection);

    ASSERT_TRUE(schema.initialize()) << schema.errorMessage();
    EXPECT_TRUE(schema.errorMessage().empty());
    EXPECT_EQ(appliedVersions(), allVersions);
    EXPECT_EQ(measurementColumns(), expectedMeasurementColumns);

    ASSERT_TRUE(schema.initialize()) << schema.errorMessage();
    EXPECT_EQ(appliedVersions(), allVersions);
}
