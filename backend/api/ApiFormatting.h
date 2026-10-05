#pragma once

#include <chrono>
#include <string>

namespace api {

std::string formatTimestamp(
    const std::chrono::system_clock::time_point& timePoint);

}
