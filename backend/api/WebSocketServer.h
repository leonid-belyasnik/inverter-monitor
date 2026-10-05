#pragma once

#include <memory>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace application {
struct IngestedMeasurement;
}

namespace api {
class WebSocketSession;

class WebSocketServer
{
  public:
	WebSocketServer(const char *address, unsigned short port);

	void run();

	void publish(const application::IngestedMeasurement &measurement);

  private:
	void accept();
	void publishOnContext(const application::IngestedMeasurement &measurement);

	boost::asio::io_context m_ioContext;
	boost::asio::ip::tcp::acceptor m_acceptor;
	const char *m_address;
	unsigned short m_port;
	std::vector<std::weak_ptr<WebSocketSession>> m_sessions;
};

} // namespace api
