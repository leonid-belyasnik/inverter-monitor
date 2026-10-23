/******************************************************************************
**
** File      Migrations.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

#pragma once

#include <vector>

#include "Migration.h"

namespace database {

// Applied migrations must never be edited; schema changes are added as new versions.
const std::vector<Migration> &allMigrations();

} // namespace database
