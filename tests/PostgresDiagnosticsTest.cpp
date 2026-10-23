/******************************************************************************
**
** File      PostgresDiagnosticsTest.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include <chrono>
#include <memory>
#include <string>
#include <type_traits>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <application/TelemetryIngestionService.h>
#include <database/MeasurementRepository.h>
#include <database/MigrationRunner.h>
#include <database/PostgresConnection.h>
#include <database/PostgresResult.h>

namespace {

static_assert(
    std::is_same_v<decltype(std::declval<const database::PostgresConnection &>().errorMessage()),
                   std::string>,
    "PostgresConnection::errorMessage() must return a copy");

// Holds a bound but non-listening local TCP port, so connections to it are
// refused and no other process can start listening on it during the test.
class ClosedLocalPort
{
  public:
	ClosedLocalPort() : m_socket(::socket(AF_INET, SOCK_STREAM, 0))
	{
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = 0;

		socklen_t length = sizeof(address);

		if (m_socket >= 0 &&
		    ::bind(m_socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0 &&
		    ::getsockname(m_socket, reinterpret_cast<sockaddr *>(&address), &length) == 0) {
			m_port = ntohs(address.sin_port);
		}
	}

	~ClosedLocalPort()
	{
		if (m_socket >= 0) {
			::close(m_socket);
		}
	}

	int port() const
	{
		return m_port;
	}

  private:
	int m_socket;
	int m_port = 0;
};

// Every parameter that could redirect the connection is set explicitly, so
// PG* variables, ~/.pgpass and service files cannot affect the target.
std::string unreachableConnectionString(const int port)
{
	return "host=127.0.0.1 hostaddr=127.0.0.1 port=" + std::to_string(port) +
	       " dbname=diagnostics_test user=diagnostics_test password=unused"
	       " passfile=/dev/null sslmode=disable gssencmode=disable connect_timeout=1";
}

class PostgresDiagnosticsTest : public ::testing::Test
{
  protected:
	void SetUp() override
	{
		ASSERT_NE(m_port.port(), 0) << "Failed to reserve a local port";

		m_connection = std::make_unique<database::PostgresConnection>(
		    unreachableConnectionString(m_port.port()).c_str());

		ASSERT_FALSE(m_connection->isOk());
	}

	ClosedLocalPort m_port;
	std::unique_ptr<database::PostgresConnection> m_connection;
};

domain::Measurement sampleMeasurement()
{
	domain::Measurement measurement;
	measurement.measuredAt = std::chrono::system_clock::now();
	measurement.batteryPower = 100.0;
	measurement.batterySoc = 50.0;

	return measurement;
}

} // namespace

TEST_F(PostgresDiagnosticsTest, FailedConnectionKeepsNonEmptyMessage)
{
	const std::string message = m_connection->errorMessage();

	EXPECT_FALSE(message.empty());
	EXPECT_NE(message.find("127.0.0.1"), std::string::npos) << message;
}

TEST_F(PostgresDiagnosticsTest, ErrorMessageCopyOutlivesConnection)
{
	const std::string message = m_connection->errorMessage();

	m_connection.reset();

	EXPECT_FALSE(message.empty());
}

TEST_F(PostgresDiagnosticsTest, QueryOnFailedConnectionReturnsErrorResult)
{
	const auto result = m_connection->execute("SELECT 1");

	EXPECT_FALSE(result.isOk());
	EXPECT_FALSE(std::string(result.errorMessage()).empty());
	EXPECT_STRNE(result.errorMessage(), "No result");
	EXPECT_TRUE(result.sqlState().empty());
}

TEST_F(PostgresDiagnosticsTest, RepositorySaveReportsErrorWithoutCrashing)
{
	database::MeasurementRepository repository(*m_connection);
	database::DatabaseError error;

	EXPECT_EQ(repository.save(domain::DeviceId{"diagnostics-001"}, sampleMeasurement(), &error), 0);
	EXPECT_FALSE(error.message.empty());

	EXPECT_EQ(repository.save(domain::DeviceId{"diagnostics-001"}, sampleMeasurement()), 0);
}

TEST_F(PostgresDiagnosticsTest, IngestionReportsPersistenceFailureAndSkipsHandler)
{
	database::MeasurementRepository repository(*m_connection);
	bool handlerCalled = false;

	const application::TelemetryIngestionService ingestion(
	    repository,
	    [&handlerCalled](const application::IngestedMeasurement &) { handlerCalled = true; });

	application::IngestionError error;
	const auto measurementId = ingestion.ingest(
	    application::IngestedMeasurement{domain::DeviceId{"diagnostics-001"}, sampleMeasurement()},
	    &error);

	EXPECT_FALSE(measurementId.has_value());
	EXPECT_FALSE(handlerCalled);
	EXPECT_EQ(error.stage, application::IngestionError::Stage::Persistence);
	EXPECT_FALSE(error.message.empty());
}

TEST_F(PostgresDiagnosticsTest, RefusedConnectionIsTransient)
{
	EXPECT_EQ(m_connection->failureKind(), database::ErrorKind::Transient);
}

TEST_F(PostgresDiagnosticsTest, MigrationOnLostConnectionIsTransientWithoutSqlState)
{
	database::MigrationRunner runner(*m_connection, {{1, "first", "SELECT 1"}});

	const auto result = runner.run();

	EXPECT_FALSE(result.ok);
	EXPECT_EQ(result.errorKind, database::ErrorKind::Transient);
	EXPECT_TRUE(result.sqlState.empty());
	EXPECT_FALSE(result.error.empty());
}

TEST_F(PostgresDiagnosticsTest, InvalidMigrationListIsPermanent)
{
	database::MigrationRunner runner(*m_connection,
	                                 {{2, "second", "SELECT 1"}, {1, "first", "SELECT 1"}});

	const auto result = runner.run();

	EXPECT_FALSE(result.ok);
	EXPECT_EQ(result.errorKind, database::ErrorKind::Permanent);
	EXPECT_NE(result.error.find("Invalid migration list"), std::string::npos) << result.error;
}

TEST(PostgresConnectionParametersTest, UnparsableConnectionStringIsPermanent)
{
	const database::PostgresConnection connection("host");

	EXPECT_FALSE(connection.isOk());
	EXPECT_EQ(connection.failureKind(), database::ErrorKind::Permanent);
	EXPECT_FALSE(connection.errorMessage().empty());
}

TEST(PostgresConnectionParametersTest, InvalidConnectTimeoutIsPermanent)
{
	const database::PostgresConnection connection("host=127.0.0.1 connect_timeout=soon");

	EXPECT_FALSE(connection.isOk());
	EXPECT_EQ(connection.failureKind(), database::ErrorKind::Permanent);
	EXPECT_NE(connection.errorMessage().find("connect_timeout"), std::string::npos)
	    << connection.errorMessage();
}
