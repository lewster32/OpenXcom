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
#include "MCDPatch.h"
#include "MapDataSet.h"
#include "MapData.h"
#include "../Engine/Palette.h"

namespace
{
// Sanity ceilings for parse-time validation of MCDPatch light fields.
// The runtime path re-clamps to the layer-specific max via
// Mod::getMaxStatic/DynamicLightDistance() (currently 16 / 24 in OXCE);
// these constants are a soft upper bound to reject obviously bogus
// modder values without coupling MCDPatch to a Mod handle. Match the
// equivalent local constants in RuleItem.cpp.
constexpr int LIGHT_RADIUS_MAX = 20;
constexpr double LIGHT_INTENSITY_MAX = 5.0;
}

namespace OpenXcom
{

/**
 * Initializes an MCD Patch.
 */
MCDPatch::MCDPatch()
{
}

/**
 *
 */
MCDPatch::~MCDPatch()
{
}

/**
 * Loads the MCD Patch from a YAML file.
 * TODO: fill this out with more data.
 * @param node YAML node.
 */
void MCDPatch::load(const YAML::YamlNodeReader& reader)
{
	for (const auto& d : reader["data"].children())
	{
		const auto& mcd = d.useIndex();
		size_t MCDIndex = mcd["MCDIndex"].readVal<size_t>();
		if (mcd["bigWall"])
		{
			int bigWall = mcd["bigWall"].readVal<int>();
			_bigWalls.push_back(std::make_pair(MCDIndex, bigWall));
		}
		if (mcd["TUWalk"])
		{
			int TUWalk = mcd["TUWalk"].readVal<int>();
			_TUWalks.push_back(std::make_pair(MCDIndex, TUWalk));
		}
		if (mcd["TUFly"])
		{
			int TUFly = mcd["TUFly"].readVal<int>();
			_TUFlys.push_back(std::make_pair(MCDIndex, TUFly));
		}
		if (mcd["TUSlide"])
		{
			int TUSlide = mcd["TUSlide"].readVal<int>();
			_TUSlides.push_back(std::make_pair(MCDIndex, TUSlide));
		}
		if (mcd["deathTile"])
		{
			int deathTile = mcd["deathTile"].readVal<int>();
			_deathTiles.push_back(std::make_pair(MCDIndex, deathTile));
		}
		if (mcd["terrainHeight"])
		{
			int terrainHeight = mcd["terrainHeight"].readVal<int>();
			_terrainHeight.push_back(std::make_pair(MCDIndex, terrainHeight));
		}
		if (mcd["specialType"])
		{
			int specialType = mcd["specialType"].readVal<int>();
			_specialTypes.push_back(std::make_pair(MCDIndex, specialType));
		}
		if (mcd["explosive"])
		{
			int explosive = mcd["explosive"].readVal<int>();
			_explosives.push_back(std::make_pair(MCDIndex, explosive));
		}
		if (mcd["armor"])
		{
			int armor = mcd["armor"].readVal<int>();
			_armors.push_back(std::make_pair(MCDIndex, armor));
		}
		if (mcd["flammability"])
		{
			int flammability = mcd["flammability"].readVal<int>();
			_flammabilities.push_back(std::make_pair(MCDIndex, flammability));
		}
		if (mcd["fuel"])
		{
			int fuel = mcd["fuel"].readVal<int>();
			_fuels.push_back(std::make_pair(MCDIndex, fuel));
		}
		if (mcd["footstepSound"])
		{
			int footstepSound = mcd["footstepSound"].readVal<int>();
			_footstepSounds.push_back(std::make_pair(MCDIndex, footstepSound));
		}
		if (mcd["HEBlock"])
		{
			int HEBlock = mcd["HEBlock"].readVal<int>();
			_HEBlocks.push_back(std::make_pair(MCDIndex, HEBlock));
		}
		if (mcd["noFloor"])
		{
			bool noFloor = mcd["noFloor"].readVal<bool>();
			_noFloors.push_back(std::make_pair(MCDIndex, noFloor));
		}
		if (mcd["LOFTS"])
		{
			std::vector<int> lofts = mcd["LOFTS"].readVal<std::vector<int> >();
			_LOFTS.push_back(std::make_pair(MCDIndex, lofts));
		}
		if (mcd["stopLOS"])
		{
			bool stopLOS = mcd["stopLOS"].readVal<bool>();
			_stopLOSses.push_back(std::make_pair(MCDIndex, stopLOS));
		}
		if (mcd["objectType"])
		{
			int objectType = mcd["objectType"].readVal<int>();
			_objectTypes.push_back(std::make_pair(MCDIndex, objectType));
		}
		if (mcd["lightSource"])
		{
			// Vanilla and existing-mod back-compat alias for light.source.
			int lightSource = mcd["lightSource"].readVal<int>();
			_lightSources.push_back(std::make_pair(MCDIndex, lightSource));
		}
		if (const auto& lightNode = mcd["light"])
		{
			if (lightNode["source"])
			{
				// Push another entry for the same MCDIndex. The apply order in
				// modifyData() (a simple iteration over _lightSources in push order)
				// makes this nested light.source push override the earlier flat
				// lightSource: push for the same MCDIndex; do not reorder this vector.
				int lightSource = lightNode["source"].readVal<int>();
				_lightSources.push_back(std::make_pair(MCDIndex, lightSource));
			}
			if (lightNode["radius"])
			{
				int radius = lightNode["radius"].readVal<int>();
				if (radius < 1) radius = 1;
				if (radius > LIGHT_RADIUS_MAX) radius = LIGHT_RADIUS_MAX;
				_lightRadii.push_back(std::make_pair(MCDIndex, radius));
			}
			if (lightNode["intensity"])
			{
				double intensity = lightNode["intensity"].readVal<double>();
				if (intensity < 0.0) intensity = 0.0;
				if (intensity > LIGHT_INTENSITY_MAX) intensity = LIGHT_INTENSITY_MAX;
				_lightIntensities.push_back(std::make_pair(MCDIndex, intensity));
			}
			if (lightNode["litChance"])
			{
				double litChance = lightNode["litChance"].readVal<double>();
				if (litChance < 0.0) litChance = 0.0;
				if (litChance > 1.0) litChance = 1.0;
				_litChances.push_back(std::make_pair(MCDIndex, litChance));
			}
			if (lightNode["offset"])
			{
				std::vector<int> offset = lightNode["offset"].readVal<std::vector<int> >();
				if (offset.size() == 3)
					_lightOffsets.push_back(std::make_pair(MCDIndex, offset));
			}
			if (lightNode["color"])
			{
				int r, g, b;
				Palette::readColor(lightNode["color"], r, g, b);
				std::vector<int> rgb;
				rgb.push_back(r);
				rgb.push_back(g);
				rgb.push_back(b);
				_lightColors.push_back(std::make_pair(MCDIndex, rgb));
			}
			if (lightNode["fullBright"])
			{
				bool fullBright = lightNode["fullBright"].readVal<bool>();
				_fullBrights.push_back(std::make_pair(MCDIndex, fullBright ? 1 : 0));
			}
		}
	}
}

/**
 * Applies an MCD patch to a mapDataSet.
 * @param dataSet The MapDataSet we want to modify.
 */
void MCDPatch::modifyData(MapDataSet *dataSet) const
{
	for (const auto& pair : _bigWalls)
	{
		dataSet->getObject(pair.first)->setBigWall(pair.second);
	}
	for (const auto& pair : _TUWalks)
	{
		dataSet->getObject(pair.first)->setTUWalk(pair.second);
	}
	for (const auto& pair : _TUFlys)
	{
		dataSet->getObject(pair.first)->setTUFly(pair.second);
	}
	for (const auto& pair : _TUSlides)
	{
		dataSet->getObject(pair.first)->setTUSlide(pair.second);
	}
	for (const auto& pair : _deathTiles)
	{
		dataSet->getObject(pair.first)->setDieMCD(pair.second);
	}
	for (const auto& pair : _terrainHeight)
	{
		dataSet->getObject(pair.first)->setTerrainLevel(pair.second);
	}
	for (const auto& pair : _specialTypes)
	{
		dataSet->getObject(pair.first)->setSpecialType(pair.second, dataSet->getObject(pair.first)->getObjectType());
	}
	for (const auto& pair : _explosives)
	{
		dataSet->getObject(pair.first)->setExplosive(pair.second);
	}
	for (const auto& pair : _armors)
	{
		dataSet->getObject(pair.first)->setArmor(pair.second);
	}
	for (const auto& pair : _flammabilities)
	{
		dataSet->getObject(pair.first)->setFlammable(pair.second);
	}
	for (const auto& pair : _fuels)
	{
		dataSet->getObject(pair.first)->setFuel(pair.second);
	}
	for (const auto& pair : _HEBlocks)
	{
		dataSet->getObject(pair.first)->setHEBlock(pair.second);
	}
	for (const auto& pair : _footstepSounds)
	{
		dataSet->getObject(pair.first)->setFootstepSound(pair.second);
	}
	for (const auto& pair : _objectTypes)
	{
		dataSet->getObject(pair.first)->setObjectType((TilePart)pair.second);
	}
	for (const auto& pair : _noFloors)
	{
		dataSet->getObject(pair.first)->setNoFloor(pair.second);
	}
	for (const auto& pair : _stopLOSses)
	{
		dataSet->getObject(pair.first)->setStopLOS(pair.second);
	}
	for (const auto& pair : _LOFTS)
	{
		int layer = 0;
		for (int loft : pair.second)
		{
			dataSet->getObject(pair.first)->setLoftID(loft, layer);
			++layer;
		}
	}
	for (const auto& pair : _lightSources)
	{
		dataSet->getObject(pair.first)->setModLightSource(pair.second);
	}
	for (const auto& pair : _lightOffsets)
	{
		const std::vector<int>& o = pair.second;
		dataSet->getObject(pair.first)->setLightOffset(Position(o[0], o[1], o[2]));
	}
	for (const auto& pair : _lightColors)
	{
		const std::vector<int>& rgb = pair.second;
		if (rgb.size() != 3) continue;
		MapData* md = dataSet->getObject(pair.first);
		md->setModLightColor(rgb[0], rgb[1], rgb[2]);
		if (!md->hasModLightSource() && md->getLightSource() <= 0)
			md->setModLightSource(1);
	}
	for (const auto& pair : _fullBrights)
	{
		dataSet->getObject(pair.first)->setFullBright(pair.second);
	}
	for (const auto& pair : _lightRadii)
	{
		dataSet->getObject(pair.first)->setModLightRadius(pair.second);
	}
	for (const auto& pair : _lightIntensities)
	{
		dataSet->getObject(pair.first)->setModLightIntensity(pair.second);
	}
	for (const auto& pair : _litChances)
	{
		dataSet->getObject(pair.first)->setLitChance(pair.second);
	}
}

}
