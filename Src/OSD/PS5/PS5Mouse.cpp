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
 * PS5Mouse.cpp
 *
 * The prototypes, the layout of a sample and the NIDs are the PS4 SDK's, as
 * shadPS4 has them (src/core/libraries/mouse), and as XPSemu Chihiro uses
 * them on the PS5.
 */

#include "PS5Mouse.h"
#include "PS5Pad.h"
#include "Supermodel.h"

#include <cstddef>
#include <cstring>

extern "C" {
int sceKernelLoadStartModule(const char *path, size_t args, const void *argp, uint32_t flags, const void *option,
  int *result);
int sceKernelDlsym(int handle, const char *symbol, void **address);
}

namespace
{
  struct MouseOpenParam
  {
    uint8_t behavior;  // 1: every mouse merged into one
    uint8_t reserved[7];
  };

  struct MouseData
  {
    uint64_t timestamp;
    bool connected;
    uint32_t buttons;  // left 1, right 2, middle 4, side 8 and 16
    int32_t x, y;      // movement since the last sample, down positive
    int32_t wheel, tilt;
    uint8_t reserved[8];
  };
  static_assert(sizeof(MouseData) == 40, "libSceMouse's sample is 40 bytes");

  int (*s_init)(void) = nullptr;
  int (*s_open)(int32_t user, int32_t type, int32_t index, const MouseOpenParam *param) = nullptr;
  int (*s_read)(int32_t handle, MouseData *data, int32_t num) = nullptr;

  bool s_tried = false, s_loaded = false, s_inited = false, s_connected = false;
  int s_module = -1;
  int32_t s_handle = -1;
  int s_lastOpenRc = 1;
  unsigned s_callsUntilRetry = 0;
  uint32_t s_buttons = 0;
  bool s_firstMove = false;

  void *Symbol(const char *name, const char *nid)
  {
    void *address = nullptr;
    const int byName = sceKernelDlsym(s_module, name, &address);
    int byNid = 0;
    if (!address)
      byNid = sceKernelDlsym(s_module, nid, &address);
    InfoLog("Mouse: %s %s (by name 0x%x, by NID %s 0x%x).", name, address ? "found" : "not found",
      (unsigned)byName, nid, (unsigned)byNid);
    return address;
  }

  bool Load()
  {
    if (s_tried)
      return s_loaded;
    s_tried = true;
    static const char *const paths[] = { "libSceMouse.sprx", "/system/common/lib/libSceMouse.sprx" };
    for (const char *path : paths)
    {
      int status = 0;
      const int h = sceKernelLoadStartModule(path, 0, nullptr, 0, nullptr, &status);
      InfoLog("Mouse: loading %s: 0x%x (status 0x%x).", path, (unsigned)h, (unsigned)status);
      if (h >= 0)
      {
        s_module = h;
        break;
      }
    }
    if (s_module < 0)
      return false;
    s_init = (int (*)(void))Symbol("sceMouseInit", "Qs0wWulgl7U");
    s_open = (int (*)(int32_t, int32_t, int32_t, const MouseOpenParam *))Symbol("sceMouseOpen", "RaqxZIf6DvE");
    s_read = (int (*)(int32_t, MouseData *, int32_t))Symbol("sceMouseRead", "x8qnXqh-tiM");
    s_loaded = s_init && s_open && s_read;
    return s_loaded;
  }

  void Open()
  {
    s_callsUntilRetry = 120;
    const int32_t user = PS5PadUser();
    if (user < 0 || !Load())
      return;
    if (!s_inited)
    {
      s_inited = true;
      InfoLog("Mouse: sceMouseInit 0x%x.", (unsigned)s_init());
    }
    MouseOpenParam merged = {};
    merged.behavior = 1;
    int rc = s_open(user, 0, 0, &merged);
    if (rc < 0)
      rc = s_open(user, 0, 0, nullptr);
    if (rc >= 0)
    {
      s_handle = rc;
      InfoLog("Mouse: open for user %d (handle %d); a mouse moves the aim (guns, analog joysticks).", user, rc);
    }
    else if (rc != s_lastOpenRc)
    {
      s_lastOpenRc = rc;
      InfoLog("Mouse: open for user %d: 0x%x; trying again every 2 s.", user, (unsigned)rc);
    }
  }
}

PS5MouseInput PS5MouseRead()
{
  PS5MouseInput in;
  if (s_handle < 0)
  {
    if (!s_tried || (s_loaded && s_callsUntilRetry-- == 0))
      Open();
    if (s_handle < 0)
      return in;
  }
  in.open = true;
  MouseData d[32];
  const int n = s_read(s_handle, d, 32);
  if (n < 0)
    return in;
  for (int i = 0; i < n; i++)
  {
    in.dx += d[i].x;
    in.dy += d[i].y;
    in.wheel += d[i].wheel;
    s_buttons = d[i].buttons;
    if (d[i].connected != s_connected)
    {
      s_connected = d[i].connected;
      InfoLog("Mouse: %s.", s_connected ? "connected" : "disconnected");
    }
  }
  if (!s_firstMove && (in.dx || in.dy || s_buttons))
  {
    s_firstMove = true;
    InfoLog("Mouse: first movement or click (%d,%d, buttons 0x%x).", in.dx, in.dy, s_buttons);
  }
  in.buttons = s_buttons;
  return in;
}
