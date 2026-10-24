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
 * Display.h
 *
 * The PS5's one output: VideoOut through RADV's VK_KHR_display. The Vulkan
 * backend reaches it through the SDL names in SDLShim.h.
 */

#ifndef INCLUDED_PS5_DISPLAY_H
#define INCLUDED_PS5_DISPLAY_H

#include "SDLShim.h"
#include <cstdint>

// The window handle to give CVulkanContext::InitDevice.
SDL_Window *PS5DisplayWindow();

// The display mode's refresh rate in millihertz, 0 before the surface exists.
uint32_t PS5DisplayRefreshMilliHz();

// Loads Vulkan from the statically linked RADV (there is no loader to open).
bool PS5VulkanInitialize();

#endif  // INCLUDED_PS5_DISPLAY_H
