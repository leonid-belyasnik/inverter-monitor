#include <chrono>
#include <iostream>

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

    std::cout << "Connected to PostgreSQL\n";

    DatabaseSchema schema(connection);
    
    if (!schema.initialize()) {
        std::cerr << "Database schema initialization failed\n";
        return 1;
    }
    
    std::cout << "Database schema initialized\n";

    MeasurementRepository repository(connection);
    
    const Measurement measurement{
        .measuredAt = std::chrono::system_clock::now(),
        .pvPower = 3200.0,
        .gridPower = -500.0,
        .batteryPower = 2700.0,
        .loadPower = 2200.0,
        .batterySoc = 78.0
    };
    
    const auto measurementId = repository.save(measurement);
    
    if (measurementId == 0) {
        std::cerr << "Failed to save measurement\n";
        return 1;
    }
    
    std::cout << "Measurement saved, id = " << measurementId << '\n';

    return 0;
}
