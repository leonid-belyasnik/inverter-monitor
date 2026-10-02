#include "WebSocketServer.h"

#include <utility>

#include <boost/asio.hpp>
#include <boost/beast/websocket.hpp>

#include "WebSocketSession.h"

namespace api {

WebSocketServer::WebSocketServer(
    const char* address,
    unsigned short port)
    : m_ioContext()
    , m_acceptor(
        m_ioContext,
        boost::asio::ip::tcp::endpoint{
            boost::asio::ip::make_address(address),
            port
        })
    , m_address(address)
    , m_port(port)
{
}

void WebSocketServer::run()
{
    accept();

    m_ioContext.run();
}

void WebSocketServer::accept()
{
    m_acceptor.async_accept(
        [this](boost::system::error_code error,
               boost::asio::ip::tcp::socket socket) {
            if (!error) {
                std::make_shared<WebSocketSession>(
                    std::move(socket)
                )->start();
            }

            accept();
        });
}

}
