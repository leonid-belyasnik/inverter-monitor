#pragma once

namespace application {

class MeasurementQueryService;

}

namespace api {

class HttpRouter;

class ApiRoutes
{
public:
    static void registerRoutes(
        HttpRouter& router,
        application::MeasurementQueryService& queryService);
};

} // namespace api
