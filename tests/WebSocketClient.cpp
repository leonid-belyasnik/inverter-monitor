#include <iostream>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

int main()
{
    boost::asio::io_context ioContext;

    boost::asio::ip::tcp::resolver resolver(ioContext);
    boost::beast::websocket::stream<
        boost::asio::ip::tcp::socket
    > websocket(ioContext);

    const auto endpoints = resolver.resolve(
        "localhost",
        "8081");

    boost::asio::connect(
        websocket.next_layer(),
        endpoints);

    websocket.handshake(
        "localhost",
        "/");

    websocket.write(
    boost::asio::buffer(
        R"({"type":"subscribe","device_id":"simulator-001"})"));

    boost::beast::flat_buffer buffer;

    websocket.read(buffer);

    std::cout
        << boost::beast::buffers_to_string(
               buffer.data())
        << '\n';

    websocket.close(
        boost::beast::websocket::close_code::normal);
}
