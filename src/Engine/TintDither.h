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
 *                      Uses thread_local scratch buffers shared across blits;
 *                      construction zero-initialises [0, spriteWidth) on demand.
 *
 * Usage per blit:
 *   TintDither d(mode, spriteWidth);
 *   for each row py:
 *     d.beginRow(py);
 *     for each px:
 *       int r, g, b; d.quantise(px, r16, g16, b16, r, g, b);
 *       ...
 *     d.endRow();
 *
 * Not safe for re-entrant or interleaved use within a single thread (FS scratch
 * is shared file-scope state). drawTerrain calls are sequential, so this holds.
 */
class TintDither
{
public:
	enum Mode { NONE = 0, BAYER = 1, FLOYD_STEINBERG = 2 };

	/// Construct for a sprite of the given width. FS lazily resizes thread_local scratch and zeroes [0, spriteWidth). NONE/BAYER are zero-cost.
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
};

}
