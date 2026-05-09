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

// Perceptual gamma applied to the tile shade ONLY when rendering a living
// unit sprite (Map::drawUnit). Tiles and items render at the linear scalar
// (so corpses, dropped weapons, and tile/object surfaces dim correctly with
// distance like physical light), but units get this softer curve so they
// stay visible far from a light source's centre. Lower values brighten
// units further: 1.0 = no softening, 0.5 = sqrt (perceptually justified),
// 0.25 = fourth root, 0.1 = near-uniform unit shading across the lit area
// (in-game-tuned default for tactical visibility).
constexpr double SCALAR_GAMMA = 0.1;

// Boost-style per-step hash combiner. Has weak avalanche on its own when fed
// consecutive small ints, so callers must finish with fmix32 before reading
// raw bits - mix() does this for you.
inline std::uint32_t mixStep(std::uint32_t h, int v)
{
	h ^= static_cast<std::uint32_t>(v) + 0x9E3779B9u + (h << 6) + (h >> 2);
	return h;
}

// Murmur3 fmix32 - finalizer that avalanches every input bit across all
// output bits. Without this, stacked rooms at the same (x, y, part) end up
// correlated >99% across z levels in the bottom-24-bit roll in passes().
inline std::uint32_t fmix32(std::uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85EBCA6Bu;
	h ^= h >> 13;
	h *= 0xC2B2AE35u;
	h ^= h >> 16;
	return h;
}

inline std::uint32_t mix(int a, int b, int c, int d)
{
	std::uint32_t h = 0;
	h = mixStep(h, a);
	h = mixStep(h, b);
	h = mixStep(h, c);
	h = mixStep(h, d);
	return fmix32(h);
}

// Returns true if a source with this litChance should be considered lit
// for the instance whose stable hash is `hash`. Default litChance >= 1.0
// short-circuits the hash compare so always-lit sources stay branch-free.
inline bool passes(double litChance, std::uint32_t hash)
{
	if (litChance >= 1.0) return true;
	if (litChance <= 0.0) return false;
	const double r = static_cast<double>(hash & 0xFFFFFFu) / static_cast<double>(0x1000000u);
	return r < litChance;
}

}
}
