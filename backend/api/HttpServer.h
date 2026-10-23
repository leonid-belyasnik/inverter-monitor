#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <exception>
#include <memory>
#include <vector>

namespace api {

class HttpRouter;
class HttpSession;

class HttpServer
{
  public:
	HttpServer(const char *address, unsigned short port, HttpRouter &router);

	void listen();
	void run();
	void stop();

  private:
	void accept();
	void closeSessions();

	boost::asio::io_context m_ioContext;
	HttpRouter &m_router;
	boost::asio::ip::tcp::acceptor m_acceptor;
	const char *m_address;
	unsigned short m_port;
	std::vector<std::weak_ptr<HttpSession>> m_sessions;
	std::exception_ptr m_acceptError;
};

} // namespace api
