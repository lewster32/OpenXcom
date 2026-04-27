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
#include "RuleItem.h"
#include "../Battlescape/Position.h"

namespace OpenXcom
{

class MapDataSet;

enum SpecialTileType : int {TILE=0,
					START_POINT,
					UFO_POWER_SOURCE,
					UFO_NAVIGATION,
					UFO_CONSTRUCTION,
					ALIEN_FOOD,
					ALIEN_REPRODUCTION,
					ALIEN_ENTERTAINMENT,
					ALIEN_SURGERY,
					EXAM_ROOM,
					ALIEN_ALLOYS,
					ALIEN_HABITAT,
					DEAD_TILE,
					END_POINT,
					MUST_DESTROY,
					DEATH_TRAPS=200};

enum MovementType : int { MT_WALK, MT_FLY, MT_SLIDE, MT_FLOAT, MT_SINK};
enum VoxelType : int { V_EMPTY = -1, V_FLOOR, V_WESTWALL, V_NORTHWALL, V_OBJECT, V_UNIT, V_OUTOFBOUNDS };
enum TilePart : int { O_FLOOR, O_WESTWALL, O_NORTHWALL, O_OBJECT, O_MAX };

/**
 * MapData is the smallest piece of a Battlescape terrain, holding info about a certain object, wall, floor, ...
 * @sa MapDataSet.
 */
class MapData
{
private:
	MapDataSet *_dataset;
	SpecialTileType _specialType;
	bool _isUfoDoor, _stopLOS, _isNoFloor, _isGravLift, _isDoor, _blockFire, _blockSmoke, _baseModule;
	int _yOffset, _TUWalk, _TUFly, _TUSlide, _terrainLevel, _footstepSound, _dieMCD, _altMCD;
	TilePart _objectType;
	int _lightSource;
	int _modLightSource;
	bool _hasModLightSource;
	Position _lightOffset;
	int _lightColorR, _lightColorG, _lightColorB;
	int _modLightColorR, _modLightColorG, _modLightColorB;
	bool _hasModLightColor;
	// Tri-state: -1 = unset (legacy auto from getLightSource()), 0 = forced off, 1 = forced on.
	int _fullBright;
	int _armor, _flammable, _fuel, _explosive, _explosiveType, _bigWall;
	int _sprite[8];
	int _block[6];
	int _loftID[12];
	unsigned short _miniMapIndex;
public:
	static const int O_DUMMY = 999;
	MapData(MapDataSet *dataset);
	~MapData();
	/// Gets the dataset this object belongs to.
	MapDataSet *getDataset() const;
	/// Gets the sprite index for a certain frame.
	int getSprite(int frameID) const;
	/// Sets the sprite index for a certain frame.
	void setSprite(int frameID, int value);
	/// Gets whether this is an animated ufo door.
	bool isUFODoor() const;
	/// Gets whether this is a floor.
	bool isNoFloor() const;
	/// Gets whether this is a big wall, which blocks all surrounding paths.
	int getBigWall() const;
	/// Gets whether this is a normal door.
	bool isDoor() const;
	/// Gets whether this is a grav lift.
	bool isGravLift() const;
	/// Gets whether this should be drawn behind a unit or in front of a unit (i.e. if it works as a S or E wall).
	bool isBackTileObject() const;
	/// Sets all kinds of flags.
	void setFlags(bool isUfoDoor, bool stopLOS, bool isNoFloor, int bigWall, bool isGravLift, bool isDoor, bool blockFire, bool blockSmoke, bool baseModule);
	/// Gets the amount of blockage of a certain type.
	int getBlock(ItemDamageType type) const;
	/// Sets the amount of blockage for all types.
	void setBlockValue(int lightBlock, int visionBlock, int HEBlock, int smokeBlock, int fireBlock, int gasBlock);
	/// Sets the amount of HE blockage.
	void setHEBlock(int HEBlock);
	/// Gets the offset on the Y axis when drawing this object.
	int getYOffset() const;
	/// Sets the offset on the Y axis for drawing this object.
	void setYOffset(int value);
	/// Set the type of tile.
	void setObjectType(TilePart type);
	/// Get the type of tile.
	TilePart getObjectType() const;
	/// Gets info about special tile types
	SpecialTileType getSpecialType() const;
	/// Sets a special tile type and object type.
	void setSpecialType(int value, TilePart otype);
	/// Gets the TU cost to move over the object.
	int getTUCost(MovementType movementType) const;
	/// Sets the TU cost to move over the object.
	void setTUCosts(int walk, int fly, int slide);
	/// Adds this to the graphical Y offset of units or objects on this tile.
	int getTerrainLevel() const;
	/// Sets Y offset for units/objects on this tile.
	void setTerrainLevel(int value);
	/// Gets the index to the footstep sound.
	int getFootstepSound() const;
	/// Sets the index to the footstep sound.
	void setFootstepSound(int value);
	/// Gets the alternative object ID.
	int getAltMCD() const;
	/// Sets the alternative object ID.
	void setAltMCD(int value);
	/// Gets the dead object ID.
	int getDieMCD() const;
	/// Sets the dead object ID.
	void setDieMCD(int value);
	/// Gets the effective lightSource. With Options::oxceBattleColourLightAllowOverride on, the mod-supplied value (if any) wins; otherwise the vanilla / auto-derived value is returned.
	int getLightSource() const;
	/// Sets the vanilla / auto-derived lightSource (called from MCD parsing).
	void setLightSource(int value);
	/// Sets the mod-supplied lightSource override (called from MCDPatch when AllowOverride is on).
	void setModLightSource(int value);
	/// True iff a mod-supplied lightSource override has been recorded.
	bool hasModLightSource() const { return _hasModLightSource; }
	/// True iff this part emits light on either track (vanilla _lightSource > 0, or a mod-supplied override). Bypasses Options::oxceBattleColourLightAllowOverride so callers like Mod::autoDeriveLightColors can iterate every part that could ever emit.
	bool hasAnyLightSource() const { return _lightSource > 0 || _hasModLightSource; }
	/// Gets the light emission point offset in voxel coords.
	Position getLightOffset() const;
	/// Sets the light emission point offset.
	void setLightOffset(Position offset);
	/// Gets the effective lightColour. With Options::oxceBattleColourLightAllowOverride on, the mod-supplied colour (if any) wins; otherwise the auto-derived (or vanilla default) colour is returned.
	void getLightColor(int &r, int &g, int &b) const;
	/// Sets the auto-derived (or default) lightColour.
	void setLightColor(int r, int g, int b);
	/// Sets the mod-supplied lightColour override (called from MCDPatch).
	void setModLightColor(int r, int g, int b);
	/// True iff this part should be rendered fullbright (sprite at max shade, no tint LUT). Default: legacy auto-detect from getLightSource() > 0; modders override per-part via `fullBright: true/false` in MCDPatch.
	bool getFullBright() const;
	/// Sets the fullBright override (called from MCDPatch). v=-1 unset, 0 off, 1 on.
	void setFullBright(int v) { _fullBright = v; }
	/// True iff a mod-supplied lightColour override has been recorded.
	bool hasModLightColor() const { return _hasModLightColor; }
	/// Gets the amount of armor.
	int getArmor() const;
	/// Sets the amount of armor.
	void setArmor(int value);
	/// Gets the amount of flammable.
	int getFlammable() const;
	/// Sets the amount of flammable.
	void setFlammable(int value);
	/// Gets the amount of fuel.
	int getFuel() const;
	/// Sets the amount of fuel.
	void setFuel(int value);
	/// Gets the loft index for a certain layer.
	int getLoftID(int layer) const;
	/// Sets the loft index for a certain layer.
	void setLoftID(int loft, int layer);
	/// Gets the amount of explosive.
	int getExplosive() const;
	/// Sets the amount of explosive.
	void setExplosive(int value);
	/// Gets the type of explosive.
	int getExplosiveType() const;
	/// Sets the type of explosive.
	void setExplosiveType(int value);
	/// Sets the MiniMap index
	void setMiniMapIndex(unsigned short i);
	/// Gets the MiniMap index
	unsigned short getMiniMapIndex() const;
	/// Sets the bigwall value.
	void setBigWall(const int bigWall);
	/// Sets the TUWalk value.
	void setTUWalk(const int TUWalk);
	/// Sets the TUFly value.
	void setTUFly(const int TUFly);
	/// Sets the TUSlide value.
	void setTUSlide(const int TUSlide);
	/// Check if this is an xcom base object.
	bool isBaseModule() const;
	/// Sets this tile as not a floor (water, etc.)
	void setNoFloor(bool isNoFloor);
	/// Sets this tile as not stopping LOS.
	void setStopLOS(bool stopLOS);
};

}
