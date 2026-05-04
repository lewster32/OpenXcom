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
#include <cstdint>

namespace OpenXcom
{
namespace LightingHash
{

// Boost-style hash combiner. Same inputs always yield the same output;
// outputs are uniformly distributed across the 32-bit range for the
// small-integer inputs we feed in (tile coords + part index, or a
// BattleItem id padded with zeros). Used to make the per-source
// litChance roll deterministic for the duration of a battle.
inline std::uint32_t mixStep(std::uint32_t h, int v)
{
    h ^= static_cast<std::uint32_t>(v) + 0x9E3779B9u + (h << 6) + (h >> 2);
    return h;
}

inline std::uint32_t mix(int a, int b, int c, int d)
{
    std::uint32_t h = 0;
    h = mixStep(h, a);
    h = mixStep(h, b);
    h = mixStep(h, c);
    h = mixStep(h, d);
    return h;
}

// Returns true if a source with this litChance should be considered lit
// for the instance whose stable hash is `hash`. Default litChance >= 1.0
// short-circuits so the hot path stays the same as legacy code.
inline bool passes(double litChance, std::uint32_t hash)
{
    if (litChance >= 1.0) return true;
    if (litChance <= 0.0) return false;
    const double r = static_cast<double>(hash & 0xFFFFFFu) / static_cast<double>(0x1000000u);
    return r < litChance;
}

}
}
