#include "ApiRoutes.h"

#include <boost/json.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include <application/MeasurementQueryService.h>

#include "HttpRouter.h"

namespace {

std::string formatTimestamp(
    const std::chrono::system_clock::time_point& timePoint)
{
    const auto time = std::chrono::system_clock::to_time_t(timePoint);

    std::tm tm{};
    gmtime_r(&time, &tm);

    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            timePoint.time_since_epoch()) %
        std::chrono::seconds(1);

    std::ostringstream stream;

    stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
           << '.'
           << std::setfill('0')
           << std::setw(3)
           << milliseconds.count()
           << 'Z';

    return stream.str();
}

} // namespace

namespace api {

void ApiRoutes::registerRoutes(
    HttpRouter& router,
    application::MeasurementQueryService& queryService)
{
    router.get(
        "/api/v1/health",
        [](const HttpRouter::Request& request,
            const HttpRouter::Parameters& parameters,
            const HttpRouter::QueryParameters& queryParameters) {
            HttpRouter::Response response{
                boost::beast::http::status::ok,
                request.version()
            };

            response.set(
                boost::beast::http::field::content_type,
                "text/plain");

            response.body() = "OK\n";

            return response;
        });

    router.get(
        "/api/v1/devices/{deviceId}/measurements",
        [&queryService](
            const HttpRouter::Request &request,
            const HttpRouter::Parameters &parameters,
            const HttpRouter::QueryParameters &queryParameters)
        {
            const auto deviceId = parameters.at("deviceId");

            std::size_t limit = 100;

            const auto limitIt = queryParameters.find("limit");

            if (limitIt != queryParameters.end())
            {
                try
                {
                    const auto requestedLimit =
                        std::stoul(limitIt->second);

                    if (requestedLimit >= 1 &&
                        requestedLimit <= 1000)
                    {
                        limit = requestedLimit;
                    }
                }
                catch (...)
                {
                }
            }

            const auto measurements =
                queryService.findByDevice(deviceId, limit);

            boost::json::array items;

            for (const auto &record : measurements) {
                boost::json::object item;

                item["measured_at"] = ::formatTimestamp(record.measurement.measuredAt);
                item["pv_power"] = record.measurement.pvPower;
                item["grid_power"] = record.measurement.gridPower;
                item["battery_power"] = record.measurement.batteryPower;
                item["load_power"] = record.measurement.loadPower;
                item["battery_soc"] = record.measurement.batterySoc;

                items.push_back(std::move(item));
            }

            boost::json::object body;
            body["device_id"] = deviceId;
            body["measurements"] = std::move(items);

            HttpRouter::Response response{
                boost::beast::http::status::ok,
                request.version()};

            response.set(
                boost::beast::http::field::content_type,
                "application/json");

            response.body() = boost::json::serialize(body);
            response.prepare_payload();

            return response;
        });

    router.get(
        "/",
        [](const HttpRouter::Request& request,
           const HttpRouter::Parameters& parameters,
           const HttpRouter::QueryParameters& queryParameters) {
            HttpRouter::Response response{
                boost::beast::http::status::ok,
                request.version()
            };

            response.set(
                boost::beast::http::field::content_type,
                "text/plain");

            response.body() =
                "Hello from inverter-monitor API\n";

            return response;
        });
}

} // namespace api
