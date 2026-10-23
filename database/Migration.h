#pragma once

#include <string>

namespace database {

struct Migration
{
    int version;
    std::string name;
    std::string sql;
};

} // namespace database
