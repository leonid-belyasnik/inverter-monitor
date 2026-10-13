#include "HttpServer.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>

#include "HttpRouter.h"

namespace api {
namespace http = boost::beast::http;
using ErrorCode = boost::system::error_code;

class HttpSession : public std::enable_shared_from_this<HttpSession>
{
public:
    HttpSession(boost::asio::ip::tcp::socket socket, HttpRouter& router)
        : m_stream(std::move(socket)), m_router(router) {}

    void start() { read(); }

    void stop()
    {
        ErrorCode ignored;
        m_stream.socket().cancel(ignored);
        m_stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
        m_stream.socket().close(ignored);
    }

private:
    void read()
    {
        m_stream.expires_after(std::chrono::seconds(30));
        http::async_read(m_stream, m_buffer, m_request,
            [self = shared_from_this()](ErrorCode error, std::size_t) {
                if (error) return;
                try {
                    self->m_response = std::make_shared<HttpRouter::Response>(
                        self->m_router.handle(self->m_request));
                    self->m_response->keep_alive(self->m_request.keep_alive());
                    self->m_stream.expires_after(std::chrono::seconds(30));
                    http::async_write(self->m_stream, *self->m_response,
                        [self](ErrorCode writeError, std::size_t) {
                            if (writeError || !self->m_response->keep_alive()) {
                                self->stop();
                                return;
                            }
                            self->m_response.reset();
                            self->m_request = {};
                            self->read();
                        });
                } catch (...) {
                    self->stop();
                }
            });
    }

    boost::beast::tcp_stream m_stream;
    boost::beast::flat_buffer m_buffer;
    HttpRouter::Request m_request;
    HttpRouter& m_router;
    std::shared_ptr<HttpRouter::Response> m_response;
};
HttpServer::HttpServer(const char* address, unsigned short port, HttpRouter& router)
    : m_router(router), m_acceptor(m_ioContext), m_address(address), m_port(port) {}

void HttpServer::listen()
{
    if (m_acceptor.is_open()) return;
    const boost::asio::ip::tcp::endpoint endpoint{boost::asio::ip::make_address(m_address), m_port};
    m_acceptor.open(endpoint.protocol());
    m_acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    m_acceptor.bind(endpoint);
    m_acceptor.listen();
}

void HttpServer::run()
{
    listen();
    accept();
    m_ioContext.run();
    if (m_acceptError) std::rethrow_exception(m_acceptError);
}

void HttpServer::stop()
{
    boost::asio::post(m_ioContext, [this] {
        ErrorCode ignored;
        if (m_acceptor.is_open()) {
            m_acceptor.cancel(ignored);
            m_acceptor.close(ignored);
        }
        closeSessions();
    });
}

void HttpServer::closeSessions()
{
    for (auto& weak : m_sessions) if (auto session = weak.lock()) session->stop();
    m_sessions.clear();
}

void HttpServer::accept()
{
    m_acceptor.async_accept([this](ErrorCode error, boost::asio::ip::tcp::socket socket) {
        if (error) {
            if (error != boost::asio::error::operation_aborted && m_acceptor.is_open()) {
                m_acceptError = std::make_exception_ptr(boost::system::system_error(error));
                ErrorCode ignored;
                m_acceptor.close(ignored);
                closeSessions();
            }
            return;
        }

        auto session = std::make_shared<HttpSession>(std::move(socket), m_router);
        m_sessions.erase(std::remove_if(m_sessions.begin(), m_sessions.end(),
            [](const auto& weak) { return weak.expired(); }), m_sessions.end());
        m_sessions.emplace_back(session);
        session->start();
        if (m_acceptor.is_open()) accept();
    });
}

} // namespace api
