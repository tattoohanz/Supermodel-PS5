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
 * SDLShim.h
 *
 * The PS5 build has no SDL. The emulator core and the Vulkan backend name a
 * handful of SDL entry points (a message box on unknown hardware accesses, the
 * Vulkan surface, the window size) and the network board's socket types; this
 * header declares just those, implemented in Display.cpp and NetStubs.cpp.
 * SDL.h, SDL_vulkan.h and SDLIncludes.h in this directory include it.
 */

#ifndef INCLUDED_PS5_SDLSHIM_H
#define INCLUDED_PS5_SDLSHIM_H

#include <cstdint>

typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

// The one "window": the VideoOut display (Display.cpp).
struct SDL_Window;

#define SDL_MESSAGEBOX_ERROR       0x00000010
#define SDL_MESSAGEBOX_WARNING     0x00000020
#define SDL_MESSAGEBOX_INFORMATION 0x00000040

// Logged rather than shown.
int SDL_ShowSimpleMessageBox(uint32_t flags, const char *title, const char *message, SDL_Window *window);
const char *SDL_GetError(void);
void SDL_GetWindowSize(SDL_Window *window, int *w, int *h);

// Vulkan: VK_KHR_display on VideoOut. The handles are declared the way
// vulkan_core.h declares them on a 64-bit target.
struct VkInstance_T;
struct VkSurfaceKHR_T;
SDL_bool SDL_Vulkan_GetInstanceExtensions(SDL_Window *window, unsigned *count, const char **names);
SDL_bool SDL_Vulkan_CreateSurface(SDL_Window *window, VkInstance_T *instance, VkSurfaceKHR_T **surface);

// SDL_net's types, for the network board's members (NetLoopback.cpp: the
// link runs inside the process).
typedef struct _TCPsocket *TCPsocket;
typedef struct _SDLNet_SocketSet *SDLNet_SocketSet;
typedef struct
{
  uint32_t host;
  uint16_t port;
} IPaddress;

#endif  // INCLUDED_PS5_SDLSHIM_H
