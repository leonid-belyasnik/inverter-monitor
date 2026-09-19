#include <iostream>

#include <libpq-fe.h>

int main()
{
    PGconn* conn = PQconnectdb(
        "host=localhost port=5432 dbname=inverter user=inverter password=inverter_dev"
    );

    if (PQstatus(conn) != CONNECTION_OK)
    {
        std::cerr << "Connection failed: "
                  << PQerrorMessage(conn);
        PQfinish(conn);
        return 1;
    }

    std::cout << "Connected to PostgreSQL\n";

    PGresult* result = PQexec(conn, "SELECT 1");

    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    {
        std::cerr << "Query failed: "
                  << PQerrorMessage(conn);
        PQclear(result);
        PQfinish(conn);
        return 1;
    }

    std::cout << "Result: " << PQgetvalue(result, 0, 0) << '\n';

    PQclear(result);
    PQfinish(conn);

    return 0;
}
