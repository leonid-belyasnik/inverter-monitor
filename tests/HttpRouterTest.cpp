#include <gtest/gtest.h>

#include <boost/beast/http.hpp>

#include <api/HttpRouter.h>

TEST(HttpRouterTest, ReturnsHealthCheck)
{
	api::HttpRouter router;

	router.get("/api/v1/health", [](const api::HttpRouter::Request &request,
	                                const api::HttpRouter::Parameters &,
	                                const api::HttpRouter::QueryParameters &) {
		api::HttpRouter::Response response{boost::beast::http::status::ok, request.version()};
		response.body() = "OK\n";
		return response;
	});

	api::HttpRouter::Request request{boost::beast::http::verb::get, "/api/v1/health", 11};

	const auto response = router.handle(request);

	EXPECT_EQ(response.result(), boost::beast::http::status::ok);
	EXPECT_EQ(response.body(), "OK\n");
}

TEST(HttpRouterTest, ExtractsPathParameter)
{
	api::HttpRouter router;

	router.get(
	    "/api/v1/devices/{deviceId}/measurements",
	    [](const api::HttpRouter::Request &request, const api::HttpRouter::Parameters &parameters,
	       const api::HttpRouter::QueryParameters &) {
		    api::HttpRouter::Response response{boost::beast::http::status::ok, request.version()};

		    response.body() = parameters.at("deviceId");
		    return response;
	    });

	api::HttpRouter::Request request{boost::beast::http::verb::get,
	                                 "/api/v1/devices/simulator-001/measurements", 11};

	const auto response = router.handle(request);

	EXPECT_EQ(response.result(), boost::beast::http::status::ok);
	EXPECT_EQ(response.body(), "simulator-001");
}

TEST(HttpRouterTest, ExtractsQueryParameter)
{
	api::HttpRouter router;

	router.get(
	    "/api/v1/devices/{deviceId}/measurements",
	    [](const api::HttpRouter::Request &request, const api::HttpRouter::Parameters &parameters,
	       const api::HttpRouter::QueryParameters &queryParameters) {
		    api::HttpRouter::Response response{boost::beast::http::status::ok, request.version()};

		    response.body() = parameters.at("deviceId") + ":" + queryParameters.at("limit");

		    return response;
	    });

	api::HttpRouter::Request request{boost::beast::http::verb::get,
	                                 "/api/v1/devices/simulator-001/measurements?limit=3", 11};

	const auto response = router.handle(request);

	EXPECT_EQ(response.result(), boost::beast::http::status::ok);
	EXPECT_EQ(response.body(), "simulator-001:3");
}

TEST(HttpRouterTest, ReturnsNotFoundForUnknownRoute)
{
	api::HttpRouter router;

	api::HttpRouter::Request request{boost::beast::http::verb::get, "/api/v1/unknown", 11};

	const auto response = router.handle(request);

	EXPECT_EQ(response.result(), boost::beast::http::status::not_found);
}
