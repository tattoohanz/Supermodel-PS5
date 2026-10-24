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
 * PS5InputSystem.cpp
 *
 * The libScePad calls and the layout of the pad's state are XPSemu Chihiro's
 * (ui/xemu-input-ps5.c), which has them from PS5SX2.
 */

#include "PS5InputSystem.h"
#include "PS5App.h"
#include "PS5Mouse.h"
#include "PS5Pad.h"
#include "Supermodel.h"
#include "Inputs/Input.h"

#include <algorithm>
#include <cstring>

namespace
{
  enum : uint32_t
  {
    PAD_CREATE = PS5_PAD_CREATE,
    PAD_L3 = PS5_PAD_L3,
    PAD_R3 = PS5_PAD_R3,
    PAD_OPTIONS = PS5_PAD_OPTIONS,
    PAD_UP = PS5_PAD_UP,
    PAD_RIGHT = PS5_PAD_RIGHT,
    PAD_DOWN = PS5_PAD_DOWN,
    PAD_LEFT = PS5_PAD_LEFT,
    PAD_L1 = PS5_PAD_L1,
    PAD_R1 = PS5_PAD_R1,
    PAD_TRIANGLE = PS5_PAD_TRIANGLE,
    PAD_CIRCLE = PS5_PAD_CIRCLE,
    PAD_CROSS = PS5_PAD_CROSS,
    PAD_SQUARE = PS5_PAD_SQUARE,
    PAD_TOUCHPAD = PS5_PAD_TOUCHPAD,
  };

  // JOY1_BUTTON1.. in SDL game controller order (see PS5InputSystem.h).
  const uint32_t kButtonBits[] = {
    PAD_CROSS, PAD_CIRCLE, PAD_SQUARE, PAD_TRIANGLE, PAD_L1, PAD_R1,
    PAD_CREATE, PAD_OPTIONS, PAD_L3, PAD_R3, PAD_TOUCHPAD,
  };
  const char *const kButtonNames[] = {
    "Cross (JOY1_BUTTON1)", "Circle (JOY1_BUTTON2)", "Square (JOY1_BUTTON3)", "Triangle (JOY1_BUTTON4)",
    "L1 (JOY1_BUTTON5)", "R1 (JOY1_BUTTON6)", "Create (JOY1_BUTTON7)", "Options (JOY1_BUTTON8)",
    "L3 (JOY1_BUTTON9)", "R3 (JOY1_BUTTON10)", "touchpad (JOY1_BUTTON11)",
  };
  constexpr int kNumButtons = (int)(sizeof(kButtonBits) / sizeof(kButtonBits[0]));

  const uint32_t kPovBits[] = { PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT };
  const char *const kPovNames[] = { "D-pad up", "D-pad down", "D-pad left", "D-pad right" };

  int Stick(uint8_t v)
  {
    int value = ((int)v - 128) * 256;
    return value > 32767 ? 32767 : value;
  }

  bool s_leftStickInverted = false;

  int Inverted(int value)
  {
    return s_leftStickInverted ? (value <= -32767 ? 32767 : -value) : value;
  }

  int Trigger(uint8_t v)
  {
    return (int)v * 32767 / 255;
  }
}

CPS5InputSystem::CPS5InputSystem(const Util::Config::Node &config) : CInputSystem("ps5")
{
  memset(&m_joyDetails, 0, sizeof(m_joyDetails));
  strncpy(m_joyDetails.name, "DualSense", MAX_NAME_LENGTH);
  m_joyDetails.numAxes = 6;
  m_joyDetails.numPOVs = 1;
  m_joyDetails.numButtons = kNumButtons;
  // The motors take the force feedback of whatever is on the sticks (the
  // wheel's steering, Star Wars Trilogy's joystick).
  m_joyDetails.hasFFeedback = true;
  for (int axis : { AXIS_X, AXIS_Y, AXIS_RX, AXIS_RY })
    m_joyDetails.axisHasFF[axis] = true;
  auto percent = [&config](const char *key, unsigned fallback)
  {
    const Util::Config::Node *node = config.TryGet(key);
    return (float)std::min(200u, node ? node->ValueAsDefault<unsigned>(fallback) : fallback) / 100.0f;
  };
  m_ffThreshold = percent("XInputConstForceThreshold", 30);
  m_ffConstMax = percent("XInputConstForceMax", 100);
  m_ffVibrateMax = percent("XInputVibrateMax", 100);
  const Util::Config::Node *stereo = config.TryGet("XInputStereoVibration");
  m_ffStereo = stereo ? stereo->ValueAsDefault<bool>(true) : true;
  static const char *const axisNames[] = { "Left stick X", "Left stick Y", "L2", "Right stick X", "Right stick Y", "R2" };
  const int axes[] = { AXIS_X, AXIS_Y, AXIS_Z, AXIS_RX, AXIS_RY, AXIS_RZ };
  for (int i = 0; i < 6; i++)
  {
    m_joyDetails.hasAxis[axes[i]] = true;
    strncpy(m_joyDetails.axisName[axes[i]], axisNames[i], MAX_NAME_LENGTH);
  }
}

CPS5InputSystem::~CPS5InputSystem()
{
  // The session is over: the motors stop with it.
  PS5PadVibrate(0, 0);
}

void CPS5InputSystem::SetLeftStickInverted(bool inverted)
{
  s_leftStickInverted = inverted;
}

bool CPS5InputSystem::InitializeSystem()
{
  // The pad is the app's (PS5Pad.cpp): opened on its first read, shared
  // with the menu.
  PS5PadRead();
  return true;
}

bool CPS5InputSystem::Poll()
{
  // The mouse: its movement moves the pointer over the game's picture.
  const PS5MouseInput mouse = PS5MouseRead();
  m_mouseOpen = mouse.open;
  m_mouseButtons = mouse.buttons;
  m_mouseWheel = mouse.wheel;
  if (m_dispW > 0 && m_dispH > 0)
  {
    const float x0 = (float)m_dispX, y0 = (float)m_dispY;
    const float x1 = x0 + (float)m_dispW - 1.0f, y1 = y0 + (float)m_dispH - 1.0f;
    if (!m_mouseCentred)
    {
      m_mouseCentred = true;
      m_mouseX = x0 + m_dispW * 0.5f;
      m_mouseY = y0 + m_dispH * 0.5f;
    }
    const float k = (float)m_dispW / 1000.0f;  // 1000 counts across the picture
    m_mouseX = std::min(x1, std::max(x0, m_mouseX + mouse.dx * k));
    m_mouseY = std::min(y1, std::max(y0, m_mouseY + mouse.dy * k));
  }

  const PS5PadState d = PS5PadRead();
  UpdateMotors();
  m_stateValid = d.valid;
  m_buttons = d.buttons;
  m_lx = d.lx;
  m_ly = d.ly;
  m_rx = d.rx;
  m_ry = d.ry;
  m_l2 = d.l2;
  m_r2 = d.r2;
  if (!m_stateValid)
    return true;

  // The first press of each control, once, to check the layout against the log.
  for (int i = 0; i < kNumButtons; i++)
  {
    const uint32_t bit = kButtonBits[i];
    if ((m_buttons & bit) && !(m_buttonsLogged & bit))
    {
      m_buttonsLogged |= bit;
      InfoLog("Input: first press of %s.", kButtonNames[i]);
    }
  }
  for (int i = 0; i < 4; i++)
  {
    const uint32_t bit = kPovBits[i];
    if ((m_buttons & bit) && !(m_buttonsLogged & bit))
    {
      m_buttonsLogged |= bit;
      InfoLog("Input: first press of %s (JOY1_POV1).", kPovNames[i]);
    }
  }
  if (m_l2 > 128 && !m_triggersLogged[0])
  {
    m_triggersLogged[0] = true;
    InfoLog("Input: first press of L2 (JOY1_ZAXIS).");
  }
  if (m_r2 > 128 && !m_triggersLogged[1])
  {
    m_triggersLogged[1] = true;
    InfoLog("Input: first press of R2 (JOY1_RZAXIS).");
  }
  return true;
}

int CPS5InputSystem::GetNumKeyboards() const { return 0; }
int CPS5InputSystem::GetNumMice() const { return ANY_MOUSE; }
int CPS5InputSystem::GetNumJoysticks() const { return 1; }
const KeyDetails *CPS5InputSystem::GetKeyDetails(int) { return nullptr; }
const MouseDetails *CPS5InputSystem::GetMouseDetails(int) { return nullptr; }

const JoyDetails *CPS5InputSystem::GetJoyDetails(int joyNum)
{
  return joyNum == 0 ? &m_joyDetails : nullptr;
}

void CPS5InputSystem::SetMouseVisibility(bool) {}
int CPS5InputSystem::GetKeyIndex(const char *) { return -1; }
const char *CPS5InputSystem::GetKeyName(int) { return nullptr; }
bool CPS5InputSystem::IsKeyPressed(int, int) const { return false; }
int CPS5InputSystem::GetMouseAxisValue(int, int axisNum) const
{
  // Turned round with the left stick (L1): mirrored about the centre.
  const float cx = m_dispX + m_dispW * 0.5f, cy = m_dispY + m_dispH * 0.5f;
  switch (axisNum)
  {
  case AXIS_X: return (int)(s_leftStickInverted ? 2.0f * cx - m_mouseX : m_mouseX);
  case AXIS_Y: return (int)(s_leftStickInverted ? 2.0f * cy - m_mouseY : m_mouseY);
  default:     return 0;
  }
}

int CPS5InputSystem::GetMouseWheelDir(int) const
{
  return m_mouseWheel > 0 ? 1 : m_mouseWheel < 0 ? -1 : 0;
}

bool CPS5InputSystem::IsMouseButPressed(int, int butNum) const
{
  // Supermodel's numbering: left, middle, right, side 1, side 2.
  static const uint32_t bits[] = { PS5_MOUSE_LEFT, PS5_MOUSE_MIDDLE, PS5_MOUSE_RIGHT, PS5_MOUSE_SIDE1, PS5_MOUSE_SIDE2 };
  return butNum >= 0 && butNum < 5 && (m_mouseButtons & bits[butNum]) != 0;
}

int CPS5InputSystem::GetJoyAxisValue(int joyNum, int axisNum) const
{
  if (joyNum != 0)
    return 0;
  switch (axisNum)
  {
  case AXIS_X:  return Inverted(Stick(m_lx));
  case AXIS_Y:  return Inverted(Stick(m_ly));
  case AXIS_Z:  return Trigger(m_l2);
  case AXIS_RX: return Stick(m_rx);
  case AXIS_RY: return Stick(m_ry);
  case AXIS_RZ: return Trigger(m_r2);
  default:      return 0;
  }
}

bool CPS5InputSystem::IsJoyPOVInDir(int joyNum, int povNum, int povDir) const
{
  if (joyNum != 0 || povNum != 0)
    return false;
  switch (povDir)
  {
  case POV_UP:    return (m_buttons & PAD_UP) != 0;
  case POV_DOWN:  return (m_buttons & PAD_DOWN) != 0;
  case POV_LEFT:  return (m_buttons & PAD_LEFT) != 0;
  case POV_RIGHT: return (m_buttons & PAD_RIGHT) != 0;
  default:        return false;
  }
}

bool CPS5InputSystem::IsJoyButPressed(int joyNum, int butNum) const
{
  if (joyNum != 0 || butNum < 0 || butNum >= kNumButtons)
    return false;
  return (m_buttons & kButtonBits[butNum]) != 0;
}

/*
 * From the drive board's thread. The levels as the desktop's XInput pads get
 * them (DirectInputSystem.cpp, CDirectInputSystem::ProcessForceFeedbackCmd):
 * a constant force below the threshold stops the motors; at or above it, its
 * size times XInputConstForceMax, on both motors or on the side it pushes to
 * (left the large motor, right the small one); the vibration effect on both.
 */
bool CPS5InputSystem::ProcessForceFeedbackCmd(int, int, ForceFeedbackCmd ffCmd)
{
  auto level = [](float force, float max) { return (int)std::min(255.0f, std::max(0.0f, force * max * 255.0f)); };
  m_ffCommands++;
  const float size = ffCmd.force < 0.0f ? -ffCmd.force : ffCmd.force;
  const int percent = (int)(size * 100.0f + 0.5f);
  if (ffCmd.id != FFStop && percent > m_ffStrongestPercent)
    m_ffStrongestPercent = percent;  // for the log; a race with Poll() only costs a reading
  switch (ffCmd.id)
  {
  case FFStop:
    m_ffLeft = m_ffRight = m_ffVibrate = 0;
    return true;
  case FFConstantForce:
    if (size == 0.0f || size < m_ffThreshold)
      m_ffLeft = m_ffRight = 0;
    else if (m_ffStereo)
      m_ffLeft = m_ffRight = level(size, m_ffConstMax);
    else
    {
      m_ffLeft = ffCmd.force < 0.0f ? level(size, m_ffConstMax) : 0;
      m_ffRight = ffCmd.force < 0.0f ? 0 : level(size, m_ffConstMax);
    }
    return true;
  case FFVibrate:
    m_ffVibrate = level(size, m_ffVibrateMax);
    return true;
  default:  // self-centring, friction: no motor for them
    return false;
  }
}

// From Poll(): the levels to the motors, and every 10 s what they got.
void CPS5InputSystem::UpdateMotors()
{
  const int vibrate = m_ffVibrate;
  const int large = std::min(255, m_ffLeft + vibrate);
  const int small = std::min(255, m_ffRight + vibrate);
  PS5PadVibrate((uint8_t)large, (uint8_t)small);

  const double now = PS5AppSeconds();
  if (now - m_ffLoggedAt < 10.0)
    return;
  m_ffLoggedAt = now;
  const unsigned commands = m_ffCommands.exchange(0);
  PS5PadVibrationStats stats;
  const bool sent = PS5PadTakeVibrationStats(stats);
  if (!commands && !sent)
    return;
  const int strongest = m_ffStrongestPercent.exchange(0);
  if (stats.failures)
    InfoLog("Input: force feedback: %u commands (strongest %d%%), %u motor sends (strongest %d of 255), %u failed (last 0x%x).",
      commands, strongest, stats.sends, stats.strongest, stats.failures, (unsigned)stats.lastError);
  else
    InfoLog("Input: force feedback: %u commands (strongest %d%%), %u motor sends (strongest %d of 255).",
      commands, strongest, stats.sends, stats.strongest);
}
