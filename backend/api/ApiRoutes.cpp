/******************************************************************************
**
** File      ApiRoutes.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "ApiRoutes.h"

#include <boost/json.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include <application/MeasurementQueryService.h>

#include "ApiFormatting.h"
#include "HttpRouter.h"

namespace api {

void ApiRoutes::registerRoutes(HttpRouter &router,
                               application::MeasurementQueryService &queryService)
{
	router.get("/api/v1/health",
	           [](const HttpRouter::Request &request, const HttpRouter::Parameters &parameters,
	              const HttpRouter::QueryParameters &queryParameters) {
		           HttpRouter::Response response{boost::beast::http::status::ok, request.version()};

		           response.set(boost::beast::http::field::content_type, "text/plain");

		           response.body() = "OK\n";

		           return response;
	           });

	router.get("/api/v1/devices/{deviceId}/measurements",
	           [&queryService](const HttpRouter::Request &request,
	                           const HttpRouter::Parameters &parameters,
	                           const HttpRouter::QueryParameters &queryParameters) {
		           const auto deviceId = parameters.at("deviceId");

		           std::size_t limit = 100;

		           const auto limitIt = queryParameters.find("limit");

		           if (limitIt != queryParameters.end()) {
			           try {
				           const auto requestedLimit = std::stoul(limitIt->second);

				           if (requestedLimit >= 1 && requestedLimit <= 1000) {
					           limit = requestedLimit;
				           }
			           } catch (...) {
			           }
		           }

		           const auto measurements = queryService.findByDevice(deviceId, limit);

		           boost::json::array items;

		           for (const auto &record : measurements) {
			           items.push_back(api::formatMeasurement(record.measurement));
		           }

		           boost::json::object body;
		           body["device_id"] = deviceId;
		           body["measurements"] = std::move(items);

		           HttpRouter::Response response{boost::beast::http::status::ok, request.version()};

		           response.set(boost::beast::http::field::content_type, "application/json");

		           response.body() = boost::json::serialize(body);
		           response.prepare_payload();

		           return response;
	           });

	router.get("/", [](const HttpRouter::Request &request, const HttpRouter::Parameters &parameters,
	                   const HttpRouter::QueryParameters &queryParameters) {
		HttpRouter::Response response{boost::beast::http::status::ok, request.version()};

		response.set(boost::beast::http::field::content_type, "text/plain");

		response.body() = "Hello from inverter-monitor API\n";

		return response;
	});
}

} // namespace api
