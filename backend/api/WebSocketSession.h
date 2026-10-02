#pragma once

#include <memory>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

namespace api {

class WebSocketSession
    : public std::enable_shared_from_this<WebSocketSession>
{
public:
    explicit WebSocketSession(
        boost::asio::ip::tcp::socket socket);

    void start();

private:
    void onAccept(
        boost::system::error_code error);

    void read();

    boost::beast::websocket::stream<
        boost::asio::ip::tcp::socket
    > m_websocket;

    boost::beast::flat_buffer m_buffer;
};

}
