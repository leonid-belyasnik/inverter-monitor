#pragma once

#include <boost/asio/io_context.hpp>

namespace api {

class HttpServer
{
public:
    HttpServer(const char* address, unsigned short port);

    void run();

private:
    boost::asio::io_context m_ioContext;
    const char* m_address;
    unsigned short m_port;
};

} // namespace api
