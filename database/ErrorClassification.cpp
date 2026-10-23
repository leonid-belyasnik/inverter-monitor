#include "ErrorClassification.h"

#include <algorithm>
#include <array>

namespace database {

namespace {

constexpr std::array transientSqlStates{
    std::string_view{"57P01"}, // admin_shutdown
    std::string_view{"57P02"}, // crash_shutdown
    std::string_view{"57P03"}, // cannot_connect_now
    std::string_view{"53300"}, // too_many_connections
    std::string_view{"55P03"}, // lock_not_available
    std::string_view{"40001"}, // serialization_failure
    std::string_view{"40P01"}, // deadlock_detected
};

} // namespace

const char *toString(const ErrorKind kind)
{
	return kind == ErrorKind::Transient ? "transient" : "permanent";
}

ErrorKind classifySqlState(const std::string_view sqlState)
{
	if (sqlState.size() != 5) {
		return ErrorKind::Permanent;
	}

	// Class 08: connection exception.
	if (sqlState.starts_with("08")) {
		return ErrorKind::Transient;
	}

	const bool transient = std::find(transientSqlStates.begin(), transientSqlStates.end(),
	                                 sqlState) != transientSqlStates.end();

	return transient ? ErrorKind::Transient : ErrorKind::Permanent;
}

ErrorKind classifyQueryFailure(const std::string_view sqlState, const bool connectionLost)
{
	return connectionLost ? ErrorKind::Transient : classifySqlState(sqlState);
}

ErrorKind classifyConnectionFailure(const ConnectionFailureFacts &facts)
{
	if (facts.invalidParameters || facts.passwordNeeded) {
		return ErrorKind::Permanent;
	}

	return ErrorKind::Transient;
}

std::chrono::milliseconds backoffDelay(const StartupRetryPolicy &policy, const int failedAttempts)
{
	auto delay = policy.initialDelay;

	for (int attempt = 1; attempt < failedAttempts && delay < policy.maxDelay; ++attempt) {
		delay *= 2;
	}

	return std::min(delay, policy.maxDelay);
}

RetryDecision decideStartupRetry(const StartupRetryPolicy &policy, const int failedAttempts,
                                 const std::chrono::milliseconds elapsed, const ErrorKind kind)
{
	if (kind == ErrorKind::Permanent) {
		return {RetryVerdict::PermanentFailure, {}};
	}

	if (failedAttempts >= policy.maxAttempts) {
		return {RetryVerdict::AttemptsExhausted, {}};
	}

	const auto delay = backoffDelay(policy, failedAttempts);

	if (elapsed + delay >= policy.deadline) {
		return {RetryVerdict::DeadlineExceeded, {}};
	}

	return {RetryVerdict::Retry, delay};
}

} // namespace database
