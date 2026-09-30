#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

namespace api {

class HttpRouter
{
public:
    using Request = boost::beast::http::request<
        boost::beast::http::string_body>;

    using Response = boost::beast::http::response<
        boost::beast::http::string_body>;

    using Parameters = std::unordered_map<std::string, std::string>;
    using QueryParameters = std::unordered_map<std::string, std::string>;

    using Handler = std::function<Response(
        const Request&,
        const Parameters&,
        const QueryParameters&)>;

    void get(std::string path, Handler handler);

    Response handle(const Request& request) const;

private:
    std::unordered_map<std::string, Handler> m_getRoutes;
};

} // namespace api
