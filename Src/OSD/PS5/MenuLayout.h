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
 * MenuLayout.h
 *
 * The game menu's picture, after SEGA's ALL.Net P-ras MULTI: the games'
 * marquees in a column on the left, the chosen game's box art in the large
 * frame on the right, its name and details under it. Drawn with ImGui's draw
 * lists only, in units of a 1920x1080 screen scaled to the real one, so the
 * same code runs on the console (Menu.cpp) and in the host preview
 * (ps5/tools/menu-preview.cpp).
 */

#ifndef INCLUDED_PS5_MENULAYOUT_H
#define INCLUDED_PS5_MENULAYOUT_H

#include "imgui/imgui.h"

#include <cstdint>
#include <string>
#include <vector>

struct MenuImage
{
  ImTextureID id = ImTextureID_Invalid;  // invalid: not there (yet)
  float width = 0.0f, height = 0.0f;     // pixels
  float alpha = 1.0f;                    // fade-in
  bool Valid() const { return id != ImTextureID_Invalid && width > 0.0f && height > 0.0f; }
};

struct MenuEntry
{
  std::string set;           // ROM set
  std::string title;         // from Games.xml
  std::string manufacturer;
  std::string version;
  std::string hardware;      // "Model 3 Step 2.0"
  std::string sound;         // "DSB2", or empty
  unsigned year = 0;
  MenuImage marquee;
};

// One of the chosen game's settings (MenuSettings.h), as shown.
struct MenuSettingRow
{
  std::string label;   // "Picture"
  std::string value;   // "16:9"
};

// The DualSense's controls, for the Controls page.
enum MenuPadControl
{
  MENU_PAD_CROSS, MENU_PAD_CIRCLE, MENU_PAD_SQUARE, MENU_PAD_TRIANGLE,
  MENU_PAD_L1, MENU_PAD_R1, MENU_PAD_L2, MENU_PAD_R2, MENU_PAD_L3, MENU_PAD_R3,
  MENU_PAD_CREATE, MENU_PAD_OPTIONS, MENU_PAD_TOUCHPAD,
  MENU_PAD_UP, MENU_PAD_DOWN, MENU_PAD_LEFT, MENU_PAD_RIGHT,
  MENU_PAD_LSTICK, MENU_PAD_RSTICK,
  MENU_PAD__COUNT
};

// One of a game's controls on the Controls page (MenuControls.h): what it
// does, on which of the DualSense's controls.
struct MenuControlRow
{
  std::string function;                     // "Accelerator"
  MenuPadControl control = MENU_PAD_CROSS;  // where its line points
  std::string name;                         // "R2", "Left stick left"
  int ways = 0;                             // a stick's whole axis: 1 left and right, 2 up and down
  bool system = false;                      // Start, Coin, Service, Test, the menu
};

struct MenuFrame
{
  const std::vector<MenuEntry> *games = nullptr;
  int selected = 0;
  float scroll = 0.0f;         // the list's position, eased towards selected
  double time = 0.0;           // seconds, for the pulses
  MenuImage boxart;            // of the selected game
  MenuImage backdrop;          // its box art blurred, behind it
  MenuImage logo;              // its clear logo
  MenuImage headerLogo;        // the title's own (SEGA MODEL 3)
  ImFont *bold = nullptr;
  ImFont *medium = nullptr;
  const char *message = nullptr;  // shown in the panel instead of a game (no games)
  std::vector<MenuSettingRow> settings;  // the chosen game's
  bool settingsFocus = false;            // Triangle: the D-pad works on the settings
  int settingsRow = 0;
  // The Controls page (Square): the chosen game's controls, what a mouse
  // does in it ("" none), and the DualSense's controls held now (a bit per
  // MenuPadControl), lit.
  bool controlsOpen = false;
  const std::vector<MenuControlRow> *controls = nullptr;
  std::string controlsMouse;
  uint32_t padLive = 0;
};

void MenuDraw(ImDrawList *draw, const MenuFrame &frame, ImVec2 origin, float scale);

#endif  // INCLUDED_PS5_MENULAYOUT_H
