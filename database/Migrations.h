#pragma once

#include <vector>

#include "Migration.h"

namespace database {

// Applied migrations must never be edited; schema changes are added as new versions.
const std::vector<Migration> &allMigrations();

} // namespace database
