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

/*
 * Single-source perceptual colour math for the Realistic Lighting pipeline.
 *
 * - srgbToLinear / linearToSrgb : sRGB <-> linear-light conversion using the
 *   standard piecewise-linear curve below 0.04045 / 0.0031308 and gamma 2.4
 *   above. Used wherever channel-RGB needs to be multiplied physically (the
 *   tint-LUT linear-light path) before snapping back to sRGB byte space.
 *
 * - rgbToOKLab : Bjorn Ottosson's OKLab perceptually-uniform colour space.
 *   Better than CIELAB in the blue region and cheap (one cbrt per channel).
 *   Output L is in [0..1]-ish; a/b are small-magnitude signed floats.
 *   Reference: https://bottosson.github.io/posts/oklab/
 */

#include <cmath>

namespace OpenXcom
{
namespace OkLab
{

// sRGB byte [0..255] -> linear [0..1]. Standard sRGB curve.
inline double srgbToLinear(int c)
{
	double f = c / 255.0;
	return f <= 0.04045 ? f / 12.92 : std::pow((f + 0.055) / 1.055, 2.4);
}

// Linear [0..1] -> sRGB byte [0..255]. Inverse of srgbToLinear with rounded
// byte output and clamps at the extremes.
inline int linearToSrgb(double c)
{
	if (c <= 0.0)
	{
		return 0;
	}
	if (c >= 1.0)
	{
		return 255;
	}
	double f = c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
	return (int)std::lround(f * 255.0);
}

// sRGB byte triple -> OKLab triple. See https://bottosson.github.io/posts/oklab/
inline void rgbToOKLab(int r, int g, int b, float &L, float &A, float &B)
{
	double lr = srgbToLinear(r);
	double lg = srgbToLinear(g);
	double lb = srgbToLinear(b);
	double l = 0.4122214708 * lr + 0.5363325363 * lg + 0.0514459929 * lb;
	double m = 0.2119034982 * lr + 0.6806995451 * lg + 0.1073969566 * lb;
	double s = 0.0883024619 * lr + 0.2817188376 * lg + 0.6299787005 * lb;
	double lc = std::cbrt(l);
	double mc = std::cbrt(m);
	double sc = std::cbrt(s);
	L = (float)( 0.2104542553 * lc + 0.7936177850 * mc - 0.0040720468 * sc);
	A = (float)( 1.9779984951 * lc - 2.4285922050 * mc + 0.4505937099 * sc);
	B = (float)( 0.0259040371 * lc + 0.7827717662 * mc - 0.8086757660 * sc);
}

} // namespace OkLab
} // namespace OpenXcom
