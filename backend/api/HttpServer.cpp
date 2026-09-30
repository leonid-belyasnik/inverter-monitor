#include "HttpServer.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace api {

HttpServer::HttpServer(
    const char* address,
    unsigned short port)
    : m_address(address)
    , m_port(port)
{
}

void HttpServer::run()
{
    boost::asio::ip::tcp::acceptor acceptor(
        m_ioContext,
        {
            boost::asio::ip::make_address(m_address),
            m_port
        });

    boost::asio::ip::tcp::socket socket(m_ioContext);

    acceptor.accept(socket);

    boost::beast::flat_buffer buffer;
    boost::beast::http::request<boost::beast::http::string_body> request;

    boost::beast::http::read(
        socket,
        buffer,
        request);

    boost::beast::http::response<boost::beast::http::string_body> response{
        boost::beast::http::status::ok,
        request.version()
    };

    response.set(
        boost::beast::http::field::content_type,
        "text/plain");

    response.body() = "Hello from inverter-monitor API\n";
    response.prepare_payload();

    boost::beast::http::write(
        socket,
        response);

    boost::system::error_code error;
    socket.shutdown(
        boost::asio::ip::tcp::socket::shutdown_send,
        error);
}

} // namespace api
