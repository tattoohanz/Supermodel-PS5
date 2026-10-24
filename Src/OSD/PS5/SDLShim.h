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
 * Vulkan surface, the window size), the desktop audio code (OSD/SDL/Audio.cpp,
 * used as it is) SDL's audio device, and the network board SDL_net's socket
 * types; this header declares just those, implemented in Display.cpp,
 * AudioOut.cpp and NetLoopback.cpp.
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

// SDL 2's audio device API, as OSD/SDL/Audio.cpp uses it (AudioOut.cpp:
// sceAudioOut at 48 kHz, resampled).
typedef uint8_t Uint8;
typedef uint16_t SDL_AudioFormat;
typedef uint32_t SDL_AudioDeviceID;
typedef void (*SDL_AudioCallback)(void *userdata, Uint8 *stream, int len);
struct SDL_AudioSpec
{
  int freq;
  SDL_AudioFormat format;
  uint8_t channels;
  uint8_t silence;
  uint16_t samples;
  uint16_t padding;
  uint32_t size;
  SDL_AudioCallback callback;
  void *userdata;
};
#define AUDIO_S16SYS 0x8010
#define SDL_INIT_AUDIO 0x00000010u
#define SDL_AUDIO_ALLOW_ANY_CHANGE 0x0000000F
int SDL_InitSubSystem(uint32_t flags);
int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
SDL_AudioDeviceID SDL_OpenAudioDevice(const char *device, int iscapture, const SDL_AudioSpec *desired,
  SDL_AudioSpec *obtained, int allowed_changes);
void SDL_PauseAudio(int pause_on);
void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on);
void SDL_LockAudio(void);
void SDL_UnlockAudio(void);
void SDL_LockAudioDevice(SDL_AudioDeviceID dev);
void SDL_UnlockAudioDevice(SDL_AudioDeviceID dev);
void SDL_CloseAudio(void);
void SDL_CloseAudioDevice(SDL_AudioDeviceID dev);

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
