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
 * MenuSettings.cpp
 */

#include "MenuSettings.h"
#include "Supermodel.h"

#include <cstdio>
#include <cstring>

const std::vector<MenuSettingDef> &MenuSettingDefs()
{
  // Picture: the Model 3's own 4:3, or Supermodel's widescreen, where the 3D
  // scene fills the width (and the background layer is stretched with it;
  // the 2D layer over the scene, the HUD, stays 4:3 in the middle).
  static const std::vector<MenuSettingDef> defs = {
    { "Picture", {
      { "4:3", { { "WideScreen", "0" }, { "WideBackground", "0" } } },
      { "16:9", { { "WideScreen", "1" }, { "WideBackground", "1" } } },
    } },
  };
  return defs;
}

static std::string Trim(const std::string &s)
{
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n'))
    a++;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n'))
    b--;
  return s.substr(a, b - a);
}

bool CMenuSettings::Load(const std::string &path)
{
  m_path = path;
  m_sections.clear();
  FILE *f = fopen(path.c_str(), "r");
  if (!f)
    return false;
  char line[512];
  std::string section;
  while (fgets(line, sizeof(line), f))
  {
    std::string s = Trim(line);
    if (s.empty() || s[0] == ';' || s[0] == '#')
      continue;
    if (s.front() == '[' && s.back() == ']')
    {
      section = Trim(s.substr(1, s.size() - 2));
      continue;
    }
    const size_t eq = s.find('=');
    if (eq == std::string::npos || section.empty())
      continue;
    m_sections[section][Trim(s.substr(0, eq))] = Trim(s.substr(eq + 1));
  }
  fclose(f);
  return true;
}

bool CMenuSettings::Save() const
{
  const std::string temp = m_path + ".tmp";
  FILE *f = fopen(temp.c_str(), "w");
  if (!f)
  {
    ErrorLog("Menu: '%s' cannot be written.", temp.c_str());
    return false;
  }
  fprintf(f, "; Written by the PS5 menu (Triangle on a game). Applied over Supermodel.ini.\n");
  for (const auto &section : m_sections)
  {
    if (section.second.empty())
      continue;
    fprintf(f, "\n[ %s ]\n", section.first.c_str());
    for (const auto &kv : section.second)
      fprintf(f, "%s = %s\n", kv.first.c_str(), kv.second.c_str());
  }
  const bool ok = fflush(f) == 0;
  fclose(f);
  if (!ok || rename(temp.c_str(), m_path.c_str()) != 0)
  {
    ErrorLog("Menu: '%s' could not be saved.", m_path.c_str());
    remove(temp.c_str());
    return false;
  }
  return true;
}

const std::string *CMenuSettings::Get(const std::string &set, const std::string &key) const
{
  auto s = m_sections.find(set);
  if (s == m_sections.end())
    return nullptr;
  auto v = s->second.find(key);
  return v == s->second.end() ? nullptr : &v->second;
}

void CMenuSettings::Set(const std::string &set, const std::string &key, const std::string &value)
{
  m_sections[set][key] = value;
}

const std::map<std::string, std::string> *CMenuSettings::Section(const std::string &set) const
{
  auto s = m_sections.find(set);
  return s == m_sections.end() ? nullptr : &s->second;
}
