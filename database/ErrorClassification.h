#pragma once

#include <chrono>
#include <string_view>

namespace database {

// Transient errors may succeed when the whole operation is repeated later.
// Anything not known to be transient is permanent.
enum class ErrorKind { Transient, Permanent };

const char* toString(ErrorKind kind);

// Classifies a SQLSTATE reported by the server. An empty SQLSTATE is permanent.
ErrorKind classifySqlState(std::string_view sqlState);

// Classifies a failed statement. A lost connection is transient regardless of
// the SQLSTATE, because the statement may not have reached the server.
ErrorKind classifyQueryFailure(std::string_view sqlState, bool connectionLost);

// libpq reports no SQLSTATE for a failed connection attempt; these are the
// structured facts it does provide.
struct ConnectionFailureFacts
{
    bool invalidParameters = false; // connection parameters could not be parsed
    bool passwordNeeded = false;    // PQconnectionNeedsPassword()
};

// Invalid parameters and a required but missing password are permanent. Any
// other connection failure is treated as transient and stays bounded by the
// startup retry policy. PQconnectionUsedPassword() is deliberately not an
// input: it only says that the server asked for a password, not that the
// password was rejected, so it cannot separate a wrong password from a
// failure after successful authentication (for example 53300 or 57P01).
ErrorKind classifyConnectionFailure(const ConnectionFailureFacts& facts);

struct StartupRetryPolicy
{
    int maxAttempts = 10;
    std::chrono::milliseconds deadline{60000};
    std::chrono::milliseconds initialDelay{1000};
    std::chrono::milliseconds maxDelay{10000};
};

enum class RetryVerdict { Retry, PermanentFailure, AttemptsExhausted, DeadlineExceeded };

struct RetryDecision
{
    RetryVerdict verdict = RetryVerdict::PermanentFailure;
    std::chrono::milliseconds delay{0};
};

// Delay after the given number of failed attempts (1-based): initialDelay
// doubled per attempt, capped at maxDelay.
std::chrono::milliseconds backoffDelay(const StartupRetryPolicy& policy, int failedAttempts);

// Decides whether to start another attempt after a failure. elapsed is the
// time since the first attempt started. A retry is allowed only if the next
// attempt would start before the deadline; a running attempt is not bounded.
RetryDecision decideStartupRetry(
    const StartupRetryPolicy& policy,
    int failedAttempts,
    std::chrono::milliseconds elapsed,
    ErrorKind kind
);

} // namespace database
