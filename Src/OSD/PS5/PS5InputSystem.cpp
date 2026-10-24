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
 */

#include "PS5InputSystem.h"
#include "Supermodel.h"
#include "Inputs/Input.h"

CPS5InputSystem::CPS5InputSystem() : CInputSystem("ps5")
{
}

CPS5InputSystem::~CPS5InputSystem()
{
}

bool CPS5InputSystem::InitializeSystem()
{
  return true;
}

int CPS5InputSystem::GetNumKeyboards() const { return 0; }
int CPS5InputSystem::GetNumMice() const { return 0; }
int CPS5InputSystem::GetNumJoysticks() const { return 0; }
const KeyDetails *CPS5InputSystem::GetKeyDetails(int) { return nullptr; }
const MouseDetails *CPS5InputSystem::GetMouseDetails(int) { return nullptr; }
const JoyDetails *CPS5InputSystem::GetJoyDetails(int) { return nullptr; }
bool CPS5InputSystem::Poll() { return true; }
void CPS5InputSystem::SetMouseVisibility(bool) {}
int CPS5InputSystem::GetKeyIndex(const char *) { return -1; }
const char *CPS5InputSystem::GetKeyName(int) { return nullptr; }
bool CPS5InputSystem::IsKeyPressed(int, int) const { return false; }
int CPS5InputSystem::GetMouseAxisValue(int, int) const { return 0; }
int CPS5InputSystem::GetMouseWheelDir(int) const { return 0; }
bool CPS5InputSystem::IsMouseButPressed(int, int) const { return false; }
int CPS5InputSystem::GetJoyAxisValue(int, int) const { return 0; }
bool CPS5InputSystem::IsJoyPOVInDir(int, int, int) const { return false; }
bool CPS5InputSystem::IsJoyButPressed(int, int) const { return false; }
bool CPS5InputSystem::ProcessForceFeedbackCmd(int, int, ForceFeedbackCmd) { return false; }
