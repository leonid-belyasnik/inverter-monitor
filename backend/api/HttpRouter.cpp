/******************************************************************************
**
** File      HttpRouter.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "HttpRouter.h"

#include <vector>

namespace api {

namespace {

std::vector<std::string> splitPath(const std::string &path)
{
	std::vector<std::string> parts;

	std::size_t start = 0;

	while (start < path.size()) {
		const auto separator = path.find('/', start);

		if (separator == std::string::npos) {
			parts.push_back(path.substr(start));
			break;
		}

		if (separator > start) {
			parts.push_back(path.substr(start, separator - start));
		}

		start = separator + 1;
	}

	return parts;
}

bool matchPath(const std::string &route, const std::string &target,
               HttpRouter::Parameters &parameters)
{
	const auto routeParts = splitPath(route);
	const auto targetParts = splitPath(target);

	if (routeParts.size() != targetParts.size()) {
		return false;
	}

	for (std::size_t i = 0; i < routeParts.size(); ++i) {
		const auto &routePart = routeParts[i];
		const auto &targetPart = targetParts[i];

		if (routePart.size() >= 2 && routePart.front() == '{' && routePart.back() == '}') {
			const auto parameterName = routePart.substr(1, routePart.size() - 2);

			parameters.emplace(parameterName, targetPart);

			continue;
		}

		if (routePart != targetPart) {
			return false;
		}
	}

	return true;
}

} // namespace

void HttpRouter::get(std::string path, Handler handler)
{
	m_getRoutes.emplace(std::move(path), std::move(handler));
}

HttpRouter::Response HttpRouter::handle(const Request &request) const
{
	Response response{boost::beast::http::status::not_found, request.version()};

	response.set(boost::beast::http::field::content_type, "text/plain");

	if (request.method() != boost::beast::http::verb::get) {
		response.prepare_payload();
		return response;
	}

	const std::string target(request.target());

	const auto queryPosition = target.find('?');

	const std::string path =
	    queryPosition == std::string::npos ? target : target.substr(0, queryPosition);

	QueryParameters queryParameters;

	if (queryPosition != std::string::npos) {
		const auto query = target.substr(queryPosition + 1);

		const auto separator = query.find('=');

		if (separator != std::string::npos) {
			queryParameters.emplace(query.substr(0, separator), query.substr(separator + 1));
		}
	}

	for (const auto &[route, handler] : m_getRoutes) {
		Parameters parameters;

		if (!matchPath(route, path, parameters)) {
			continue;
		}

		response = handler(request, parameters, queryParameters);

		response.prepare_payload();

		return response;
	}

	response.prepare_payload();
	return response;
}

} // namespace api
