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
 * PS5Pad.h
 *
 * The DualSense of the user who started the app, read with libScePad. One
 * handle for the whole run, shared by the game's input system
 * (PS5InputSystem.cpp) and the menu (Menu.cpp): opened on the first read,
 * retried every 120 reads (2 s at 60 a second) while there is no pad.
 */

#ifndef INCLUDED_PS5PAD_H
#define INCLUDED_PS5PAD_H

#include <cstdint>

// libScePad's button bits.
enum : uint32_t
{
  PS5_PAD_CREATE = 0x00000001,
  PS5_PAD_L3 = 0x00000002,
  PS5_PAD_R3 = 0x00000004,
  PS5_PAD_OPTIONS = 0x00000008,
  PS5_PAD_UP = 0x00000010,
  PS5_PAD_RIGHT = 0x00000020,
  PS5_PAD_DOWN = 0x00000040,
  PS5_PAD_LEFT = 0x00000080,
  PS5_PAD_L2 = 0x00000100,
  PS5_PAD_R2 = 0x00000200,
  PS5_PAD_L1 = 0x00000400,
  PS5_PAD_R1 = 0x00000800,
  PS5_PAD_TRIANGLE = 0x00001000,
  PS5_PAD_CIRCLE = 0x00002000,
  PS5_PAD_CROSS = 0x00004000,
  PS5_PAD_SQUARE = 0x00008000,
  PS5_PAD_TOUCHPAD = 0x00100000,
};

struct PS5PadState
{
  bool valid = false;                 // false while there is no pad: all released
  uint32_t buttons = 0;               // PS5_PAD_* bits
  uint8_t lx = 128, ly = 128;         // left stick, 0-255, 128 centred, down and right larger
  uint8_t rx = 128, ry = 128;         // right stick
  uint8_t l2 = 0, r2 = 0;             // triggers, 0-255
};

// Reads the pad now (opening it first if need be).
PS5PadState PS5PadRead();

// The state the last PS5PadRead() returned.
const PS5PadState &PS5PadLast();

// The user who started the app (-1 until the first read finds one).
int32_t PS5PadUser();

// The DualSense's two motors (0-255 each; 0, 0 stops them), for the drive
// boards' force feedback (PS5InputSystem.cpp). Sent when the levels change
// and again every second while they are on; a no-op without a pad. In the
// controller's compatible vibration mode, set once per handle, as XPSemu
// Chihiro does after PS5_RetroArch.
void PS5PadVibrate(uint8_t large, uint8_t small);

// What the motors were sent since the last call, for the log: false when
// nothing was.
struct PS5PadVibrationStats
{
  unsigned sends = 0, failures = 0;
  int lastError = 0;
  uint8_t strongest = 0;
};
bool PS5PadTakeVibrationStats(PS5PadVibrationStats &stats);

#endif  // INCLUDED_PS5PAD_H
