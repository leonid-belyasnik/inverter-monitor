#include "HttpServer.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "HttpRouter.h"

namespace api {

HttpServer::HttpServer(
    const char* address,
    unsigned short port,
    HttpRouter& router)
    : m_address(address)
    , m_port(port)
    , m_router(router)
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

    for (;;)
    {
        boost::asio::ip::tcp::socket socket(m_ioContext);

        acceptor.accept(socket);

        boost::beast::flat_buffer buffer;
        boost::beast::http::request<boost::beast::http::string_body> request;

        boost::beast::http::read(
            socket,
            buffer,
            request);

        const auto response = m_router.handle(request);

        boost::beast::http::write(
            socket,
            response);

        boost::system::error_code error;
        socket.shutdown(
            boost::asio::ip::tcp::socket::shutdown_send,
            error);
    }
}

} // namespace api
