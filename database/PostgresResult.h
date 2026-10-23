#pragma once

#include <memory>
#include <string>

struct pg_result;

namespace database {

using PGresult = struct pg_result;

class PostgresResult
{
  public:
	// fallbackError is reported by errorMessage() when result is null.
	explicit PostgresResult(PGresult *result, std::string fallbackError = {});

	bool isOk() const;

	int rowCount() const;
	int columnCount() const;
	const char *value(int row, int column) const;
	bool isNull(int row, int column) const;

	const char *errorMessage() const;
	// Empty when the server did not report a SQLSTATE.
	std::string sqlState() const;

	PGresult *get() const;

  private:
	struct ResultDeleter
	{
		void operator()(PGresult *result) const;
	};

	using ResultPtr = std::unique_ptr<PGresult, ResultDeleter>;

	ResultPtr m_result;
	std::string m_fallbackError;
};

} // namespace database
