/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **/

/*
 * MenuControls.h
 *
 * A game's controls on the DualSense, for the menu's Controls page: read
 * from the configuration the game runs with (its Input* keys: the defaults,
 * Supermodel.ini, Main.cpp's GameConfig) for the inputs it has (Game::INPUT_*,
 * the core's own list in Inputs.cpp), so a mapping changed in Supermodel.ini
 * shows as it is.
 */

#ifndef INCLUDED_PS5_MENUCONTROLS_H
#define INCLUDED_PS5_MENUCONTROLS_H

#include "MenuLayout.h"
#include "Util/NewConfig.h"

#include <cstdint>
#include <string>
#include <vector>

// A row for each of the pad's controls in each of a control's mappings,
// the PS5 front's own (L1 in the analog joystick games, L3 held for the
// menu) after them; what a mouse does, or "".
void MenuControlsFor(const Util::Config::Node &config, uint32_t gameInputs, std::vector<MenuControlRow> &rows,
  std::string &mouse);

#endif  // INCLUDED_PS5_MENUCONTROLS_H
