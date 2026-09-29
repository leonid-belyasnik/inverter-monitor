#pragma once

#include <string>

namespace domain {

struct DeviceId
{
    std::string value;

    bool operator==(const DeviceId&) const = default;
};

} // namespace domain
