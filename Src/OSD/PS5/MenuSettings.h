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
 * MenuSettings.h
 *
 * The settings each game gets in the menu (Triangle), kept in
 * /data/supermodel/Config/MenuSettings.ini, one section per ROM set:
 *
 *   [ srally2 ]
 *   WideScreen = 1
 *   WideBackground = 1
 *
 * applied over everything else when the game starts (defaults, built-in,
 * Supermodel.ini). Written by the menu only; a setting the menu has not
 * changed for a game is not in the file, and the game keeps what
 * Supermodel.ini says.
 */

#ifndef INCLUDED_PS5_MENUSETTINGS_H
#define INCLUDED_PS5_MENUSETTINGS_H

#include <map>
#include <string>
#include <vector>

// One setting as the menu shows it: a name and a few values, each value
// being a set of config keys.
struct MenuSettingChoice
{
  const char *label;                                  // "16:9"
  std::vector<std::pair<const char *, const char *>> keys;  // { "WideScreen", "1" }, ...
};

struct MenuSettingDef
{
  const char *label;                                  // "Picture"
  std::vector<MenuSettingChoice> choices;
};

const std::vector<MenuSettingDef> &MenuSettingDefs();

// A value as compared: "true"/"on"/"yes" as "1", the false ones as "0",
// numbers in one form ("120", "120.0" -> "120").
std::string MenuSettingNormal(const std::string &value);

class CMenuSettings
{
public:
  bool Load(const std::string &path);
  bool Save() const;

  // The value the menu set for this game, or nullptr.
  const std::string *Get(const std::string &set, const std::string &key) const;
  void Set(const std::string &set, const std::string &key, const std::string &value);
  const std::map<std::string, std::string> *Section(const std::string &set) const;

private:
  std::string m_path;
  std::map<std::string, std::map<std::string, std::string>> m_sections;
};

#endif  // INCLUDED_PS5_MENUSETTINGS_H
