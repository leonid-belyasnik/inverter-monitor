/******************************************************************************
**
** File      Migration.h
** Copyright (c) 2026 Leonid Belyasnik.
**
** Part of the HomePowerGuard project.
**
******************************************************************************/

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
