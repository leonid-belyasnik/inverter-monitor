#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include <database/ErrorClassification.h>
#include <database/PostgresConnection.h>

using namespace std::chrono_literals;
using database::ErrorKind;
using database::RetryVerdict;

namespace {

class FakeEnvironment
{
public:
    explicit FakeEnvironment(std::map<std::string, std::string> variables = {})
        : m_variables(std::move(variables))
    {
    }

    database::EnvironmentLookup lookup() const
    {
        return [this](const char* name) -> const char* {
            const auto it = m_variables.find(name);
            return it == m_variables.end() ? nullptr : it->second.c_str();
        };
    }

private:
    std::map<std::string, std::string> m_variables;
};

std::optional<std::string> valueOf(const database::ConnectionParameters& parameters,
                                   const std::string& keyword)
{
    std::optional<std::string> found;

    for (const auto& [name, value] : parameters.values) {
        if (name == keyword) {
            EXPECT_FALSE(found.has_value()) << "duplicate " << keyword;
            found = value;
        }
    }

    return found;
}

} // namespace

TEST(ErrorClassificationTest, TransientSqlStates)
{
    for (const char* sqlState : {"08000", "08001", "08006", "57P01", "57P02", "57P03", "53300",
                                 "55P03", "40001", "40P01"}) {
        EXPECT_EQ(database::classifySqlState(sqlState), ErrorKind::Transient) << sqlState;
    }
}

TEST(ErrorClassificationTest, PermanentAndUnknownSqlStates)
{
    for (const char* sqlState : {"", "08", "22P02", "23505", "28P01", "3D000", "42P01", "42601",
                                 "XX000", "53100", "57014", "25006"}) {
        EXPECT_EQ(database::classifySqlState(sqlState), ErrorKind::Permanent) << sqlState;
    }
}

TEST(ErrorClassificationTest, LostConnectionMakesQueryFailureTransient)
{
    EXPECT_EQ(database::classifyQueryFailure("", true), ErrorKind::Transient);
    EXPECT_EQ(database::classifyQueryFailure("42P01", true), ErrorKind::Transient);
    EXPECT_EQ(database::classifyQueryFailure("", false), ErrorKind::Permanent);
    EXPECT_EQ(database::classifyQueryFailure("42P01", false), ErrorKind::Permanent);
    EXPECT_EQ(database::classifyQueryFailure("55P03", false), ErrorKind::Transient);
}

TEST(ErrorClassificationTest, ConnectionFailures)
{
    EXPECT_EQ(database::classifyConnectionFailure({}), ErrorKind::Transient);
    EXPECT_EQ(database::classifyConnectionFailure({.invalidParameters = true}), ErrorKind::Permanent);
    EXPECT_EQ(database::classifyConnectionFailure({.passwordNeeded = true}), ErrorKind::Permanent);
}

TEST(ErrorClassificationTest, ConnectionFailureWithoutMissingPasswordIsTransient)
{
    // A server that asked for a password and then refused the connection
    // (53300, 57P01, a rejected password) is indistinguishable from libpq's
    // public state, so it must stay transient and bounded by the retry policy.
    // Only a missing password or invalid parameters are permanent.
    const database::ConnectionFailureFacts afterAuthentication{
        .invalidParameters = false, .passwordNeeded = false};
    EXPECT_EQ(database::classifyConnectionFailure(afterAuthentication), ErrorKind::Transient);

    const database::StartupRetryPolicy policy;
    const auto decision = database::decideStartupRetry(
        policy, 1, std::chrono::milliseconds(0),
        database::classifyConnectionFailure(afterAuthentication));
    EXPECT_EQ(decision.verdict, database::RetryVerdict::Retry);

    const auto late = database::decideStartupRetry(
        policy, 5, policy.deadline, database::classifyConnectionFailure(afterAuthentication));
    EXPECT_EQ(late.verdict, database::RetryVerdict::DeadlineExceeded);
}

TEST(ConnectionParametersTest, AddsDefaultsWhenNothingIsConfigured)
{
    const FakeEnvironment environment;
    const auto parameters = database::resolveConnectionParameters("", environment.lookup());

    ASSERT_TRUE(parameters.error.empty()) << parameters.error;
    EXPECT_EQ(valueOf(parameters, "connect_timeout"), "5");
    EXPECT_EQ(valueOf(parameters, "keepalives"), "1");
    EXPECT_EQ(valueOf(parameters, "keepalives_idle"), "30");
    EXPECT_EQ(valueOf(parameters, "keepalives_interval"), "10");
    EXPECT_EQ(valueOf(parameters, "keepalives_count"), "3");
    EXPECT_EQ(valueOf(parameters, "tcp_user_timeout"), "60000");
}

TEST(ConnectionParametersTest, ExplicitOptionsWinOverDefaults)
{
    const FakeEnvironment environment({{"PGCONNECT_TIMEOUT", "7"}});
    const auto parameters = database::resolveConnectionParameters(
        "host=db.example port=6543 connect_timeout=12 keepalives_idle=5", environment.lookup());

    ASSERT_TRUE(parameters.error.empty()) << parameters.error;
    EXPECT_EQ(valueOf(parameters, "host"), "db.example");
    EXPECT_EQ(valueOf(parameters, "port"), "6543");
    EXPECT_EQ(valueOf(parameters, "connect_timeout"), "12");
    EXPECT_EQ(valueOf(parameters, "keepalives_idle"), "5");
    EXPECT_EQ(valueOf(parameters, "keepalives_interval"), "10");
}

TEST(ConnectionParametersTest, EnvironmentTimeoutIsLeftToLibpq)
{
    const FakeEnvironment environment({{"PGCONNECT_TIMEOUT", "7 "}});
    const auto parameters = database::resolveConnectionParameters("", environment.lookup());

    ASSERT_TRUE(parameters.error.empty()) << parameters.error;
    EXPECT_FALSE(valueOf(parameters, "connect_timeout").has_value());
    EXPECT_EQ(valueOf(parameters, "keepalives"), "1");
}

TEST(ConnectionParametersTest, RejectsInvalidTimeoutAndConnectionString)
{
    const FakeEnvironment empty;
    const FakeEnvironment invalidTimeout({{"PGCONNECT_TIMEOUT", ""}});

    EXPECT_FALSE(database::resolveConnectionParameters("connect_timeout=5s", empty.lookup()).error.empty());
    EXPECT_FALSE(database::resolveConnectionParameters("", invalidTimeout.lookup()).error.empty());

    const auto unparsable = database::resolveConnectionParameters("host", empty.lookup());
    EXPECT_FALSE(unparsable.error.empty());
    EXPECT_TRUE(unparsable.values.empty());
}

TEST(StartupRetryPolicyTest, BackoffDoublesUpToCap)
{
    const database::StartupRetryPolicy policy;

    EXPECT_EQ(database::backoffDelay(policy, 1), 1s);
    EXPECT_EQ(database::backoffDelay(policy, 2), 2s);
    EXPECT_EQ(database::backoffDelay(policy, 3), 4s);
    EXPECT_EQ(database::backoffDelay(policy, 4), 8s);
    EXPECT_EQ(database::backoffDelay(policy, 5), 10s);
    EXPECT_EQ(database::backoffDelay(policy, 1000), 10s);
}

TEST(StartupRetryPolicyTest, PermanentErrorsAreNotRetried)
{
    const auto decision =
        database::decideStartupRetry(database::StartupRetryPolicy{}, 1, 0ms, ErrorKind::Permanent);

    EXPECT_EQ(decision.verdict, RetryVerdict::PermanentFailure);
}

TEST(StartupRetryPolicyTest, StopsAfterMaxAttempts)
{
    const database::StartupRetryPolicy policy;

    const auto ninth = database::decideStartupRetry(policy, 9, 0ms, ErrorKind::Transient);
    EXPECT_EQ(ninth.verdict, RetryVerdict::Retry);
    EXPECT_EQ(ninth.delay, 10s);

    EXPECT_EQ(database::decideStartupRetry(policy, 10, 0ms, ErrorKind::Transient).verdict,
              RetryVerdict::AttemptsExhausted);
}

TEST(StartupRetryPolicyTest, NextAttemptMustStartBeforeDeadline)
{
    const database::StartupRetryPolicy policy;

    EXPECT_EQ(database::decideStartupRetry(policy, 5, 49s, ErrorKind::Transient).verdict,
              RetryVerdict::Retry);
    EXPECT_EQ(database::decideStartupRetry(policy, 5, 50s, ErrorKind::Transient).verdict,
              RetryVerdict::DeadlineExceeded);
    EXPECT_EQ(database::decideStartupRetry(policy, 1, 59500ms, ErrorKind::Transient).verdict,
              RetryVerdict::DeadlineExceeded);
}

// Simulates a server that stays unreachable, on a virtual clock.
int simulateUnreachableServer(const std::chrono::milliseconds attemptDuration,
                              RetryVerdict& finalVerdict,
                              std::chrono::milliseconds& lastAttemptStart)
{
    const database::StartupRetryPolicy policy;
    std::chrono::milliseconds now{0};

    for (int attempt = 1;; ++attempt) {
        EXPECT_LT(now, policy.deadline);
        lastAttemptStart = now;
        now += attemptDuration;

        const auto decision =
            database::decideStartupRetry(policy, attempt, now, ErrorKind::Transient);

        if (decision.verdict != RetryVerdict::Retry) {
            finalVerdict = decision.verdict;
            return attempt;
        }

        now += decision.delay;
    }
}

TEST(StartupRetryPolicyTest, UnreachableServerStopsWithinBounds)
{
    RetryVerdict verdict{};
    std::chrono::milliseconds lastStart{};

    // Immediate failures (connection refused): 0, 1, 3, 7, 15, 25, 35, 45, 55 s.
    EXPECT_EQ(simulateUnreachableServer(0ms, verdict, lastStart), 9);
    EXPECT_EQ(verdict, RetryVerdict::DeadlineExceeded);
    EXPECT_EQ(lastStart, 55s);

    // Each attempt runs into connect_timeout=5: 0, 6, 13, 22, 35, 50 s.
    EXPECT_EQ(simulateUnreachableServer(5s, verdict, lastStart), 6);
    EXPECT_EQ(verdict, RetryVerdict::DeadlineExceeded);
    EXPECT_EQ(lastStart, 50s);
}
