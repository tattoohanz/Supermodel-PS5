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
 * Menu.h
 *
 * The PS5 game menu: the games in /data/supermodel/roms with their art (the
 * title's Assets/Menu, made by ps5/prepare-menu-art.sh), drawn by
 * MenuLayout.cpp with ImGui on the app's Vulkan context. The D-pad (or the
 * left stick) chooses, L1/R1 move a page, Cross or Options starts the game.
 * Triangle puts the D-pad on the game's settings (MenuSettings.h): up and
 * down choose one, left and right change it (saved at once), Circle or
 * Triangle goes back to the list. Square shows the game's controls
 * (MenuControls.h): there every control only lights up, to try it, and
 * Circle or Square goes back.
 */

#ifndef INCLUDED_PS5_MENU_H
#define INCLUDED_PS5_MENU_H

#include "MenuLayout.h"
#include "MenuSettings.h"

#include <map>
#include <string>
#include <vector>

namespace Vulkan3D { class CVulkanContext; }

struct MenuGame
{
  MenuEntry entry;
  std::string parent;
  std::string zipPath;
  // The art's files, found once (MenuFindArt); empty when there is none.
  std::string marquee, boxart, backdrop, logo;
  // What the game gets for each setting's keys without the menu's settings
  // (the defaults and Supermodel.ini), "1" or "0": what the menu shows when
  // it has not set one itself.
  std::map<std::string, std::string> base;
  // Its inputs (Game::INPUT_*), and its controls on the DualSense and the
  // mouse for the Controls page, from the configuration it runs with.
  uint32_t inputs = 0;
  std::vector<MenuControlRow> controls;
  std::string controlsMouse;
};

// Fills in the art's paths: the set's own, else its parent's, else an alias's.
void MenuFindArt(MenuGame &game);

class CPS5Menu
{
public:
  CPS5Menu();
  ~CPS5Menu();

  // ImGui and its Vulkan backend on the app's context, the fonts.
  bool Init(Vulkan3D::CVulkanContext &context);

  // Shows the menu until a game is started: its index in games, or -1 when
  // Cross was pressed with no games (to look again).
  int Run(const std::vector<MenuGame> &games, int selected, CMenuSettings &settings);

  // Frees the art's images (waits for the GPU), for a game to have the memory.
  void ReleaseTextures();

private:
  struct Impl;
  Impl *m;
};

#endif  // INCLUDED_PS5_MENU_H
