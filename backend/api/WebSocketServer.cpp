#include "WebSocketServer.h"

#include <utility>

#include <boost/asio.hpp>
#include <boost/beast/websocket.hpp>

#include <application/IngestedMeasurement.h>

#include "WebSocketSession.h"

namespace api {

WebSocketServer::WebSocketServer(const char *address, unsigned short port)
    : m_ioContext(),
      m_acceptor(m_ioContext,
                 boost::asio::ip::tcp::endpoint{boost::asio::ip::make_address(address), port}),
      m_address(address), m_port(port)
{
}

void WebSocketServer::run()
{
	accept();

	m_ioContext.run();
}

void WebSocketServer::stop()
{
	m_ioContext.stop();
}

void WebSocketServer::publish(const application::IngestedMeasurement &measurement)
{
	boost::asio::post(m_ioContext, [this, measurement] { publishOnContext(measurement); });
}

void WebSocketServer::accept()
{
	m_acceptor.async_accept(
	    [this](boost::system::error_code error, boost::asio::ip::tcp::socket socket) {
		    if (!error) {
			    auto session = std::make_shared<WebSocketSession>(std::move(socket));

			    m_sessions.push_back(session);

			    session->start();
		    }

		    accept();
	    });
}

void WebSocketServer::publishOnContext(const application::IngestedMeasurement &measurement)
{
	for (auto it = m_sessions.begin(); it != m_sessions.end();) {
		if (auto session = it->lock()) {
			session->publish(measurement);
			++it;
		} else {
			it = m_sessions.erase(it);
		}
	}
}

} // namespace api
