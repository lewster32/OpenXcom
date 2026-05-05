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
#include "TintDither.h"
#include <algorithm>
#include <vector>

namespace OpenXcom
{

namespace
{
	// 4x4 Bayer matrix, ceil-or-floor thresholds (0..15). Without dither, the
	// bilinear's truncate would collapse all 16 sub-cell fractions to the floor;
	// the threshold lets adjacent pixels alternate between floor and ceil based
	// on each pixel's position, producing a stippled gradient.
	const int bayer4[4][4] = {
		{  0,  8,  2, 10 },
		{ 12,  4, 14,  6 },
		{  3, 11,  1,  9 },
		{ 15,  7, 13,  5 }
	};

	// Floyd-Steinberg error scratch, hoisted out of TintDither so we don't
	// allocate 6 std::vector<int> per blit. Sized to the widest sprite seen so
	// far on this thread; only [0, currentWidth) is touched per blit. Zero-fill
	// happens lazily in TintDither's constructor when FS mode is active.
	thread_local std::vector<int> g_errR_cur, g_errG_cur, g_errB_cur;
	thread_local std::vector<int> g_errR_next, g_errG_next, g_errB_next;

	void prepareFsScratch(int width)
	{
		auto prep = [width](std::vector<int>& v) {
			if ((int)v.size() < width) v.resize(width);
			std::fill(v.begin(), v.begin() + width, 0);
		};
		prep(g_errR_cur);
		prep(g_errG_cur);
		prep(g_errB_cur);
		prep(g_errR_next);
		prep(g_errG_next);
		prep(g_errB_next);
	}
}

TintDither::TintDither(int mode, int spriteWidth)
	: _mode(static_cast<Mode>(mode)), _spriteWidth(spriteWidth), _bayerRow(bayer4[0])
{
	if (_mode != NONE && _mode != BAYER && _mode != FLOYD_STEINBERG)
		_mode = NONE; // out-of-range option value: degrade to no-dither
	if (_mode == FLOYD_STEINBERG)
	{
		prepareFsScratch(spriteWidth);
	}
}

void TintDither::beginRow(int py)
{
	_bayerRow = bayer4[py & 3];
}

void TintDither::quantise(int px, int rIn, int gIn, int bIn, int &r, int &g, int &b)
{
	if (_mode == FLOYD_STEINBERG)
	{
		// Add accumulated error from earlier passes (right-propagation from
		// this row's earlier pixels and 3-element broadcast from prev row).
		const int rE = rIn + g_errR_cur[px];
		const int gE = gIn + g_errG_cur[px];
		const int bE = bIn + g_errB_cur[px];
		// Clamped quantise: drop the 4-bit fraction, clip to [0,15].
		r = rE >> 4; if (r > 15) r = 15; else if (r < 0) r = 0;
		g = gE >> 4; if (g > 15) g = 15; else if (g < 0) g = 0;
		b = bE >> 4; if (b > 15) b = 15; else if (b < 0) b = 0;
		// Residual = ideal-with-accumulated-error vs the value we actually
		// chose (post-clamp). Folding the clamp in here keeps a saturated
		// channel from spilling wrong-sign error into its neighbours.
		const int rErr = rE - (r << 4);
		const int gErr = gE - (g << 4);
		const int bErr = bE - (b << 4);
		// Floyd-Steinberg distribution (weights /16):
		//         X     7
		//   3     5     1
		if (px + 1 < _spriteWidth)
		{
			g_errR_cur[px + 1]  += rErr * 7 / 16;
			g_errG_cur[px + 1]  += gErr * 7 / 16;
			g_errB_cur[px + 1]  += bErr * 7 / 16;
		}
		if (px > 0)
		{
			g_errR_next[px - 1] += rErr * 3 / 16;
			g_errG_next[px - 1] += gErr * 3 / 16;
			g_errB_next[px - 1] += bErr * 3 / 16;
		}
		g_errR_next[px]     += rErr * 5 / 16;
		g_errG_next[px]     += gErr * 5 / 16;
		g_errB_next[px]     += bErr * 5 / 16;
		if (px + 1 < _spriteWidth)
		{
			g_errR_next[px + 1] += rErr * 1 / 16;
			g_errG_next[px + 1] += gErr * 1 / 16;
			g_errB_next[px + 1] += bErr * 1 / 16;
		}
		return;
	}

	// NONE / BAYER share the truncate path; BAYER additionally bumps each
	// channel by 1 if the pixel's fractional part exceeds the matrix
	// threshold for this (px, py).
	r = rIn >> 4;
	g = gIn >> 4;
	b = bIn >> 4;
	if (_mode == BAYER)
	{
		const int t = _bayerRow[px & 3];
		if ((rIn & 15) > t) r += 1;
		if ((gIn & 15) > t) g += 1;
		if ((bIn & 15) > t) b += 1;
	}
	if (r > 15) r = 15; if (r < 0) r = 0;
	if (g > 15) g = 15; if (g < 0) g = 0;
	if (b > 15) b = 15; if (b < 0) b = 0;
}

void TintDither::endRow()
{
	if (_mode != FLOYD_STEINBERG) return;
	// Slide the window: yesterday's "next" becomes today's "cur"; recycle
	// yesterday's "cur" buffer to a zero-filled "next". Only zero [0, _spriteWidth)
	// since the tail (if any) was never written by this blit.
	std::swap(g_errR_cur, g_errR_next);
	std::swap(g_errG_cur, g_errG_next);
	std::swap(g_errB_cur, g_errB_next);
	std::fill(g_errR_next.begin(), g_errR_next.begin() + _spriteWidth, 0);
	std::fill(g_errG_next.begin(), g_errG_next.begin() + _spriteWidth, 0);
	std::fill(g_errB_next.begin(), g_errB_next.begin() + _spriteWidth, 0);
}

}
