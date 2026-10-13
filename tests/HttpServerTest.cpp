#include <atomic>
#include <chrono>
#include <thread>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <gtest/gtest.h>

#include <api/HttpRouter.h>
#include <api/HttpServer.h>

TEST(HttpServerTest, ReportsBindFailureForOccupiedPort)
{
    boost::asio::io_context ioContext;
    boost::asio::ip::tcp::acceptor occupied(
        ioContext,
        {boost::asio::ip::make_address("127.0.0.1"), 0});
    const auto port = occupied.local_endpoint().port();
    api::HttpRouter router;

    api::HttpServer server("127.0.0.1", port, router);
    EXPECT_THROW(server.listen(), boost::system::system_error);
}

TEST(HttpServerTest, StopsRunLoop)
{
    api::HttpRouter router;
    api::HttpServer server("127.0.0.1", 0, router);
    server.listen();
    std::atomic_bool failed{false};

    std::thread thread([&] {
        try {
            server.run();
        } catch (...) {
            failed = true;
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server.stop();
    thread.join();

    EXPECT_FALSE(failed);
}

TEST(HttpServerTest, HandlesKeepAliveAndClosesIdleClientsOnStop)
{
    boost::asio::io_context setupContext;
    boost::asio::ip::tcp::acceptor reservation(
        setupContext, {boost::asio::ip::make_address("127.0.0.1"), 0});
    const auto port = reservation.local_endpoint().port();
    reservation.close();
    api::HttpRouter router;
    api::HttpServer server("127.0.0.1", port, router);
    server.listen();
    std::thread serverThread([&] { server.run(); });

    boost::asio::io_context clientContext;
    boost::asio::ip::tcp::socket socket(clientContext);
    socket.connect({boost::asio::ip::make_address("127.0.0.1"), port});
    boost::beast::flat_buffer buffer;
    for (int i = 0; i < 2; ++i) {
        boost::beast::http::request<boost::beast::http::empty_body> request{
            boost::beast::http::verb::get, "/missing", 11};
        request.set(boost::beast::http::field::host, "localhost");
        if (i == 1) request.keep_alive(false);
        boost::beast::http::write(socket, request);
        boost::beast::http::response<boost::beast::http::string_body> response;
        boost::beast::http::read(socket, buffer, response);
        EXPECT_EQ(response.result(), boost::beast::http::status::not_found);
        EXPECT_EQ(response.keep_alive(), i == 0);
    }

    boost::asio::ip::tcp::socket idleSocket(clientContext);
    idleSocket.connect({boost::asio::ip::make_address("127.0.0.1"), port});
    server.stop();
    serverThread.join();
}
