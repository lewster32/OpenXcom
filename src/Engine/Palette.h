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
#include <string>
#include <map>
#include <vector>
#include <SDL.h>

namespace OpenXcom
{

namespace YAML { class YamlNodeReader; }

/**
 * Container for palettes (sets of 8bpp colors).
 * Works as an encapsulation for SDL's SDL_Color struct and
 * provides shortcuts for common tasks to make code more readable.
 */
class Palette
{
private:
	SDL_Color *_colors;
	int _count;
	int _firstUsableColor; ///< First palette index that nearestIndex may return (defaults to 1 - skip transparent slot 0).
	int _lastUsableColor;  ///< Last palette index (inclusive) that nearestIndex may return (defaults to 255). TFTD battlescape palettes set this to 254 to skip the reserved tail slot.
	std::map<int, Uint8*> _blendLUTs;
	std::map<int, Uint8*> _tintLUTs; ///< Keyed on mix; each LUT is 256 * 4096 bytes (4-bit per-channel grid).
	/// Pre-converted OKLab triples [L, a, b] interleaved, one per palette entry. Built lazily on first
	/// nearestIndexOKLab call after _colors becomes valid; cleared whenever _colors is mutated. ~3 KB per palette.
	mutable std::vector<float> _oklab;
	/// Lazily populates _oklab from _colors. Idempotent. Called from nearestIndexOKLab.
	void ensureOKLabCache() const;
public:
	/// Creates a blank palette.
	Palette();
	/// Cleans up the palette.
	~Palette();
	/// Loads the colors from an X-Com palette.
	void loadDat(const std::string &filename, int ncolors, int offset = 0);
	/// Initializes an all-black palette.
	void initBlack();
	/// Loads the colors from an existing palette.
	void copyFrom(Palette *srcPal);
	// Gets a certain color from the palette.
	SDL_Color *getColors(int offset = 0) const;
	// Gets a number of colors in the palette.
	int getColorCount() const { return _count; }

	void savePal(const std::string &file) const;
	void savePalMod(const std::string &file, const std::string &type, const std::string &target) const;
	void savePalJasc(const std::string &file) const;
	void setColors(SDL_Color* pal, int ncolors);
	void setColor(int index, int r, int g, int b);
	/// Restricts the index range that nearestIndex may return. Use this to keep blend/tint LUTs from picking
	/// reserved slots (transparent index 0, TFTD's reserved tail at 255, etc). Defaults are (1, 255).
	void setUsableColorRange(int first, int last) { _firstUsableColor = first; _lastUsableColor = last; }
	int getFirstUsableColor() const { return _firstUsableColor; }
	int getLastUsableColor() const { return _lastUsableColor; }
	void copyColor(int index, int r, int g, int b);
	/// Returns a 256x256 LUT where lut[src*256+dst] = nearest palette index to (opacity*srcRGB + (100-opacity)*dstRGB)/100. Caches one table per requested opacity.
	const Uint8 *getBlendLUT(int opacity);
	/// Parses a "#rrggbb" hex colour string. On malformed input logs a warning and returns (255,255,255).
	static void parseHexColor(const std::string &hex, int &r, int &g, int &b);
	/// Reads a light colour from a YAML node. Accepts either "#rrggbb" hex string or [r, g, b] int array form.
	/// On missing or malformed input, logs a warning and returns white (255, 255, 255).
	static void readColor(const YAML::YamlNodeReader &node, int &r, int &g, int &b);
	/// Returns a 256 * 4096 tint LUT for the additive-photon model. The grid index
	/// is the 4-bit-per-channel bit-packed quantised RGB accumulator (12 bits total).
	/// Cached per mix value; built lazily.
	const Uint8 *getTintLUT(int mix);
	/// Returns a pointer to the lazily-built OKLab cache (3 floats per palette entry: L, a, b).
	/// Triggers ensureOKLabCache(). Used by the OKLab nearest-search in getTintLUT.
	const float *getOKLabCache() const { ensureOKLabCache(); return _oklab.data(); }
	/// Converts a given color into a RGBA color value.
	static Uint32 getRGBA(SDL_Color* pal, Uint8 color);
	/// Gets the position of a given palette.
	/**
	 * Returns the position of a palette inside an X-Com palette file (each is a 768-byte chunks).
	 * Handy for loading the palettes from the game files.
	 * @param palette Requested palette.
	 * @return Palette position in bytes.
	 */
	static inline int palOffset(int palette) { return palette*(768+6); }
	/// Gets the position of a certain color block in a palette.
	/**
	 * Returns the position of a certain color block in an X-Com palette (they're usually split in 16-color gradients).
	 * Makes setting element colors a lot easier than determining the exact color position.
	 * @param block Requested block.
	 * @return Color position.
	 */
	static inline Uint8 blockOffset(Uint8 block) { return block*16; }
	/// Position of the background colors block in an X-Com palette (used for background images in screens).
	static const int backPos = 224;
};

}
