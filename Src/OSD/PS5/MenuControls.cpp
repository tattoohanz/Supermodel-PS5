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
 * MenuControls.cpp
 *
 * A mapping is read as InputSystem.cpp parses it (ParseMultiSource): ","
 * between alternatives, "+" between sources pressed together, "!" for one
 * that must not be. Its tokens are the DualSense's in PS5InputSystem.h's
 * numbering (JOY1_ or JOY_), or the mouse's. Keys (there is no keyboard),
 * other joysticks and the triggers' negative halves (the DualSense's never
 * go below 0) cannot be pressed here and are left out; so is a negated
 * source.
 */

#include "MenuControls.h"

#include "Game.h"
#include "Inputs/Input.h"
#include "Inputs/Inputs.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace
{
  // JOY1_BUTTON1-11 (PS5InputSystem.h).
  struct ButtonName { MenuPadControl control; const char *name; };
  const ButtonName kButtons[] =
  {
    { MENU_PAD_CROSS, "Cross" }, { MENU_PAD_CIRCLE, "Circle" }, { MENU_PAD_SQUARE, "Square" },
    { MENU_PAD_TRIANGLE, "Triangle" }, { MENU_PAD_L1, "L1" }, { MENU_PAD_R1, "R1" },
    { MENU_PAD_CREATE, "Create" }, { MENU_PAD_OPTIONS, "Options" }, { MENU_PAD_L3, "L3" },
    { MENU_PAD_R3, "R3" }, { MENU_PAD_TOUCHPAD, "Touchpad" },
  };

  enum SourceKind { SOURCE_NONE, SOURCE_PAD, SOURCE_MOUSE };

  struct Source
  {
    SourceKind kind = SOURCE_NONE;
    MenuPadControl control = MENU_PAD_CROSS;
    std::string name;   // the pad's, as the page says it
    int ways = 0;       // MenuControlRow::ways
    std::string part;   // the mouse's, as InputSystem.cpp names it
  };

  bool EndsWith(const std::string &s, const char *end)
  {
    const size_t n = strlen(end);
    return s.size() >= n && s.compare(s.size() - n, n, end) == 0;
  }

  // One source of a mapping, the way the page shows it.
  Source Translate(std::string t)
  {
    Source s;
    while (!t.empty() && (isspace((unsigned char)t.front()) || t.front() == '"'))
      t.erase(0, 1);
    while (!t.empty() && (isspace((unsigned char)t.back()) || t.back() == '"'))
      t.pop_back();
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char ch) { return (char)toupper(ch); });
    const size_t us = t.find('_');
    if (t.empty() || t[0] == '!' || us == std::string::npos)
      return s;
    const std::string device = t.substr(0, us);
    std::string part = t.substr(us + 1);
    if (device == "MOUSE" || device == "MOUSE1")
    {
      s.kind = SOURCE_MOUSE;
      s.part = part;
      return s;
    }
    if (device != "JOY" && device != "JOY1")
      return s;

    if (part.compare(0, 6, "BUTTON") == 0)
    {
      const int n = atoi(part.c_str() + 6);
      if (n >= 1 && n <= (int)(sizeof(kButtons) / sizeof(kButtons[0])))
      {
        s.kind = SOURCE_PAD;
        s.control = kButtons[n - 1].control;
        s.name = kButtons[n - 1].name;
      }
      return s;
    }
    static const struct { const char *part; MenuPadControl control; const char *name; } pov[] =
    {
      { "POV1_UP", MENU_PAD_UP, "D-pad up" }, { "POV1_DOWN", MENU_PAD_DOWN, "D-pad down" },
      { "POV1_LEFT", MENU_PAD_LEFT, "D-pad left" }, { "POV1_RIGHT", MENU_PAD_RIGHT, "D-pad right" },
    };
    for (const auto &p : pov)
      if (part == p.part)
      {
        s.kind = SOURCE_PAD;
        s.control = p.control;
        s.name = p.name;
        return s;
      }

    // The axes, whole or a half (_POS, _NEG; _INV turned round is still the
    // whole axis), by name or number; LEFT/RIGHT/UP/DOWN are the left stick's halves.
    std::string half;
    for (const char *suffix : { "_INV", "_POS", "_NEG" })
      if (EndsWith(part, suffix))
      {
        half = suffix + 1;
        part.erase(part.size() - 4);
        break;
      }
    static const struct { const char *alias, *axis, *half; } aliases[] =
    {
      { "LEFT", "XAXIS", "NEG" }, { "RIGHT", "XAXIS", "POS" }, { "UP", "YAXIS", "NEG" }, { "DOWN", "YAXIS", "POS" },
      { "AXIS1", "XAXIS", nullptr }, { "AXIS2", "YAXIS", nullptr }, { "AXIS3", "ZAXIS", nullptr },
      { "AXIS4", "RXAXIS", nullptr }, { "AXIS5", "RYAXIS", nullptr }, { "AXIS6", "RZAXIS", nullptr },
    };
    for (const auto &a : aliases)
      if (part == a.alias)
      {
        part = a.axis;
        if (a.half)
          half = a.half;
        break;
      }
    if (part == "ZAXIS" || part == "RZAXIS")
    {
      if (half == "NEG")
        return s;
      s.kind = SOURCE_PAD;
      s.control = part == "ZAXIS" ? MENU_PAD_L2 : MENU_PAD_R2;
      s.name = part == "ZAXIS" ? "L2" : "R2";
      return s;
    }
    const bool leftStick = part == "XAXIS" || part == "YAXIS";
    if (!leftStick && part != "RXAXIS" && part != "RYAXIS")
      return s;
    const bool across = part == "XAXIS" || part == "RXAXIS";
    s.kind = SOURCE_PAD;
    s.control = leftStick ? MENU_PAD_LSTICK : MENU_PAD_RSTICK;
    s.name = leftStick ? "Left stick" : "Right stick";
    if (half == "POS")
      s.name += across ? " right" : " down";
    else if (half == "NEG")
      s.name += across ? " left" : " up";
    else
      s.ways = across ? 1 : 2;
    return s;
  }

  // An input's label as the page shows it: the core's, without "P1 ", a few shorter.
  std::string Label(const CInput &input)
  {
    static const struct { const char *id, *label; } own[] =
    {
      { "Steering", "Steering" }, { "Accelerator", "Accelerator" }, { "Brake", "Brake" },
      { "ServiceA", "Service" }, { "TestA", "Test" },
    };
    for (const auto &o : own)
      if (!strcmp(input.id, o.id))
        return o.label;
    std::string label = input.label;
    if (label.compare(0, 3, "P1 ") == 0)
      label.erase(0, 3);
    return label;
  }

  // A mouse source as the page says it, or nullptr for one it does not show.
  const char *MouseWord(const std::string &part)
  {
    static const struct { const char *part, *word; } words[] =
    {
      { "XAXIS", "move" }, { "YAXIS", "move" }, { "AXIS1", "move" }, { "AXIS2", "move" },
      { "LEFT_BUTTON", "left button" }, { "BUTTON1", "left button" },
      { "MIDDLE_BUTTON", "middle button" }, { "BUTTON2", "middle button" },
      { "RIGHT_BUTTON", "right button" }, { "BUTTON3", "right button" },
      { "BUTTONX1", "side button" }, { "BUTTON4", "side button" },
      { "BUTTONX2", "second side button" }, { "BUTTON5", "second side button" },
    };
    for (const auto &w : words)
      if (part == w.part)
        return w.word;
    return nullptr;
  }

  // "Analog Gun X-Axis" moved by the mouse is the "Analog Gun".
  std::string Moved(std::string label)
  {
    for (const char *axis : { " X-Axis", " Y-Axis" })
      if (EndsWith(label, axis))
        label.erase(label.size() - strlen(axis));
    return label;
  }
}

void MenuControlsFor(const Util::Config::Node &config, uint32_t gameInputs, std::vector<MenuControlRow> &rows,
  std::string &mouse)
{
  rows.clear();
  mouse.clear();
  std::vector<std::pair<std::string, std::string>> mouseUses;  // word, label

  // Only to list the core's inputs: with no input system, nothing is read.
  CInputs inputs(nullptr);
  for (unsigned i = 0; i < inputs.Count(); i++)
  {
    const std::shared_ptr<CInput> input = inputs[i];
    if (!input->IsConfigurable() || !(input->gameFlags & gameInputs))
      continue;
    const Util::Config::Node *node = config.TryGet(std::string("Input") + input->id);
    if (!node || node->Empty())
      continue;
    const std::string mapping = node->ValueAs<std::string>();
    const std::string label = Label(*input);
    const bool system = input->gameFlags == Game::INPUT_COMMON;
    for (size_t start = 0; start <= mapping.size();)
    {
      size_t end = mapping.find(',', start);
      if (end == std::string::npos)
        end = mapping.size();
      const std::string alternative = mapping.substr(start, end - start);
      start = end + 1;

      std::vector<Source> sources;
      bool usable = true;
      for (size_t p = 0; p <= alternative.size() && usable;)
      {
        size_t q = alternative.find('+', p);
        if (q == std::string::npos)
          q = alternative.size();
        Source s = Translate(alternative.substr(p, q - p));
        p = q + 1;
        usable = s.kind != SOURCE_NONE;
        sources.push_back(s);
      }
      if (!usable || sources.empty())
        continue;
      if (sources.size() == 1 && sources[0].kind == SOURCE_MOUSE)
      {
        if (const char *word = MouseWord(sources[0].part))
        {
          std::pair<std::string, std::string> use(word, !strcmp(word, "move") ? Moved(label) : label);
          if (std::find(mouseUses.begin(), mouseUses.end(), use) == mouseUses.end())
            mouseUses.push_back(use);
        }
        continue;
      }
      if (std::any_of(sources.begin(), sources.end(), [](const Source &s) { return s.kind != SOURCE_PAD; }))
        continue;

      MenuControlRow row;
      row.function = label;
      row.control = sources[0].control;
      row.ways = sources.size() == 1 ? sources[0].ways : 0;
      row.system = system;
      for (size_t k = 0; k < sources.size(); k++)
        row.name += (k ? " + " : "") + sources[k].name;
      const bool seen = std::any_of(rows.begin(), rows.end(), [&](const MenuControlRow &r)
      {
        return r.function == row.function && r.name == row.name;
      });
      if (!seen)
        rows.push_back(row);
    }
  }

  // The PS5 front's own (Main.cpp's session loop).
  if (gameInputs & Game::INPUT_ANALOG_JOYSTICK)
  {
    MenuControlRow row;
    row.function = "Invert stick";
    row.control = MENU_PAD_L1;
    row.name = "L1";
    rows.push_back(row);
  }
  MenuControlRow menu;
  menu.function = "Menu (hold 2 s)";
  menu.control = MENU_PAD_L3;
  menu.name = "L3";
  menu.system = true;
  rows.push_back(menu);

  for (const auto &use : mouseUses)
    mouse += (mouse.empty() ? "Mouse - " : "  \xC2\xB7  ") + use.first + ": " + use.second;  // U+00B7
}
