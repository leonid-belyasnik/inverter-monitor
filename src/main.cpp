#include <iostream>

#include <libpq-fe.h>

#include "PostgresConnection.h"
#include "PostgresResult.h"

int main()
{
    PostgresConnection connection(
        "host=localhost port=5432 dbname=inverter user=inverter password=inverter_dev"
    );

    if (!connection.isOk())
    {
        std::cerr << "Connection failed: "
                  << connection.errorMessage();
        return 1;
    }

    std::cout << "Connected to PostgreSQL\n";

    auto result = connection.execute("SELECT 1");
    
    if (!result.isOk())
    {
        std::cerr << "Query failed: "
                  << result.errorMessage();
        return 1;
    }
    
    std::cout << "Result: " << PQgetvalue(result.get(), 0, 0) << '\n';

    return 0;
}
