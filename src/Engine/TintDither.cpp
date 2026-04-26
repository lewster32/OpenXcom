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
}

TintDither::TintDither(int mode, int spriteWidth)
	: _mode(static_cast<Mode>(mode)), _spriteWidth(spriteWidth), _bayerRow(bayer4[0])
{
	if (_mode != NONE && _mode != BAYER && _mode != FLOYD_STEINBERG)
		_mode = NONE; // out-of-range option value: degrade to no-dither
	if (_mode == FLOYD_STEINBERG)
	{
		_errR_cur.assign(spriteWidth, 0);
		_errG_cur.assign(spriteWidth, 0);
		_errB_cur.assign(spriteWidth, 0);
		_errR_next.assign(spriteWidth, 0);
		_errG_next.assign(spriteWidth, 0);
		_errB_next.assign(spriteWidth, 0);
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
		const int rE = rIn + _errR_cur[px];
		const int gE = gIn + _errG_cur[px];
		const int bE = bIn + _errB_cur[px];
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
			_errR_cur[px + 1]  += rErr * 7 / 16;
			_errG_cur[px + 1]  += gErr * 7 / 16;
			_errB_cur[px + 1]  += bErr * 7 / 16;
		}
		if (px > 0)
		{
			_errR_next[px - 1] += rErr * 3 / 16;
			_errG_next[px - 1] += gErr * 3 / 16;
			_errB_next[px - 1] += bErr * 3 / 16;
		}
		_errR_next[px]     += rErr * 5 / 16;
		_errG_next[px]     += gErr * 5 / 16;
		_errB_next[px]     += bErr * 5 / 16;
		if (px + 1 < _spriteWidth)
		{
			_errR_next[px + 1] += rErr * 1 / 16;
			_errG_next[px + 1] += gErr * 1 / 16;
			_errB_next[px + 1] += bErr * 1 / 16;
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
	// yesterday's "cur" buffer to a zero-filled "next".
	std::swap(_errR_cur, _errR_next);
	std::swap(_errG_cur, _errG_next);
	std::swap(_errB_cur, _errB_next);
	std::fill(_errR_next.begin(), _errR_next.end(), 0);
	std::fill(_errG_next.begin(), _errG_next.end(), 0);
	std::fill(_errB_next.begin(), _errB_next.end(), 0);
}

}
