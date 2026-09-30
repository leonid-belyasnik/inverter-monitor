#pragma once

#include <boost/asio/io_context.hpp>

namespace api {

class HttpRouter;

class HttpServer
{
public:
    HttpServer(
        const char* address,
        unsigned short port,
        HttpRouter& router);

    void run();

private:
    boost::asio::io_context m_ioContext;
    const char* m_address;
    unsigned short m_port;
    HttpRouter& m_router;
};

} // namespace api
