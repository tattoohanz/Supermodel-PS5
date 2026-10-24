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
 * PS5InputSystem.h
 *
 * PS5 input: the DualSense of the user who started the app, read with
 * libScePad, as joystick 1 (JOY1_...) in the numbering of SDL's game
 * controller API, so the desktop build's mappings work unchanged:
 *
 *   JOY1_BUTTON1 Cross     JOY1_BUTTON2 Circle   JOY1_BUTTON3 Square
 *   JOY1_BUTTON4 Triangle  JOY1_BUTTON5 L1       JOY1_BUTTON6 R1
 *   JOY1_BUTTON7 Create    JOY1_BUTTON8 Options  JOY1_BUTTON9 L3
 *   JOY1_BUTTON10 R3       JOY1_BUTTON11 touchpad click
 *   JOY1_POV1_* the D-pad
 *   JOY1_XAXIS/YAXIS left stick, RXAXIS/RYAXIS right stick (down and right
 *   positive), ZAXIS L2 and RZAXIS R2 (0 released, 32767 fully pressed).
 *
 * A mouse (PS5Mouse.h), any one, as MOUSE_...: its movement moves a pointer
 * kept on the game's picture, 1000 counts across it, whose position is
 * MOUSE_XAXIS/YAXIS (the centre being the middle of an axis, as on the
 * desktop); buttons left, middle, right, side 1 and 2. No keyboard.
 *
 * Force feedback (a drive board's, with ForceFeedback on and its ROM in the
 * zip) on the DualSense's two motors, as the desktop gives an XInput pad
 * (OSD/Windows/DirectInputSystem.cpp), with its settings: a constant force
 * from XInputConstForceThreshold (30%) up, XInputConstForceMax (100%) at
 * full force, on both motors (XInputStereoVibration, on) or on the side it
 * pushes to; the vibration effect on both, XInputVibrateMax (100%) at full.
 * Self-centring and friction have no motor to go to, as on the desktop.
 */

#ifndef INCLUDED_PS5INPUTSYSTEM_H
#define INCLUDED_PS5INPUTSYSTEM_H

#include "Inputs/InputSystem.h"
#include "Util/NewConfig.h"

#include <atomic>
#include <cstdint>

class CPS5InputSystem : public CInputSystem
{
public:
  // The left stick turned round, both ways (L1 in Star Wars Trilogy's duels,
  // Main.cpp). Every session starts with it off.
  static void SetLeftStickInverted(bool inverted);

  // The force feedback settings are read here (the session's configuration).
  explicit CPS5InputSystem(const Util::Config::Node &config);
  ~CPS5InputSystem() override;

  int GetNumKeyboards() const override;
  int GetNumMice() const override;
  int GetNumJoysticks() const override;
  const KeyDetails *GetKeyDetails(int kbdNum) override;
  const MouseDetails *GetMouseDetails(int mseNum) override;
  const JoyDetails *GetJoyDetails(int joyNum) override;
  bool Poll() override;
  void SetMouseVisibility(bool visible) override;

protected:
  bool InitializeSystem() override;
  int GetKeyIndex(const char *keyName) override;
  const char *GetKeyName(int keyIndex) override;
  bool IsKeyPressed(int kbdNum, int keyIndex) const override;
  int GetMouseAxisValue(int mseNum, int axisNum) const override;
  int GetMouseWheelDir(int mseNum) const override;
  bool IsMouseButPressed(int mseNum, int butNum) const override;
  int GetJoyAxisValue(int joyNum, int axisNum) const override;
  bool IsJoyPOVInDir(int joyNum, int povNum, int povDir) const override;
  bool IsJoyButPressed(int joyNum, int butNum) const override;
  bool ProcessForceFeedbackCmd(int joyNum, int axisNum, ForceFeedbackCmd ffCmd) override;

private:
  JoyDetails m_joyDetails;
  bool m_stateValid = false;
  bool m_mouseOpen = false;
  bool m_mouseCentred = false;
  float m_mouseX = 0.0f, m_mouseY = 0.0f;  // display pixels
  int m_mouseWheel = 0;
  uint32_t m_mouseButtons = 0;
  uint32_t m_buttons = 0;     // libScePad's button bits
  uint8_t m_lx = 128, m_ly = 128, m_rx = 128, m_ry = 128, m_l2 = 0, m_r2 = 0;
  uint32_t m_buttonsLogged = 0;
  bool m_triggersLogged[2] = { false, false };

  // Force feedback: the drive board's thread sets the levels
  // (ProcessForceFeedbackCmd), Poll() sends them to the motors.
  void UpdateMotors();
  float m_ffThreshold = 0.3f, m_ffConstMax = 1.0f, m_ffVibrateMax = 1.0f;
  bool m_ffStereo = true;
  std::atomic<int> m_ffLeft{ 0 }, m_ffRight{ 0 }, m_ffVibrate{ 0 };  // 0-255 each
  std::atomic<unsigned> m_ffCommands{ 0 };
  std::atomic<int> m_ffStrongestPercent{ 0 };   // since the last log line
  double m_ffLoggedAt = 0.0;
};

#endif  // INCLUDED_PS5INPUTSYSTEM_H
