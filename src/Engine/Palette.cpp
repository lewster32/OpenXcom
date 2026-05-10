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
#include "Palette.h"
#include <sstream>
#include <vector>
#include <cmath>
#include <limits>
#include "CrossPlatform.h"
#include "Exception.h"
#include "FileMap.h"
#include "Logger.h"
#include "OkLab.h"
#include "Yaml.h"

namespace
{

// Single source of truth for the perceptual colour math (sRGB <-> linear,
// rgbToOKLab) lives in Engine/OkLab.h. Bring those helpers into this TU's
// anonymous namespace via using-directive so the existing call sites
// (nearestIndexOKLab, ensureOKLabCache, getTintLUT) keep working unqualified.
using namespace OpenXcom::OkLab;

inline int sqrDist(const SDL_Color &a, const SDL_Color &b)
{
	int dr = (int)a.r - (int)b.r;
	int dg = (int)a.g - (int)b.g;
	int db = (int)a.b - (int)b.b;
	return dr * dr + dg * dg + db * db;
}

// Iterates only [firstColor, lastColor] inclusive. Reserved slots (transparent index 0,
// TFTD's reserved tail) must stay outside the search or near-black/near-tail tint targets
// would map to them, producing transparent or wrong-colour pixels in tinted blits.
Uint8 nearestIndex(const SDL_Color *colors, const SDL_Color &target, int firstColor, int lastColor)
{
	int best = firstColor;
	int bestDist = sqrDist(colors[firstColor], target);
	for (int i = firstColor + 1; i <= lastColor; ++i)
	{
		int d = sqrDist(colors[i], target);
		if (d < bestDist)
		{
			bestDist = d;
			best = i;
		}
	}
	return (Uint8)best;
}

// Search-by-OKLab-distance restricted to [firstColor, lastColor]. Mirrors
// nearestIndex's reserved-slot exclusion. Uses the lazy OKLab cache attached to
// the palette so per-entry conversion runs once per palette load, not per LUT cell.
Uint8 nearestIndexOKLab(const OpenXcom::Palette &pal, int targetR, int targetG, int targetB,
	int firstColor, int lastColor)
{
	const float *lab = pal.getOKLabCache();
	float tL, tA, tB;
	rgbToOKLab(targetR, targetG, targetB, tL, tA, tB);
	int best = firstColor;
	float bestDist = std::numeric_limits<float>::infinity();
	for (int i = firstColor; i <= lastColor; ++i)
	{
		float dL = lab[i * 3]     - tL;
		float dA = lab[i * 3 + 1] - tA;
		float dB = lab[i * 3 + 2] - tB;
		float d = dL * dL + dA * dA + dB * dB;
		if (d < bestDist)
		{
			bestDist = d;
			best = i;
		}
	}
	return (Uint8)best;
}

}

namespace OpenXcom
{

// Lift OkLab math into OpenXcom for the Palette member functions
// (Palette::getTintLUT, Palette::ensureOKLabCache) that call srgbToLinear,
// linearToSrgb and rgbToOKLab unqualified. Source of truth: Engine/OkLab.h.
using namespace OkLab;

/**
 * Initializes a brand new palette.
 */
// Defaults skip the transparent slot 0 (a global X-COM palette convention), but
// keep the full tail. TFTD battlescape palettes narrow the upper bound to 254
// because index 255 is reserved; that override is applied at LBM-load time.
Palette::Palette() : _colors(0), _count(0), _firstUsableColor(1), _lastUsableColor(255)
{
}

/**
 * Deletes any colors contained within.
 */
Palette::~Palette()
{
	delete[] _colors;
	for (std::map<int, Uint8*>::iterator it = _blendLUTs.begin(); it != _blendLUTs.end(); ++it)
	{
		delete[] it->second;
	}
	for (std::map<int, Uint8*>::iterator it = _tintLUTs.begin(); it != _tintLUTs.end(); ++it)
	{
		delete[] it->second;
	}
}

/**
 * Loads an X-Com palette from a file. X-Com palettes are just a set
 * of RGB colors in a row, on a 0-63 scale, which have to be adjusted
 * for modern computers (0-255 scale).
 * @param filename Filename of the palette.
 * @param ncolors Number of colors in the palette.
 * @param offset Position of the palette in the file (in bytes).
 * @sa http://www.ufopaedia.org/index.php?title=PALETTES.DAT
 */
void Palette::loadDat(const std::string &filename, int ncolors, int offset)
{
	auto palFile = FileMap::getIStream(filename);
	if (_colors != 0)
		throw Exception("loadDat can be run only once");
	_count = ncolors;
	_colors = new SDL_Color[_count];
	memset(_colors, 0, sizeof(SDL_Color) * _count);

	// Move pointer to proper palette
	palFile->seekg(offset, std::ios::beg);

	Uint8 value[3];

	for (int i = 0; i < _count && palFile->read((char*)value, 3); ++i)
	{
		// Correct X-Com colors to RGB colors
		_colors[i].r = value[0] * 4;
		_colors[i].g = value[1] * 4;
		_colors[i].b = value[2] * 4;
		_colors[i].unused = 255;
	}
	_colors[0].unused = 0;
}

/**
 * Initializes an all-black palette.
 */
void Palette::initBlack()
{
	if (_colors != 0)
		throw Exception("initBlack can be run only once");
	_count = 256;
	_colors = new SDL_Color[_count];
	memset(_colors, 0, sizeof(SDL_Color) * _count);

	for (int i = 0; i < _count; ++i)
	{
		_colors[i].r = 0;
		_colors[i].g = 0;
		_colors[i].b = 0;
		_colors[i].unused = 255;
	}
	_colors[0].unused = 0;
}

/**
 * Loads the colors from an existing palette.
 */
void Palette::copyFrom(Palette *srcPal)
{
	for (int i = 0; i < srcPal->getColorCount(); i++)
	{
		setColor(i, srcPal->getColors(i)->r, srcPal->getColors(i)->g, srcPal->getColors(i)->b);
	}
}

/**
 * Provides access to colors contained in the palette.
 * @param offset Offset to a specific color.
 * @return Pointer to the requested SDL_Color.
 */
SDL_Color *Palette::getColors(int offset) const
{
	return _colors + offset;
}

/**
 * Converts an SDL_Color struct into an hexadecimal RGBA color value.
 * Mostly used for operations with SDL_gfx that require colors in this format.
 * @param pal Requested palette.
 * @param color Requested color in the palette.
 * @return Hexadecimal RGBA value.
 */
Uint32 Palette::getRGBA(SDL_Color* pal, Uint8 color)
{
	return ((Uint32) pal[color].r << 24) | ((Uint32) pal[color].g << 16) | ((Uint32) pal[color].b << 8) | (Uint32) 0xFF;
}

void Palette::savePal(const std::string &file) const
{
	std::stringstream out;
	short count = _count;

	// RIFF header
	out << "RIFF";
	int length = 4 + 4 + 4 + 4 + 2 + 2 + count * 4;
	out.write((char*) &length, sizeof(length));
	out << "PAL ";

	// Data chunk
	out << "data";
	int data = count * 4 + 4;
	out.write((char*) &data, sizeof(data));
	short version = 0x0300;
	out.write((char*) &version, sizeof(version));
	out.write((char*) &count, sizeof(count));

	// Colors
	SDL_Color *color = getColors();
	for (short i = 0; i < count; ++i)
	{
		char c = 0;
		out.write((char*) &color->r, 1);
		out.write((char*) &color->g, 1);
		out.write((char*) &color->b, 1);
		out.write(&c, 1);
		color++;
	}
	CrossPlatform::writeFile(file, out.str());
}

void Palette::savePalMod(const std::string &file, const std::string &type, const std::string &target) const
{
	std::stringstream out;
	short count = _count;

	// header
	out << "customPalettes:\n";
	out << "  - type: " << type << "\n";
	out << "    target: " << target << "\n";
	out << "    palette:\n";

	// Colors
	for (int i = 0; i < count; ++i)
	{
		out << "      ";
		out << std::to_string(i);
		out << ": [";
		out << std::to_string(_colors[i].r);
		out << ",";
		out << std::to_string(_colors[i].g);
		out << ",";
		out << std::to_string(_colors[i].b);
		out << "]\n";
	}
	CrossPlatform::writeFile(file, out.str());
}

void Palette::savePalJasc(const std::string &file) const
{
	std::stringstream out;
	short count = _count;

	// header
	out << "JASC-PAL\n";
	out << "0100\n";
	out << "256\n";

	// Colors
	for (int i = 0; i < count; ++i)
	{
		out << std::to_string(_colors[i].r);
		out << " ";
		out << std::to_string(_colors[i].g);
		out << " ";
		out << std::to_string(_colors[i].b);
		out << "\n";
	}
	CrossPlatform::writeFile(file, out.str());
}

const Uint8 *Palette::getBlendLUT(int opacity)
{
	std::map<int, Uint8*>::const_iterator it = _blendLUTs.find(opacity);
	if (it != _blendLUTs.end())
		return it->second;

	Uint8 *lut = new Uint8[256 * 256];
	for (int src = 0; src < 256; ++src)
	{
		for (int dst = 0; dst < 256; ++dst)
		{
			SDL_Color blended;
			blended.r = (Uint8)((_colors[src].r * opacity + _colors[dst].r * (100 - opacity)) / 100);
			blended.g = (Uint8)((_colors[src].g * opacity + _colors[dst].g * (100 - opacity)) / 100);
			blended.b = (Uint8)((_colors[src].b * opacity + _colors[dst].b * (100 - opacity)) / 100);
			lut[src * 256 + dst] = nearestIndex(_colors, blended, _firstUsableColor, _lastUsableColor);
		}
	}
	_blendLUTs[opacity] = lut;
	return lut;
}

void Palette::ensureOKLabCache() const
{
	if (!_oklab.empty()) return;
	_oklab.resize(_count * 3);
	for (int i = 0; i < _count; ++i)
	{
		float L, A, B;
		rgbToOKLab(_colors[i].r, _colors[i].g, _colors[i].b, L, A, B);
		_oklab[i * 3]     = L;
		_oklab[i * 3 + 1] = A;
		_oklab[i * 3 + 2] = B;
	}
}

/**
 * Returns a 256 by 4096 tint LUT for the given mix value (0..100). Cached per mix.
 * Each entry maps (shadedSrc, 12-bit-packed grid index) to the nearest palette colour
 * for the additive-photon tint pipeline. Allocates 1 MB per cached mix.
 *
 * The bg * cellRGB multiply runs in linear-light space (sRGB decode -> multiply ->
 * sRGB encode) and the nearest-palette search uses OKLab perceptual distance. Both
 * eliminate distinct classes of banding/hue-shift artefacts in coloured-light
 * gradients. See docs/superpowers/specs/2026-05-08-perceptual-palette-snap-design.md.
 */
const Uint8 *Palette::getTintLUT(int mix)
{
	std::map<int, Uint8*>::const_iterator it = _tintLUTs.find(mix);
	if (it != _tintLUTs.end())
		return it->second;

	// 4-bit per channel: 16 cells per channel, 4096 cells total, 1 MB LUT per mix value.
	const int cellsPerChannel = 16;
	const int cells = 4096;
	const int step = 16;
	const int topCell = cellsPerChannel - 1;

	Uint8 *lut = new Uint8[256 * cells];
	for (int src = 0; src < 256; ++src)
	{
		// Hoist src's linear decode out of the cell loop - it's invariant across all 4096 cells.
		double srLin = srgbToLinear(_colors[src].r);
		double sgLin = srgbToLinear(_colors[src].g);
		double sbLin = srgbToLinear(_colors[src].b);

		for (int cell = 0; cell < cells; ++cell)
		{
			// Unpack the cell index into per-channel quantised values.
			// Inverse of Tile::quantiseAccumulator: (qR << 8) | (qG << 4) | qB.
			int qR = (cell >> 8) & 0xF;
			int qG = (cell >> 4) & 0xF;
			int qB = cell & 0xF;

			// Reconstruct the cell's representative RGB. The bottom and top cells
			// snap to 0/255 to preserve two spec invariants: a pure-zero accumulator
			// must render pitch black, and a fully-saturated accumulator must render
			// byte-identical to vanilla. All interior cells use the midpoint so
			// quantisation is unbiased there.
			int gR = (qR == 0) ? 0 : (qR == topCell) ? 255 : qR * step + step / 2;
			int gG = (qG == 0) ? 0 : (qG == topCell) ? 255 : qG * step + step / 2;
			int gB = (qB == 0) ? 0 : (qB == topCell) ? 255 : qB * step + step / 2;

			// Lerp toward white by mix: effective = white + mix/100 * (grid - white).
			int effR = 255 - mix * (255 - gR) / 100;
			int effG = 255 - mix * (255 - gG) / 100;
			int effB = 255 - mix * (255 - gB) / 100;

			// Multiply in linear-light space, then re-encode to sRGB bytes.
			double cRLin = srgbToLinear(effR);
			double cGLin = srgbToLinear(effG);
			double cBLin = srgbToLinear(effB);

			SDL_Color target;
			target.r = (Uint8)linearToSrgb(srLin * cRLin);
			target.g = (Uint8)linearToSrgb(sgLin * cGLin);
			target.b = (Uint8)linearToSrgb(sbLin * cBLin);

			lut[src * cells + cell] = nearestIndexOKLab(*this, target.r, target.g, target.b,
				_firstUsableColor, _lastUsableColor);
		}
	}
	_tintLUTs[mix] = lut;
	return lut;
}

/**
 * Parses a `#rrggbb` hex colour string into out-parameter RGB triple.
 * On malformed input (wrong length, missing leading `#`, bad hex chars) logs
 * a warning and returns white (255, 255, 255).
 */
void Palette::parseHexColor(const std::string &hex, int &r, int &g, int &b)
{
	r = g = b = 255;
	if (hex.size() != 7 || hex[0] != '#')
	{
		Log(LOG_WARNING) << "parseHexColor: malformed colour '" << hex << "', defaulting to #ffffff";
		return;
	}
	auto nib = [](char c) -> int {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	};
	int values[6];
	for (int i = 0; i < 6; ++i)
	{
		values[i] = nib(hex[i + 1]);
		if (values[i] < 0)
		{
			Log(LOG_WARNING) << "parseHexColor: non-hex char in '" << hex << "', defaulting to #ffffff";
			return;
		}
	}
	r = (values[0] << 4) | values[1];
	g = (values[2] << 4) | values[3];
	b = (values[4] << 4) | values[5];
}

/**
 * Reads a light colour from a YAML node.
 * Accepts either a "#rrggbb" hex string or a [r, g, b] int array.
 * On a missing or malformed node, logs a warning and returns white (255, 255, 255).
 */
void Palette::readColor(const YAML::YamlNodeReader &node, int &r, int &g, int &b)
{
	r = g = b = 255;
	if (!node) return;

	if (node.isSeq())
	{
		std::vector<int> rgb = node.readVal<std::vector<int> >();
		if (rgb.size() == 3)
		{
			r = rgb[0];
			g = rgb[1];
			b = rgb[2];
		}
		else
		{
			Log(LOG_WARNING) << "readColor: expected 3-element array [r, g, b], got " << rgb.size() << " elements; using white";
		}
	}
	else
	{
		std::string hex = node.readVal<std::string>();
		parseHexColor(hex, r, g, b);
	}
}

void Palette::setColors(SDL_Color* pal, int ncolors)
{
	if (_colors != 0)
		throw Exception("setColors can be run only once");
	_count = ncolors;
	_colors = new SDL_Color[_count];
	memset(_colors, 0, sizeof(SDL_Color) * _count);

	for (int i = 0; i < _count; ++i)
	{
		// TFTD's LBM colors are good the way they are - no need for adjustment here, except...
		_colors[i].r = pal[i].r;
		_colors[i].g = pal[i].g;
		_colors[i].b = pal[i].b;
		_colors[i].unused = 255;
		if (i > 15 && _colors[i].r == _colors[0].r &&
			_colors[i].g == _colors[0].g &&
			_colors[i].b == _colors[0].b)
		{
			// SDL "optimizes" surfaces by using RGB colour matching to reassign pixels to an "earlier" matching colour in the palette,
			// meaning any pixels in a surface that are meant to be black will be reassigned as colour 0, rendering them transparent.
			// avoid this eventuality by altering the "later" colours just enough to disambiguate them without causing them to look significantly different.
			// SDL 2.0 has some functionality that should render this hack unnecessary.
			_colors[i].r++;
			_colors[i].g++;
			_colors[i].b++;
		}
	}
	_colors[0].unused = 0;
	_oklab.clear();
}

void Palette::setColor(int index, int r, int g, int b)
{
	_colors[index].r = r;
	_colors[index].g = g;
	_colors[index].b = b;
	if (index > 15 && _colors[index].r == _colors[0].r &&
		_colors[index].g == _colors[0].g &&
		_colors[index].b == _colors[0].b)
	{
		// SDL "optimizes" surfaces by using RGB colour matching to reassign pixels to an "earlier" matching colour in the palette,
		// meaning any pixels in a surface that are meant to be black will be reassigned as colour 0, rendering them transparent.
		// avoid this eventuality by altering the "later" colours just enough to disambiguate them without causing them to look significantly different.
		// SDL 2.0 has some functionality that should render this hack unnecessary.
		_colors[index].r++;
		_colors[index].g++;
		_colors[index].b++;
	}
	_oklab.clear();
}

void Palette::copyColor(int index, int r, int g, int b)
{
	_colors[index].r = r;
	_colors[index].g = g;
	_colors[index].b = b;
	_oklab.clear();
}

}
