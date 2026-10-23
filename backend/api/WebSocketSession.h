/******************************************************************************
**
** File      WebSocketSession.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <deque>
#include <memory>
#include <optional>
#include <string>

#include <domain/DeviceId.h>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

namespace application {
struct IngestedMeasurement;
}

namespace api {

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession>
{
  public:
	explicit WebSocketSession(boost::asio::ip::tcp::socket socket);

	void start();

	void publish(const application::IngestedMeasurement &measurement);

  private:
	void onAccept(boost::system::error_code error);
	void read();
	void writeNext();

	boost::beast::websocket::stream<boost::asio::ip::tcp::socket> m_websocket;
	boost::beast::flat_buffer m_buffer;
	std::optional<domain::DeviceId> m_subscribedDevice;
	std::deque<std::shared_ptr<std::string>> m_writeQueue;
	bool m_writeInProgress = false;
};

} // namespace api
