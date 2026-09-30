#include <application/MeasurementQueryService.h>

#include <database/MeasurementRepository.h>
#include <database/PostgresConnection.h>
#include <database/DatabaseSchema.h>

#include "ApiRoutes.h"
#include "HttpRouter.h"
#include "HttpServer.h"

int main()
{
    database::PostgresConnection connection("");

    if (!connection.isOk())
    {
        return 1;
    }

    database::DatabaseSchema schema(connection);

    if (!schema.initialize())
    {
        return 1;
    }

    database::MeasurementRepository repository(connection);

    application::MeasurementQueryService queryService(repository);

    api::HttpRouter router;

    api::ApiRoutes::registerRoutes(
        router,
        queryService);

    api::HttpServer server(
        "0.0.0.0",
        8080,
        router);

    server.run();
}
