/******************************************************************************
**
** File      WebSocketSession.cpp
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#include "WebSocketSession.h"

#include <iostream>
#include <memory>
#include <utility>

#include <boost/json.hpp>

#include <application/IngestedMeasurement.h>

#include "ApiFormatting.h"
#include "WebSocketProtocol.h"

namespace api {

WebSocketSession::WebSocketSession(boost::asio::ip::tcp::socket socket)
    : m_websocket(std::move(socket))
{
}

void WebSocketSession::start()
{
	m_websocket.async_accept(
	    [self = shared_from_this()](boost::system::error_code error) { self->onAccept(error); });
}

void WebSocketSession::onAccept(boost::system::error_code error)
{
	if (error) {
		return;
	}

	read();
}

void WebSocketSession::read()
{
	m_websocket.async_read(m_buffer, [self = shared_from_this()](boost::system::error_code error,
	                                                             std::size_t bytesTransferred) {
		if (error) {
			return;
		}

		const auto message = boost::beast::buffers_to_string(self->m_buffer.data());

		const auto command = parseSubscribeCommand(message);

		if (command) {
			self->m_subscribedDevice = domain::DeviceId{command->deviceId};

			auto response = std::make_shared<std::string>(
			    std::string(R"({"type":"subscribed","device_id":")") + command->deviceId + R"("})");

			self->m_buffer.consume(bytesTransferred);

			self->m_writeQueue.push_back(response);

			if (!self->m_writeInProgress) {
				self->writeNext();
			}

			self->read();

			return;
		}

		auto response = std::make_shared<std::string>(
		    R"({"type":"error","message":"invalid subscribe command"})");

		self->m_buffer.consume(bytesTransferred);

		self->m_writeQueue.push_back(response);

		if (!self->m_writeInProgress) {
			self->writeNext();
		}

		self->read();
	});
}

void WebSocketSession::writeNext()
{
	if (m_writeQueue.empty()) {
		m_writeInProgress = false;
		return;
	}

	m_writeInProgress = true;

	const auto response = m_writeQueue.front();

	m_websocket.async_write(
	    boost::asio::buffer(*response),
	    [self = shared_from_this(), response](boost::system::error_code error, std::size_t) {
		    if (error) {
			    self->m_writeQueue.clear();
			    self->m_writeInProgress = false;
			    return;
		    }

		    self->m_writeQueue.pop_front();
		    self->writeNext();
	    });
}

void WebSocketSession::publish(const application::IngestedMeasurement &measurement)
{
	if (!m_subscribedDevice) {
		return;
	}

	if (m_subscribedDevice->value != measurement.deviceId.value) {
		return;
	}

	boost::json::object message;
	message["type"] = "measurement";
	message["device_id"] = measurement.deviceId.value;

	message["data"] = formatMeasurement(measurement.measurement);

	auto response = std::make_shared<std::string>(boost::json::serialize(message));

	m_writeQueue.push_back(response);

	if (!m_writeInProgress) {
		writeNext();
	}
}

} // namespace api
