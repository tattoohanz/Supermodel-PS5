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
#include "PS5App.h"
#include "Supermodel.h"

#include <algorithm>
#include <cstring>
#include <ctime>

namespace
{
  struct PadVibration
  {
    uint8_t largeMotor, smallMotor;
  };
}

extern "C" {
int scePadSetVibration(int32_t handle, const PadVibration *param);
int scePadSetVibrationMode(int32_t handle, int32_t mode);
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

  // The motors (PS5PadVibrate).
  int32_t s_vibrationHandle = -1;     // the handle the vibration mode was set on
  PadVibration s_vibration = { 0, 0 };
  int64_t s_vibrationSentMs = 0;
  bool s_vibrationLoggedFirst = false, s_vibrationLoggedFailure = false;
  PS5PadVibrationStats s_vibrationStats;

  int64_t NowMs()
  {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
  }

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

void PS5PadVibrate(uint8_t large, uint8_t small)
{
  static const bool bound = PS5ImportResolved("scePadSetVibration") && PS5ImportResolved("scePadSetVibrationMode");
  static bool loggedUnbound = false;
  if (!bound)
  {
    if (!loggedUnbound && (large || small))
    {
      loggedUnbound = true;
      ErrorLog("Input: no force feedback - scePadSetVibration or scePadSetVibrationMode is not bound for this title.");
    }
    return;
  }
  if (s_handle < 0)
    return;
  if (s_vibrationHandle != s_handle)
  {
    const int rc = scePadSetVibrationMode(s_handle, 2);
    InfoLog("Input: DualSense vibration mode 2 (compatible) on handle %d: 0x%x.", s_handle, (unsigned)rc);
    s_vibrationHandle = s_handle;
    s_vibration = { 0, 0 };
    s_vibrationSentMs = 0;
  }
  const int64_t now = NowMs();
  const bool changed = large != s_vibration.largeMotor || small != s_vibration.smallMotor;
  const bool refresh = (large || small) && now - s_vibrationSentMs >= 1000;
  if (!changed && !refresh)
    return;
  const PadVibration v = { large, small };
  const int rc = scePadSetVibration(s_handle, &v);
  s_vibration = v;
  s_vibrationSentMs = now;
  s_vibrationStats.sends++;
  s_vibrationStats.strongest = std::max(s_vibrationStats.strongest, std::max(large, small));
  if (rc != 0)
  {
    s_vibrationStats.failures++;
    s_vibrationStats.lastError = rc;
    if (!s_vibrationLoggedFailure)
    {
      s_vibrationLoggedFailure = true;
      ErrorLog("Input: DualSense motors %u, %u: 0x%x.", large, small, (unsigned)rc);
    }
  }
  else if ((large || small) && !s_vibrationLoggedFirst)
  {
    s_vibrationLoggedFirst = true;
    InfoLog("Input: DualSense motors first on (%u, %u of 255).", large, small);
  }
}

bool PS5PadTakeVibrationStats(PS5PadVibrationStats &stats)
{
  stats = s_vibrationStats;
  s_vibrationStats = PS5PadVibrationStats();
  return stats.sends > 0;
}
