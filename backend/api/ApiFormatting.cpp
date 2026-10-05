#include "ApiFormatting.h"

#include <iomanip>
#include <sstream>

namespace api {

std::string formatTimestamp(
    const std::chrono::system_clock::time_point& timePoint)
{
    const auto time = std::chrono::system_clock::to_time_t(timePoint);

    std::tm tm{};
    gmtime_r(&time, &tm);

    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            timePoint.time_since_epoch()) %
        std::chrono::seconds(1);

    std::ostringstream stream;

    stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
           << '.'
           << std::setfill('0')
           << std::setw(3)
           << milliseconds.count()
           << 'Z';

    return stream.str();
}

}
