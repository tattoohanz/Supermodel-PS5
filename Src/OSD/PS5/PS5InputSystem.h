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
 * PS5 input. First step: no devices, so the game runs its attract mode. The
 * DualSense (libScePad) comes next.
 */

#ifndef INCLUDED_PS5INPUTSYSTEM_H
#define INCLUDED_PS5INPUTSYSTEM_H

#include "Inputs/InputSystem.h"

class CPS5InputSystem : public CInputSystem
{
public:
  CPS5InputSystem();
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
};

#endif  // INCLUDED_PS5INPUTSYSTEM_H
