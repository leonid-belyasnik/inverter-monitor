#include <iostream>

#include <database/DatabaseSchema.h>
#include <database/PostgresConnection.h>
#include <database/PostgresResult.h>

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

    DatabaseSchema schema(connection);
    
    if (!schema.initialize())
    {
        std::cerr << "Database schema initialization failed\n";
        return 1;
    }
    
    std::cout << "Database schema initialized\n";;

    return 0;
}
