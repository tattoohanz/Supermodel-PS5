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
 * PS5Pad.cpp
 */

#include "PS5Pad.h"
#include "Supermodel.h"

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
    uint8_t lx, ly, rx, ry;
    uint8_t l2, r2;
    uint8_t pad[2];
    uint8_t rest[256];
  };

  constexpr unsigned kRetryReads = 120;

  bool s_initialized = false;
  int32_t s_user = -1;
  int32_t s_handle = -1;
  unsigned s_readsUntilRetry = 0;
  bool s_loggedFailure = false;
  PS5PadState s_last;

  void Open()
  {
    s_readsUntilRetry = kRetryReads;
    if (s_user < 0 && sceUserServiceGetInitialUser(&s_user) != 0)
      s_user = -1;
    if (s_user < 0)
      return;
    s_handle = scePadOpen(s_user, 0, 0, nullptr);
    if (s_handle < 0)
      s_handle = scePadGetHandle(s_user, 0, 0);
    if (s_handle >= 0)
      InfoLog("Input: DualSense of user %d opened (handle %d).", s_user, s_handle);
    else if (!s_loggedFailure)
    {
      s_loggedFailure = true;
      ErrorLog("Input: no DualSense for user %d yet (0x%x); trying again every 2 s.", s_user, (unsigned)s_handle);
    }
  }
}

PS5PadState PS5PadRead()
{
  if (!s_initialized)
  {
    s_initialized = true;
    const int rcUser = sceUserServiceInitialize(nullptr);
    const int rcPad = scePadInit();
    InfoLog("Input: user service 0x%x, pad library 0x%x.", (unsigned)rcUser, (unsigned)rcPad);
    Open();
  }
  else if (s_handle < 0)
  {
    if (s_readsUntilRetry == 0)
      Open();
    else
      s_readsUntilRetry--;
  }

  PS5PadState state;
  PadData d;
  memset(&d, 0, sizeof(d));
  if (s_handle >= 0 && scePadReadState(s_handle, &d) == 0)
  {
    state.valid = true;
    state.buttons = d.buttons;
    state.lx = d.lx;
    state.ly = d.ly;
    state.rx = d.rx;
    state.ry = d.ry;
    state.l2 = d.l2;
    state.r2 = d.r2;
  }
  s_last = state;
  return state;
}

const PS5PadState &PS5PadLast()
{
  return s_last;
}

int32_t PS5PadUser()
{
  return s_user;
}
