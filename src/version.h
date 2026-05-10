#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */

#define MIN_REQUIRED_RULESET_VERSION_NUMBER 8,6,0,0

#define OPENXCOM_VERSION_ENGINE "Extended"
#define OPENXCOM_VERSION_SHORT "Extended 8.6.1 + Realistic Lighting"
#define OPENXCOM_VERSION_LONG "8.6.1.0"
#define OPENXCOM_VERSION_NUMBER 8,6,1,0

// Fork identity, advertised alongside the upstream "Extended" engine in
// ModInfo's supportedEngines table. Mods that depend on Realistic Lighting
// features can pin to this fork via:
//   requiredExtendedEngine: OXCE-RL
//   requiredExtendedVersion: 1.0
// while mods that only need stock OXCE keep working through the "Extended"
// entry. Bump OPENXCOM_FORK_VERSION_NUMBER when a release introduces fields or
// behaviour that older fork builds cannot honour.
#define OPENXCOM_FORK_ENGINE "OXCE-RL"
#define OPENXCOM_FORK_VERSION_NUMBER 1,0,0,0

#ifndef OPENXCOM_VERSION_GIT
#define OPENXCOM_VERSION_GIT " (c477bb375)"
#endif
