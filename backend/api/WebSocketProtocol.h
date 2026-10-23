/******************************************************************************
**
** File      WebSocketProtocol.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <optional>
#include <string>

namespace api {

struct SubscribeCommand
{
	std::string deviceId;
};

std::optional<SubscribeCommand> parseSubscribeCommand(const std::string &message);

} // namespace api
