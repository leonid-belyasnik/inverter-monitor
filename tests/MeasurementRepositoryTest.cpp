#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include <database/DatabaseSchema.h>
#include <database/MeasurementRepository.h>
#include <database/PostgresConnection.h>
#include <database/PostgresResult.h>
#include <domain/Measurement.h>

int main()
{
    PostgresConnection connection("");

    if (!connection.isOk()) {
        std::cerr << "Connection failed: "
                  << connection.errorMessage();
        return 1;
    }

    DatabaseSchema schema(connection);

    if (!schema.initialize()) {
        std::cerr << "Schema initialization failed\n";
        return 1;
    }

    const auto beginResult = connection.execute("BEGIN");

    if (!beginResult.isOk()) {
        std::cerr << "Failed to begin transaction\n";
        return 1;
    }

    MeasurementRepository repository(connection);

    const Measurement measurement{
        .measuredAt = std::chrono::system_clock::now(),
        .pvPower = 1234.0,
        .gridPower = -100.0,
        .batteryPower = 500.0,
        .loadPower = 1634.0,
        .batterySoc = 75.0
    };

    const auto measurementId = repository.save(measurement);

    if (measurementId == 0) {
        connection.execute("ROLLBACK");
        std::cerr << "Failed to save measurement\n";
        return 1;
    }

    const std::string query =
        "SELECT pv_power, grid_power, battery_power, load_power, battery_soc "
        "FROM measurements "
        "WHERE id = " + std::to_string(measurementId);

    const auto result = connection.execute(query.c_str());

    if (!result.isOk()) {
        connection.execute("ROLLBACK");
        std::cerr << "Failed to read measurement\n";
        return 1;
    }

    if (result.rowCount() != 1) {
        connection.execute("ROLLBACK");
        std::cerr << "Expected exactly one measurement\n";
        return 1;
    }

    if (std::stod(result.value(0, 0)) != measurement.pvPower ||
        std::stod(result.value(0, 1)) != measurement.gridPower ||
        std::stod(result.value(0, 2)) != measurement.batteryPower ||
        std::stod(result.value(0, 3)) != measurement.loadPower ||
        std::stod(result.value(0, 4)) != measurement.batterySoc) {
        connection.execute("ROLLBACK");
        std::cerr << "Stored measurement does not match\n";
        return 1;
    }

    const auto rollbackResult = connection.execute("ROLLBACK");

    if (!rollbackResult.isOk()) {
        std::cerr << "Failed to rollback transaction\n";
        return 1;
    }

    std::cout << "Measurement test passed\n";

    return 0;
}
