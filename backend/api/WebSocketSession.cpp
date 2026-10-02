#include "WebSocketSession.h"

#include <memory>
#include <utility>

#include "WebSocketProtocol.h"

namespace api {

WebSocketSession::WebSocketSession(
    boost::asio::ip::tcp::socket socket)
    : m_websocket(std::move(socket))
{
}

void WebSocketSession::start()
{
    m_websocket.async_accept(
        [self = shared_from_this()]
        (boost::system::error_code error)
        {
            self->onAccept(error);
        });
}

void WebSocketSession::onAccept(
    boost::system::error_code error)
{
    if (error)
    {
        return;
    }

    read();
}

void WebSocketSession::read()
{
    m_websocket.async_read(
        m_buffer,
        [self = shared_from_this()]
        (boost::system::error_code error,
         std::size_t)
        {
            if (error)
            {
                return;
            }

            const auto message =
                boost::beast::buffers_to_string(
                    self->m_buffer.data());

            const auto command =
                parseSubscribeCommand(message);

            if (command)
            {
                auto response =
                    std::make_shared<std::string>(
                        std::string(
                            R"({"type":"subscribed","device_id":")")
                        + command->deviceId
                        + R"("})");

                self->m_websocket.text(true);

                self->m_websocket.async_write(
                    boost::asio::buffer(*response),
                    [self, response]
                    (boost::system::error_code error,
                     std::size_t)
                    {
                        if (error)
                        {
                            return;
                        }

                        self->m_buffer.consume(
                            self->m_buffer.size());

                        self->read();
                    });

                return;
            }

            self->m_websocket.text(true);

            self->m_websocket.async_write(
                self->m_buffer.data(),
                [self = std::move(self)]
                (boost::system::error_code error,
                 std::size_t)
                {
                    if (error)
                    {
                        return;
                    }

                    self->m_buffer.consume(
                        self->m_buffer.size());

                    self->read();
                });
        });
}

}
