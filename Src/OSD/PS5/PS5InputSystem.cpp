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
#include "Supermodel.h"
#include "Inputs/Input.h"

#include <cstring>

extern "C" {
int sceUserServiceInitialize(const void *params);
int sceUserServiceGetInitialUser(int32_t *user_id);
int scePadInit(void);
int scePadOpen(int32_t user_id, int32_t type, int32_t index, const void *param);
int scePadGetHandle(int32_t user_id, int32_t type, int32_t index);
int scePadReadState(int32_t handle, void *data);
}

namespace
{
  // The start of ScePadData; the rest (motion, touch, ...) is not used.
  struct PadData
  {
    uint32_t buttons;
    uint8_t lx, ly, rx, ry;  // 0-255, 128 centred, down and right larger
    uint8_t l2, r2;          // 0-255
    uint8_t pad[2];
    uint8_t rest[256];
  };

  enum : uint32_t
  {
    PAD_CREATE = 0x00000001,
    PAD_L3 = 0x00000002,
    PAD_R3 = 0x00000004,
    PAD_OPTIONS = 0x00000008,
    PAD_UP = 0x00000010,
    PAD_RIGHT = 0x00000020,
    PAD_DOWN = 0x00000040,
    PAD_LEFT = 0x00000080,
    PAD_L1 = 0x00000400,
    PAD_R1 = 0x00000800,
    PAD_TRIANGLE = 0x00001000,
    PAD_CIRCLE = 0x00002000,
    PAD_CROSS = 0x00004000,
    PAD_SQUARE = 0x00008000,
    PAD_TOUCHPAD = 0x00100000,
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

  int Trigger(uint8_t v)
  {
    return (int)v * 32767 / 255;
  }

  // Every 2 s at 60 polls a second, while the pad is not open.
  constexpr unsigned kRetryPolls = 120;
}

CPS5InputSystem::CPS5InputSystem() : CInputSystem("ps5")
{
  memset(&m_joyDetails, 0, sizeof(m_joyDetails));
  strncpy(m_joyDetails.name, "DualSense", MAX_NAME_LENGTH);
  m_joyDetails.numAxes = 6;
  m_joyDetails.numPOVs = 1;
  m_joyDetails.numButtons = kNumButtons;
  m_joyDetails.hasFFeedback = false;
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
}

bool CPS5InputSystem::InitializeSystem()
{
  const int rcUser = sceUserServiceInitialize(nullptr);
  const int rcPad = scePadInit();
  InfoLog("Input: user service 0x%x, pad library 0x%x.", (unsigned)rcUser, (unsigned)rcPad);
  OpenPad();
  return true;
}

void CPS5InputSystem::OpenPad()
{
  if (m_user < 0 && sceUserServiceGetInitialUser(&m_user) != 0)
    m_user = -1;
  if (m_user < 0)
  {
    m_pollsUntilRetry = kRetryPolls;
    return;
  }
  m_handle = scePadOpen(m_user, 0, 0, nullptr);
  if (m_handle < 0)
    m_handle = scePadGetHandle(m_user, 0, 0);
  static bool s_loggedFailure = false;
  if (m_handle >= 0)
    InfoLog("Input: DualSense of user %d opened as JOY1 (handle %d).", m_user, m_handle);
  else if (!s_loggedFailure)
  {
    s_loggedFailure = true;
    ErrorLog("Input: no DualSense for user %d yet (0x%x); trying again every 2 s.", m_user, (unsigned)m_handle);
  }
  m_pollsUntilRetry = kRetryPolls;
}

bool CPS5InputSystem::Poll()
{
  if (m_handle < 0)
  {
    if (m_pollsUntilRetry == 0)
      OpenPad();
    else
      m_pollsUntilRetry--;
  }
  PadData d;
  memset(&d, 0, sizeof(d));
  m_stateValid = m_handle >= 0 && scePadReadState(m_handle, &d) == 0;
  if (!m_stateValid)
  {
    m_buttons = 0;
    m_lx = m_ly = m_rx = m_ry = 128;
    m_l2 = m_r2 = 0;
    return true;
  }
  m_buttons = d.buttons;
  m_lx = d.lx;
  m_ly = d.ly;
  m_rx = d.rx;
  m_ry = d.ry;
  m_l2 = d.l2;
  m_r2 = d.r2;

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
int CPS5InputSystem::GetNumMice() const { return 0; }
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
int CPS5InputSystem::GetMouseAxisValue(int, int) const { return 0; }
int CPS5InputSystem::GetMouseWheelDir(int) const { return 0; }
bool CPS5InputSystem::IsMouseButPressed(int, int) const { return false; }

int CPS5InputSystem::GetJoyAxisValue(int joyNum, int axisNum) const
{
  if (joyNum != 0)
    return 0;
  switch (axisNum)
  {
  case AXIS_X:  return Stick(m_lx);
  case AXIS_Y:  return Stick(m_ly);
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

bool CPS5InputSystem::ProcessForceFeedbackCmd(int, int, ForceFeedbackCmd)
{
  return false;
}
