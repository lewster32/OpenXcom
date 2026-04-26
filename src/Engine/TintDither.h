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
#include <vector>

namespace OpenXcom
{

/**
 * Per-pixel dither helper used by the per-corner floor tint blit.
 *
 * Inputs are bilinear-interpolated channel values in 4-bit-fraction precision
 * (range 0..240, where input >> 4 = integer 0..15 cell). Outputs are the
 * final clamped 4-bit cell index (0..15) per channel.
 *
 * Three modes are supported:
 *   NONE             - pure truncate; visible 16-step gradient inside each tile.
 *   BAYER            - ordered 4x4 threshold dither; static stippled pattern,
 *                      no inter-pixel state.
 *   FLOYD_STEINBERG  - error diffusion; per-pixel residual is distributed to
 *                      right + 3 below neighbours (weights /16: 7, 3, 5, 1).
 *                      Allocates two scanline-length error buffers per channel.
 *
 * Usage per blit:
 *   TintDither d(mode, spriteWidth);
 *   for each row py:
 *     d.beginRow(py);
 *     for each px:
 *       int r, g, b; d.quantise(px, r16, g16, b16, r, g, b);
 *       ...
 *     d.endRow();
 */
class TintDither
{
public:
	enum Mode { NONE = 0, BAYER = 1, FLOYD_STEINBERG = 2 };

	/// Construct for a sprite of the given width. FS allocates ~6 * spriteWidth ints; other modes are zero-cost.
	TintDither(int mode, int spriteWidth);

	/// Update per-row state. Call once per scanline before iterating pixels.
	void beginRow(int py);

	/// Quantise one pixel's per-channel 4-bit-fraction values to 4-bit cell indices (0..15), clamping. For FS mode, also reads accumulated error from earlier passes and propagates the residual to neighbour pixels.
	void quantise(int px, int rIn, int gIn, int bIn, int &r, int &g, int &b);

	/// Slide the FS error window. Call once per scanline after iterating pixels.
	void endRow();

private:
	Mode _mode;
	int _spriteWidth;
	const int *_bayerRow;        // points into a static 4x4 matrix; valid only after beginRow()
	// Owned only when _mode == FLOYD_STEINBERG; otherwise empty.
	std::vector<int> _errR_cur, _errG_cur, _errB_cur;
	std::vector<int> _errR_next, _errG_next, _errB_next;
};

}
