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
#include <cstdlib>
#include <cstring>

/*
 * The settings, from the desktop build's (OSD/SDL/Gui.cpp) that work on the
 * PS5's Vulkan renderer. Each choice sets all its keys; the menu shows the
 * choice whose keys all match what the game gets, else the first whose
 * first key does. The first three are also shown while the box is closed.
 * All apply when the game starts.
 */
const std::vector<MenuSettingDef> &MenuSettingDefs()
{
  static const std::vector<MenuSettingDef> defs = {
    // The Model 3's own 4:3; Supermodel's widescreen, where the 3D scene
    // and the background layer fill the width (the HUD stays 4:3 in the
    // middle); or the 4:3 picture stretched over the screen.
    { "Picture", {
      { "4:3", { { "WideScreen", "0" }, { "WideBackground", "0" }, { "Stretch", "0" } } },
      { "16:9", { { "WideScreen", "1" }, { "WideBackground", "1" }, { "Stretch", "0" } } },
      { "Stretched", { { "WideScreen", "0" }, { "WideBackground", "0" }, { "Stretch", "1" } } },
    } },
    // The emulated PowerPC's clock: below the real one a game needs less of
    // the console (Sega Rally 2 is at 120 MHz by default for 60 FPS).
    { "PowerPC", {
      { "Real", { { "PowerPCFrequency", "0" } } },
      { "166 MHz", { { "PowerPCFrequency", "166" } } },
      { "150 MHz", { { "PowerPCFrequency", "150" } } },
      { "133 MHz", { { "PowerPCFrequency", "133" } } },
      { "120 MHz", { { "PowerPCFrequency", "120" } } },
      { "100 MHz", { { "PowerPCFrequency", "100" } } },
    } },
    // The desktop's CRT presets (Gui.cpp): 720 or 360 scanlines on 2160 rows.
    { "CRT filter", {
      { "Off", { { "PostCRT", "0" } } },
      { "Scanlines", { { "PostCRT", "1" }, { "PostScanlines", "0" }, { "PostScanlineIntensity", "60" } } },
      { "CRT fine", { { "PostCRT", "2" }, { "PostMaskType", "1" }, { "PostScanlines", "720" },
        { "PostMaskScale", "3" }, { "PostScanlineIntensity", "70" }, { "PostMaskIntensity", "35" }, { "PostGain", "167" } } },
      { "CRT arcade", { { "PostCRT", "2" }, { "PostMaskType", "1" }, { "PostScanlines", "360" },
        { "PostMaskScale", "3" }, { "PostScanlineIntensity", "70" }, { "PostMaskIntensity", "35" }, { "PostGain", "167" } } },
    } },
    { "Anti-aliasing", {
      { "Off", { { "PostFXAA", "0" }, { "MSAA", "0" } } },
      { "FXAA", { { "PostFXAA", "1" }, { "MSAA", "0" } } },
      { "MSAA 2x", { { "PostFXAA", "0" }, { "MSAA", "2" } } },
      { "MSAA 4x", { { "PostFXAA", "0" }, { "MSAA", "4" } } },
    } },
    // The game's own bezel (Bezels/<set>.png) beside a 4:3 picture.
    { "Bezel", {
      { "On", { { "BezelMode", "1" } } },
      { "Off", { { "BezelMode", "0" } } },
    } },
    { "Glow", {
      { "Off", { { "PostGlow", "0" } } },
      { "Soft", { { "PostGlow", "30" } } },
      { "Strong", { { "PostGlow", "60" } } },
    } },
    { "Music", {
      { "0%", { { "MusicVolume", "0" } } }, { "25%", { { "MusicVolume", "25" } } }, { "50%", { { "MusicVolume", "50" } } },
      { "75%", { { "MusicVolume", "75" } } }, { "100%", { { "MusicVolume", "100" } } }, { "125%", { { "MusicVolume", "125" } } },
      { "150%", { { "MusicVolume", "150" } } }, { "200%", { { "MusicVolume", "200" } } },
    } },
    { "Sound effects", {
      { "0%", { { "SoundVolume", "0" } } }, { "25%", { { "SoundVolume", "25" } } }, { "50%", { { "SoundVolume", "50" } } },
      { "75%", { { "SoundVolume", "75" } } }, { "100%", { { "SoundVolume", "100" } } }, { "125%", { { "SoundVolume", "125" } } },
      { "150%", { { "SoundVolume", "150" } } }, { "200%", { { "SoundVolume", "200" } } },
    } },
  };
  return defs;
}

std::string MenuSettingNormal(const std::string &value)
{
  std::string v;
  for (char ch : value)
    if (ch != ' ' && ch != '\t')
      v += (char)tolower((unsigned char)ch);
  if (v == "true" || v == "yes" || v == "on")
    return "1";
  if (v == "false" || v == "no" || v == "off")
    return "0";
  char *end = nullptr;
  const double d = strtod(v.c_str(), &end);
  if (!v.empty() && end && *end == '\0')
  {
    char number[32];
    snprintf(number, sizeof(number), "%g", d);
    return number;
  }
  return v;
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
