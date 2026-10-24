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
 * Display.cpp
 *
 * VideoOut through VK_KHR_display, after XPSemu Chihiro's ui/xui/vk-present.cc
 * (CreateDisplaySurface): RADV reports one display with a single 3840x2160
 * mode and refuses a surface of any other extent (as the PS5SX2 port found).
 */

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include "volk/volk.h"

#include "Display.h"
#include "OSD/Logger.h"

#include <cstdio>
#include <vector>

// RADV, linked statically, is the ICD; this is its entry point.
extern "C" PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance instance, const char *name);

struct SDL_Window
{
  int width = 0;
  int height = 0;
  uint32_t refreshMilliHz = 0;
};

static SDL_Window s_window;
static char s_error[256] = "";

SDL_Window *PS5DisplayWindow()
{
  return &s_window;
}

uint32_t PS5DisplayRefreshMilliHz()
{
  return s_window.refreshMilliHz;
}

bool PS5VulkanInitialize()
{
  volkInitializeCustom((PFN_vkGetInstanceProcAddr)vk_icdGetInstanceProcAddr);
  return vkCreateInstance != nullptr;
}

int SDL_ShowSimpleMessageBox(uint32_t, const char *title, const char *message, SDL_Window *)
{
  ErrorLog("%s: %s", title ? title : "Message", message ? message : "");
  return 0;
}

const char *SDL_GetError(void)
{
  return s_error;
}

void SDL_GetWindowSize(SDL_Window *window, int *w, int *h)
{
  if (w)
    *w = window ? window->width : 0;
  if (h)
    *h = window ? window->height : 0;
}

SDL_bool SDL_Vulkan_GetInstanceExtensions(SDL_Window *, unsigned *count, const char **names)
{
  static const char *const extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_DISPLAY_EXTENSION_NAME };
  const unsigned n = sizeof(extensions) / sizeof(extensions[0]);
  if (names)
  {
    if (*count < n)
      return SDL_FALSE;
    for (unsigned i = 0; i < n; i++)
      names[i] = extensions[i];
  }
  *count = n;
  return SDL_TRUE;
}

SDL_bool SDL_Vulkan_CreateSurface(SDL_Window *window, VkInstance_T *instance, VkSurfaceKHR_T **surfaceOut)
{
  *surfaceOut = VK_NULL_HANDLE;

  // One GPU: the display belongs to the first physical device.
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
  if (deviceCount == 0)
  {
    snprintf(s_error, sizeof(s_error), "no Vulkan physical device");
    return SDL_FALSE;
  }
  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
  VkPhysicalDevice pd = devices[0];

  if (!vkGetPhysicalDeviceDisplayPropertiesKHR || !vkCreateDisplayPlaneSurfaceKHR)
  {
    snprintf(s_error, sizeof(s_error), "VK_KHR_display entry points not loaded");
    return SDL_FALSE;
  }

  uint32_t displayCount = 0;
  vkGetPhysicalDeviceDisplayPropertiesKHR(pd, &displayCount, nullptr);
  if (displayCount == 0)
  {
    snprintf(s_error, sizeof(s_error), "the device reports no display");
    return SDL_FALSE;
  }
  std::vector<VkDisplayPropertiesKHR> displays(displayCount);
  vkGetPhysicalDeviceDisplayPropertiesKHR(pd, &displayCount, displays.data());

  uint32_t planeCount = 0;
  vkGetPhysicalDeviceDisplayPlanePropertiesKHR(pd, &planeCount, nullptr);
  if (planeCount == 0)
  {
    snprintf(s_error, sizeof(s_error), "the device reports no display plane");
    return SDL_FALSE;
  }
  std::vector<VkDisplayPlanePropertiesKHR> planes(planeCount);
  vkGetPhysicalDeviceDisplayPlanePropertiesKHR(pd, &planeCount, planes.data());

  // The first display with a mode, and the largest mode it offers.
  VkDisplayKHR display = VK_NULL_HANDLE;
  VkDisplayModePropertiesKHR mode = {};
  for (const VkDisplayPropertiesKHR &d : displays)
  {
    uint32_t modeCount = 0;
    vkGetDisplayModePropertiesKHR(pd, d.display, &modeCount, nullptr);
    std::vector<VkDisplayModePropertiesKHR> modes(modeCount);
    vkGetDisplayModePropertiesKHR(pd, d.display, &modeCount, modes.data());
    for (const VkDisplayModePropertiesKHR &m : modes)
    {
      const VkExtent2D &r = m.parameters.visibleRegion;
      const VkExtent2D &best = mode.parameters.visibleRegion;
      InfoLog("PS5 display: mode %ux%u @ %.3f Hz", r.width, r.height, m.parameters.refreshRate / 1000.0);
      if (display == VK_NULL_HANDLE || r.width * r.height > best.width * best.height)
      {
        display = d.display;
        mode = m;
      }
    }
  }
  if (display == VK_NULL_HANDLE)
  {
    snprintf(s_error, sizeof(s_error), "no display offers a mode");
    return SDL_FALSE;
  }

  // The plane that can drive that display.
  uint32_t plane = UINT32_MAX;
  for (uint32_t i = 0; i < planeCount; i++)
  {
    if (planes[i].currentDisplay == VK_NULL_HANDLE || planes[i].currentDisplay == display)
    {
      plane = i;
      break;
    }
  }
  if (plane == UINT32_MAX)
  {
    snprintf(s_error, sizeof(s_error), "no display plane can drive the display");
    return SDL_FALSE;
  }

  VkDisplaySurfaceCreateInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
  info.displayMode = mode.displayMode;
  info.planeIndex = plane;
  info.planeStackIndex = planes[plane].currentStackIndex;
  info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  info.globalAlpha = 1.0f;
  info.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
  info.imageExtent = mode.parameters.visibleRegion;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkResult res = vkCreateDisplayPlaneSurfaceKHR(instance, &info, nullptr, &surface);
  if (res != VK_SUCCESS)
  {
    snprintf(s_error, sizeof(s_error), "vkCreateDisplayPlaneSurfaceKHR: %d", (int)res);
    return SDL_FALSE;
  }

  if (window)
  {
    window->width = (int)info.imageExtent.width;
    window->height = (int)info.imageExtent.height;
    window->refreshMilliHz = mode.parameters.refreshRate;
  }
  InfoLog("PS5 display: surface %ux%u @ %.3f Hz on plane %u", info.imageExtent.width,
    info.imageExtent.height, mode.parameters.refreshRate / 1000.0, plane);
  *surfaceOut = surface;
  return SDL_TRUE;
}
