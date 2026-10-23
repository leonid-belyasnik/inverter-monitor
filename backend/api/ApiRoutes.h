/******************************************************************************
**
** File      ApiRoutes.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

namespace application {

class MeasurementQueryService;

}

namespace api {

class HttpRouter;

class ApiRoutes
{
  public:
	static void registerRoutes(HttpRouter &router,
	                           application::MeasurementQueryService &queryService);
};

} // namespace api
