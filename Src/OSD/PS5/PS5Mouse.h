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
 * PS5Mouse.h
 *
 * A USB or Bluetooth mouse, through libSceMouse, as XPSemu Chihiro reads it
 * (ui/xemu-input-ps5.c there): the library is loaded at run time
 * (sceKernelLoadStartModule; linked as an import the console leaves it
 * unbound) and its three functions looked up by name, then by NID. Opened
 * for the user who started the app, every mouse merged into one. Each step
 * is logged once.
 */

#ifndef INCLUDED_PS5MOUSE_H
#define INCLUDED_PS5MOUSE_H

#include <cstdint>

enum : uint32_t
{
  PS5_MOUSE_LEFT = 0x01,
  PS5_MOUSE_RIGHT = 0x02,
  PS5_MOUSE_MIDDLE = 0x04,
  PS5_MOUSE_SIDE1 = 0x08,
  PS5_MOUSE_SIDE2 = 0x10,
};

struct PS5MouseInput
{
  bool open = false;     // the library is there and a mouse handle open
  int dx = 0, dy = 0;    // movement since the last read, counts, down positive
  int wheel = 0;
  uint32_t buttons = 0;  // PS5_MOUSE_*, as of the last sample
};

// Everything since the last call. Loads and opens on the first call, and
// tries to open again every 120 calls (2 s) while that fails.
PS5MouseInput PS5MouseRead();

#endif  // INCLUDED_PS5MOUSE_H
