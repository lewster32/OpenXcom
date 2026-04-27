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
#include "OptionsAdvancedState.h"
#include <set>
#include <sstream>
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleInterface.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Engine/Options.h"
#include "../Engine/Action.h"
#include <algorithm>
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Battlescape/TileEngine.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Advanced Options window.
 * @param game Pointer to the core game.
 * @param origin Game section that originated this state.
 */
OptionsAdvancedState::OptionsAdvancedState(OptionsOrigin origin) : OptionsBaseState(origin)
{
	setCategory(_btnAdvanced);

	// Create objects
	_btnOXC = new TextButton(70, 16, 94, 8);
	_btnOXCE = new TextButton(70, 16, 168, 8);
	_btnOTHER = new TextButton(70, 16, 242, 8);
	_lstOptions = new TextList(200, 120, 94, 26);

	_owner = _btnOXC;

	_isTFTD = false;
	for (const auto& pair : Options::mods)
	{
		if (pair.second)
		{
			if (pair.first == "xcom2")
			{
				_isTFTD = true;
				break;
			}
		}
	}

	add(_btnOXC, "button", "advancedMenu");
	add(_btnOXCE, "button", "advancedMenu");
	add(_btnOTHER, "button", "advancedMenu");

	if (origin != OPT_BATTLESCAPE)
	{
		_greyedOutColor = _game->getMod()->getInterface("advancedMenu")->getElement("disabledUserOption")->color;
		add(_lstOptions, "optionLists", "advancedMenu");
	}
	else
	{
		_greyedOutColor = _game->getMod()->getInterface("battlescape")->getElement("disabledUserOption")->color;
		add(_lstOptions, "optionLists", "battlescape");
	}

	centerAllSurfaces();

	_btnOXC->setText(tr("STR_ENGINE_OXC"));
	_btnOXC->setGroup(&_owner);
	_btnOXC->onMousePress((ActionHandler)&OptionsAdvancedState::btnGroupPress, SDL_BUTTON_LEFT);

	_btnOXCE->setText(tr("STR_ENGINE_OXCE"));
	_btnOXCE->setGroup(&_owner);
	_btnOXCE->onMousePress((ActionHandler)&OptionsAdvancedState::btnGroupPress, SDL_BUTTON_LEFT);

	_btnOTHER->setText(tr("STR_ENGINE_OTHER")); // rename in your fork
	_btnOTHER->setGroup(&_owner);
	_btnOTHER->onMousePress((ActionHandler)&OptionsAdvancedState::btnGroupPress, SDL_BUTTON_LEFT);
	_btnOTHER->setVisible(false); // enable in your fork

	// how much room do we need for YES/NO
	Text text = Text(100, 9, 0, 0);
	text.initText(_game->getMod()->getFont("FONT_BIG"), _game->getMod()->getFont("FONT_SMALL"), _game->getLanguage());
	text.setText(tr("STR_YES"));
	int yes = text.getTextWidth();
	text.setText(tr("STR_NO"));
	int no = text.getTextWidth();

	int rightcol = std::max(yes, no) + 2;
	int leftcol = _lstOptions->getWidth() - rightcol;

	// Set up objects
	_lstOptions->setAlign(ALIGN_RIGHT, 1);
	_lstOptions->setColumns(2, leftcol, rightcol);
	_lstOptions->setWordWrap(true);
	_lstOptions->setSelectable(true);
	_lstOptions->setBackground(_window);
	_lstOptions->onMouseClick((ActionHandler)&OptionsAdvancedState::lstOptionsClick, 0);
	_lstOptions->onMouseOver((ActionHandler)&OptionsAdvancedState::lstOptionsMouseOver);
	_lstOptions->onMouseOut((ActionHandler)&OptionsAdvancedState::lstOptionsMouseOut);

	_colorGroup = _lstOptions->getSecondaryColor();

	for (const auto& optionInfo : Options::getOptionInfo())
	{
		if (optionInfo.type() != OPTION_KEY && !optionInfo.description().empty())
		{
			if (optionInfo.category() == "STR_GENERAL")
			{
				_settingsGeneral[optionInfo.owner()].push_back(optionInfo);
			}
			else if (optionInfo.category() == "STR_GEOSCAPE")
			{
				_settingsGeo[optionInfo.owner()].push_back(optionInfo);
			}
			else if (optionInfo.category() == "STR_BASESCAPE")
			{
				_settingsBase[optionInfo.owner()].push_back(optionInfo);
			}
			else if (optionInfo.category() == "STR_BATTLESCAPE")
			{
				_settingsBattle[optionInfo.owner()].push_back(optionInfo);
			}
			else if (optionInfo.category() == "STR_AI")
			{
				_settingsAI[optionInfo.owner()].push_back(optionInfo);
			}
		}
	}
}

/**
 *
 */
OptionsAdvancedState::~OptionsAdvancedState()
{

}

/**
 * Refreshes the UI.
 */
void OptionsAdvancedState::init()
{
	OptionsBaseState::init();

	updateList();
}

/**
 * Fills the settings list based on category.
 */
void OptionsAdvancedState::updateList()
{
	OptionOwner idx = _owner == _btnOXC ? OPTION_OXC : _owner == _btnOXCE ? OPTION_OXCE : OPTION_OTHER;

	_offsetGeneralMin = -1;
	_offsetGeneralMax = -1;
	_offsetGeoMin = -1;
	_offsetGeoMax = -1;
	_offsetBaseMin = -1;
	_offsetBaseMax = -1;
	_offsetBattleMin = -1;
	_offsetBattleMax = -1;
	_offsetAIMin = -1;
	_offsetAIMax = -1;

	_lstOptions->clearList();

	int row = -1;

	if (_settingsGeneral[idx].size() > 0)
	{
		_lstOptions->addRow(2, tr("STR_GENERAL").c_str(), "");
		row++;
		_offsetGeneralMin = row;
		_lstOptions->setCellColor(_offsetGeneralMin, 0, _colorGroup);
		addSettings(_settingsGeneral[idx]);
		row += _settingsGeneral[idx].size();
		_offsetGeneralMax = row;
	}
	if (_settingsGeo[idx].size() > 0)
	{
		if (row > -1) { _lstOptions->addRow(2, "", ""); row++; }
		_lstOptions->addRow(2, tr("STR_GEOSCAPE").c_str(), "");
		row++;
		_offsetGeoMin = row;
		_lstOptions->setCellColor(_offsetGeoMin, 0, _colorGroup);
		addSettings(_settingsGeo[idx]);
		row += _settingsGeo[idx].size();
		_offsetGeoMax = row;
	}
	if (_settingsBase[idx].size() > 0)
	{
		if (row > -1) { _lstOptions->addRow(2, "", ""); row++; }
		_lstOptions->addRow(2, tr("STR_BASESCAPE").c_str(), "");
		row++;
		_offsetBaseMin = row;
		_lstOptions->setCellColor(_offsetBaseMin, 0, _colorGroup);
		addSettings(_settingsBase[idx]);
		row += _settingsBase[idx].size();
		_offsetBaseMax = row;
	}
	if (_settingsBattle[idx].size() > 0)
	{
		if (row > -1) { _lstOptions->addRow(2, "", ""); row++; }
		_lstOptions->addRow(2, tr("STR_BATTLESCAPE").c_str(), "");
		row++;
		_offsetBattleMin = row;
		_lstOptions->setCellColor(_offsetBattleMin, 0, _colorGroup);
		addSettings(_settingsBattle[idx]);
		row += _settingsBattle[idx].size();
		_offsetBattleMax = row;
	}
	if (_settingsAI[idx].size() > 0)
	{
		if (row > -1) { _lstOptions->addRow(2, "", ""); row++; }
		_lstOptions->addRow(2, tr("STR_AI").c_str(), "");
		row++;
		_offsetAIMin = row;
		_lstOptions->setCellColor(_offsetAIMin, 0, _colorGroup);
		addSettings(_settingsAI[idx]);
		row += _settingsAI[idx].size();
		_offsetAIMax = row;
	}
}

/**
 * Adds a bunch of settings to the list.
 * @param settings List of settings.
 */
void OptionsAdvancedState::addSettings(const std::vector<OptionInfo> &settings)
{
	auto& fixeduserOptions = _game->getMod()->getFixedUserOptions();
	for (const auto& optionInfo : settings)
	{
		std::string name = tr(optionInfo.description());
		std::string value;
		if (optionInfo.type() == OPTION_BOOL)
		{
			value = *optionInfo.asBool() ? tr("STR_YES") : tr("STR_NO");
		}
		else if (optionInfo.type() == OPTION_INT)
		{
			std::ostringstream ss;
			ss << *optionInfo.asInt();
			value = ss.str();
		}
		_lstOptions->addRow(2, name.c_str(), value.c_str());
		// grey out fixed options
		auto search = fixeduserOptions.find(optionInfo.id());
		if (search != fixeduserOptions.end())
		{
			_lstOptions->setRowColor(_lstOptions->getLastRowIndex(), _greyedOutColor);
		}
		else if (isOptionDisabled(optionInfo))
		{
			_lstOptions->setCellColor(_lstOptions->getLastRowIndex(), 0, _greyedOutColor);
			_lstOptions->setCellColor(_lstOptions->getLastRowIndex(), 1, _greyedOutColor);
		}
	}
}

/**
 * Gets the currently selected setting.
 * @param sel Selected row.
 * @return Pointer to option, NULL if none selected.
 */
OptionInfo *OptionsAdvancedState::getSetting(size_t sel)
{
	int selInt = sel;
	OptionOwner idx = _owner == _btnOXC ? OPTION_OXC : _owner == _btnOXCE ? OPTION_OXCE : OPTION_OTHER;

	if (selInt > _offsetGeneralMin && selInt <= _offsetGeneralMax)
	{
		return &_settingsGeneral[idx][selInt - 1 - _offsetGeneralMin];
	}
	else if (selInt > _offsetGeoMin && selInt <= _offsetGeoMax)
	{
		return &_settingsGeo[idx][selInt - 1 - _offsetGeoMin];
	}
	else if (selInt > _offsetBaseMin && selInt <= _offsetBaseMax)
	{
		return &_settingsBase[idx][selInt - 1 - _offsetBaseMin];
	}
	else if (selInt > _offsetBattleMin && selInt <= _offsetBattleMax)
	{
		return &_settingsBattle[idx][selInt - 1 - _offsetBattleMin];
	}
	else if (selInt > _offsetAIMin && selInt <= _offsetAIMax)
	{
		return &_settingsAI[idx][selInt - 1 - _offsetAIMin];
	}
	else
	{
		return 0;
	}
}

/**
 * Rewrites the displayed value cell for the int option identified by the given
 * pointer. Scans all five settings sections and updates the matching row;
 * no-op if not found.
 */
void OptionsAdvancedState::refreshOptionRow(int *ptr)
{
	OptionOwner idx = _owner == _btnOXC ? OPTION_OXC : _owner == _btnOXCE ? OPTION_OXCE : OPTION_OTHER;
	struct { int base; const std::vector<OptionInfo> *settings; } sections[] = {
		{ _offsetGeneralMin, &_settingsGeneral[idx] },
		{ _offsetGeoMin,     &_settingsGeo[idx] },
		{ _offsetBaseMin,    &_settingsBase[idx] },
		{ _offsetBattleMin,  &_settingsBattle[idx] },
		{ _offsetAIMin,      &_settingsAI[idx] },
	};
	for (int k = 0; k < 5; ++k)
	{
		if (sections[k].base < 0) continue;
		const std::vector<OptionInfo> &s = *sections[k].settings;
		for (size_t j = 0; j < s.size(); ++j)
		{
			if (s[j].type() != OPTION_INT || s[j].asInt() != ptr) continue;
			std::ostringstream ss;
			ss << *ptr;
			_lstOptions->setCellText(sections[k].base + 1 + (int)j, 1, ss.str());
			return;
		}
	}
}

/**
 * Changes the clicked setting.
 * @param action Pointer to an action.
 */
void OptionsAdvancedState::lstOptionsClick(Action *action)
{
	Uint8 button = action->getDetails()->button.button;
	if (button != SDL_BUTTON_LEFT && button != SDL_BUTTON_RIGHT)
	{
		return;
	}
	size_t sel = _lstOptions->getSelectedRow();
	OptionInfo *setting = getSetting(sel);
	if (!setting) return;

	// greyed out options are fixed, cannot be changed by the user
	auto& fixeduserOptions = _game->getMod()->getFixedUserOptions();
	auto it = fixeduserOptions.find(setting->id());
	if (it != fixeduserOptions.end())
	{
		return;
	}

	// Locked out by an OptionGate whose controller bool is false. Refuse the
	// click silently so the value stays as it was when the gate closed -
	// re-opening the gate restores the visible/editable state.
	if (isOptionDisabled(*setting)) return;

	std::string settingText;
	if (setting->type() == OPTION_BOOL)
	{
		bool *b = setting->asBool();
		*b = !*b;
		settingText = *b ? tr("STR_YES") : tr("STR_NO");
		if (b == &Options::lazyLoadResources && !*b)
		{
			Options::reload = true; // reload when turning lazy loading off
		}
		// Recalc lighting if this option changes anything the lighting pipeline
		// depends on. oxceBattleRealisticLighting is the master gate.
		// oxceBattleColourLightAllowOverride flips which dual-track lightColor /
		// lightSource is used at runtime so a recalc rebuilds the accumulator.
		// The others alter the shape of the per-tile/per-corner data.
		if (b == &Options::oxceBattleRealisticLighting ||
			b == &Options::oxceBattleColourLightPerCorner ||
			b == &Options::oxceBattleColourLightAmbient ||
			b == &Options::oxceBattleColourLightAllowOverride)
		{
			recalculateBattleLighting();
		}
		// Toggling a bool may have changed the state of an OptionGate it
		// controls - repaint dependent rows so their disabled state stays in sync.
		refreshDisabledRowColors();
	}
	else if (setting->type() == OPTION_INT) // integer variables will need special handling
	{
		int *i = setting->asInt();

		int increment = (button == SDL_BUTTON_LEFT) ? 1 : -1; // left-click increases, right-click decreases
		if (i == &Options::changeValueByMouseWheel || i == &Options::FPS || i == &Options::FPSInactive || i == &Options::oxceWoundedDefendBaseIf
			|| i == &Options::oxceBattleSmokeOpacity || i == &Options::oxceBattleSmokeOpacityMin
			|| i == &Options::oxceBattleColourLightMix)
		{
			increment *= 10;
		}
		else if (i == &Options::oxceResearchScrollSpeedWithCtrl || i == &Options::oxceManufactureScrollSpeedWithCtrl || i == &Options::oxceReactionFireThreshold)
		{
			increment *= 5;
		}
		else if (i == &Options::oxceInterceptTableSize)
		{
			increment *= 4;
		}
		*i += increment;

		int min = 0, max = 0;
		if (i == &Options::battleExplosionHeight)
		{
			min = 0;
			max = 3;
		}
		else if (i == &Options::changeValueByMouseWheel)
		{
			min = 0;
			max = 100;
		}
		else if (i == &Options::FPS)
		{
			min = 0;
			max = 120;
		}
		else if (i == &Options::FPSInactive) {
			min = 10;
			max = 120;
		}
		else if (i == &Options::mousewheelSpeed)
		{
			min = 1;
			max = 7;
		}
		else if (i == &Options::autosaveFrequency)
		{
			min = 1;
			max = 5;
		}
		else if (i == &Options::oxceGeoAutosaveFrequency)
		{
			min = 0;
			max = 10;
		}
		else if (i == &Options::autosaveSlots || i == &Options::oxceGeoAutosaveSlots || i == &Options::oxceResearchScrollSpeed || i == &Options::oxceManufactureScrollSpeed)
		{
			min = 1;
			max = 10;
		}
		else if (i == &Options::oxceInterceptGuiMaintenanceTime || i == &Options::oxceShowETAMode || i == &Options::oxceShowAccuracyOnCrosshair || i == &Options::oxceCrashedOrLanded)
		{
			min = 0;
			max = 2;
		}
		else if (i == &Options::oxceInterceptTableSize)
		{
			min = 8;
			max = 80;
		}
		else if (i == &Options::oxceWoundedDefendBaseIf || i == &Options::oxceReactionFireThreshold) {
			min = 0;
			max = 100;
		}
		else if (i == &Options::oxceResearchScrollSpeedWithCtrl || i == &Options::oxceManufactureScrollSpeedWithCtrl)
		{
			min = 5;
			max = 50;
		}
		else if (i == &Options::oxceBattleSmokeOpacity || i == &Options::oxceBattleSmokeOpacityMin)
		{
			min = 10;
			max = 100;
		}
		else if (i == &Options::oxceBattleColourLightMix)
		{
			min = 0;
			max = 100;
		}
		else if (i == &Options::oxceBattleColourLightDither)
		{
			// 0 = None, 1 = Bayer, 2 = Floyd-Steinberg
			min = 0;
			max = 2;
		}
		else if (i == &Options::oxceAutoNightVisionThreshold) {
			min = 0;
			max = 15;
		}
		else if (i == &Options::oxceNightVisionColor)
		{
			// UFO: 1-15, TFTD: 2-16 except 8 and 10
			if (_isTFTD && ((*i) == 8 || (*i) == 10))
			{
				*i += increment;
			}
			min = _isTFTD ? 2 : 1;
			max = _isTFTD ? 16 : 15;
		}

		if (*i < min)
		{
			*i = max;
		}
		else if (*i > max)
		{
			*i = min;
		}

		// Enforce any declared co-dependent pairings (e.g. if one option must
		// be less than or equal to another, and this change has made it no
		// longer so, adjust the partner option accordingly).
		const std::vector<OptionPair> &pairings = Options::getOptionPairings();
		for (size_t p = 0; p < pairings.size(); ++p)
		{
			const OptionPair &pair = pairings[p];
			if (i != pair.lesser && i != pair.greater) continue;
			if (*pair.lesser <= *pair.greater) continue;
			int *partner = (i == pair.lesser) ? pair.greater : pair.lesser;
			*partner = *i;
			refreshOptionRow(partner);
			break;
		}

		// Mix changes the tintLUT row and finaliseTintPass output - needs a full
		// recalc so the per-tile gridIdx is rebuilt against the new mix.
		if (i == &Options::oxceBattleColourLightMix)
		{
			recalculateBattleLighting();
		}

		std::ostringstream ss;
		ss << *i;
		settingText = ss.str();
	}
	_lstOptions->setCellText(sel, 1, settingText);
}

void OptionsAdvancedState::lstOptionsMouseOver(Action *)
{
	size_t sel = _lstOptions->getSelectedRow();
	OptionInfo *setting = getSetting(sel);
	std::string desc;
	if (setting)
	{
		desc = tr(setting->description() + "_DESC");
	}
	_txtTooltip->setText(desc);
}

void OptionsAdvancedState::lstOptionsMouseOut(Action *)
{
	_txtTooltip->setText("");
}

void OptionsAdvancedState::btnGroupPress(Action*)
{
	updateList();
}

/**
 * True when this option is gated off by an Options::OptionGate whose
 * controller bool is currently false. Compares the option's storage pointer
 * (asBool / asInt) against each gate's dependent list; pointer equality is
 * sufficient since every option has a unique storage address.
 */
bool OptionsAdvancedState::isOptionDisabled(const OptionInfo &setting) const
{
	void *p = (setting.type() == OPTION_BOOL) ? (void*)setting.asBool() :
	          (setting.type() == OPTION_INT)  ? (void*)setting.asInt()  : 0;
	if (!p) return false;
	const std::vector<OptionGate> &gates = Options::getOptionGates();
	for (size_t g = 0; g < gates.size(); ++g)
	{
		if (*gates[g].gate) continue; // gate open - dependents allowed
		for (size_t d = 0; d < gates[g].dependents.size(); ++d)
		{
			if (gates[g].dependents[d] == p) return true;
		}
	}
	return false;
}

/**
 * Walks all settings sections and re-applies the dim/normal colour to each
 * row whose option appears in any OptionGate's dependent list. Both columns
 * are dimmed for disabled rows so the value display also signals the disabled
 * state. Options that are not gated are left untouched.
 */
void OptionsAdvancedState::refreshDisabledRowColors()
{
	const std::vector<OptionGate> &gates = Options::getOptionGates();
	std::set<void*> gateable;
	for (size_t g = 0; g < gates.size(); ++g)
	{
		for (size_t d = 0; d < gates[g].dependents.size(); ++d)
		{
			gateable.insert(gates[g].dependents[d]);
		}
	}
	if (gateable.empty()) return;

	OptionOwner idx = _owner == _btnOXC ? OPTION_OXC : _owner == _btnOXCE ? OPTION_OXCE : OPTION_OTHER;
	struct Section { int base; const std::vector<OptionInfo> *settings; };
	Section sections[5] = {
		{ _offsetGeneralMin, &_settingsGeneral[idx] },
		{ _offsetGeoMin,     &_settingsGeo[idx] },
		{ _offsetBaseMin,    &_settingsBase[idx] },
		{ _offsetBattleMin,  &_settingsBattle[idx] },
		{ _offsetAIMin,      &_settingsAI[idx] },
	};
	const Uint8 defaultColor = _lstOptions->getColor();
	for (int s = 0; s < 5; ++s)
	{
		if (sections[s].base < 0) continue;
		const std::vector<OptionInfo> &v = *sections[s].settings;
		for (size_t i = 0; i < v.size(); ++i)
		{
			void *p = (v[i].type() == OPTION_BOOL) ? (void*)v[i].asBool() :
			          (v[i].type() == OPTION_INT)  ? (void*)v[i].asInt()  : 0;
			if (!p || !gateable.count(p)) continue;
			const size_t row = (size_t)(sections[s].base + 1 + (int)i);
			const Uint8 color = isOptionDisabled(v[i]) ? _greyedOutColor : defaultColor;
			_lstOptions->setCellColor(row, 0, color);
			_lstOptions->setCellColor(row, 1, color);
		}
	}
}

/**
 * Triggers a full lighting recalc on the active battle (no-op if no battle is
 * running). Needed after toggling any option that changes how light is computed
 * or how light data is consumed by the renderer.
 */
void OptionsAdvancedState::recalculateBattleLighting()
{
	SavedGame *save = _game->getSavedGame();
	if (!save) return;
	SavedBattleGame *battle = save->getSavedBattle();
	if (!battle) return;
	TileEngine *te = battle->getTileEngine();
	if (!te) return;
	te->recalculateLighting();
}

}
