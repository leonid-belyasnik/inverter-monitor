#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace api {

class WebSocketServer
{
public:
    WebSocketServer(
        const char* address,
        unsigned short port);

    void run();

private:
    void accept();

    boost::asio::io_context m_ioContext;
    boost::asio::ip::tcp::acceptor m_acceptor;
    const char* m_address;
    unsigned short m_port;
};

}
