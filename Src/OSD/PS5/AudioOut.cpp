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
 * AudioOut.cpp
 *
 * SDL 2's audio device on sceAudioOut, for OSD/SDL/Audio.cpp as it is: the
 * same ring buffer, mixing and balance as the desktop build, and the same
 * callback, which also wakes the asynchronous sound board thread.
 *
 * The console's main port plays 48 kHz signed 16-bit stereo, 256 frames at a
 * time; sceAudioOutOutput blocks until the previous grain has played, which
 * paces the thread (as in XPSemu Chihiro, which has the calls from PS5SX2).
 * Model 3 sound is 44.1 kHz: the callback's frames are folded to stereo and
 * resampled linearly.
 */

#include "SDLShim.h"
#include "OSD/Logger.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

extern "C" {
int sceAudioOutInit(void);
int sceAudioOutOpen(int user_id, int type, int index, unsigned int len, unsigned int freq, unsigned int param);
int sceAudioOutOutput(int handle, const void *buf);
int sceAudioOutClose(int handle);
}

void PS5SDLSetError(const char *message);

namespace
{
  constexpr int kUserSystem = 255;
  constexpr int kPortMain = 0;
  constexpr unsigned kS16Stereo = 1;
  constexpr unsigned kGrain = 256;     // frames per sceAudioOutOutput
  constexpr unsigned kOutRate = 48000;

  // SDL_LockAudio: held around the callback, as SDL holds it.
  std::recursive_mutex s_lock;
  SDL_AudioSpec s_spec;
  bool s_open = false;
  bool s_initialized = false;
  int s_handle = -1;
  std::atomic<bool> s_paused{ true };
  std::atomic<bool> s_running{ false };
  std::thread s_thread;

  // The callback's frames as stereo, and the resampler's position in them.
  std::vector<int16_t> s_src;
  double s_pos = 0.0;
  std::vector<uint8_t> s_pull;

  void Pull()
  {
    const unsigned frames = s_spec.samples ? s_spec.samples : 512;
    const unsigned channels = s_spec.channels;
    s_pull.resize((size_t)frames * channels * sizeof(int16_t));
    {
      std::lock_guard<std::recursive_mutex> lock(s_lock);
      s_spec.callback(s_spec.userdata, s_pull.data(), (int)s_pull.size());
    }
    const int16_t *in = (const int16_t *)s_pull.data();
    for (unsigned i = 0; i < frames; i++)
    {
      int left, right;
      if (channels == 1)
        left = right = in[i];
      else if (channels == 2)
      {
        left = in[2 * i];
        right = in[2 * i + 1];
      }
      else
      {
        // Front left, front right, rear left, rear right.
        left = (in[4 * i] + in[4 * i + 2]) / 2;
        right = (in[4 * i + 1] + in[4 * i + 3]) / 2;
      }
      s_src.push_back((int16_t)left);
      s_src.push_back((int16_t)right);
    }
  }

  void OutputThread()
  {
    const double step = (double)s_spec.freq / (double)kOutRate;
    int16_t out[kGrain * 2];
    unsigned long long grains = 0;
    while (s_running)
    {
      if (s_paused)
        memset(out, 0, sizeof(out));
      else
      {
        for (unsigned i = 0; i < kGrain; i++)
        {
          size_t index = (size_t)s_pos;
          while (index + 1 >= s_src.size() / 2)
            Pull();
          const double frac = s_pos - (double)index;
          for (int c = 0; c < 2; c++)
          {
            const double a = s_src[2 * index + c];
            const double b = s_src[2 * (index + 1) + c];
            out[2 * i + c] = (int16_t)lrint(a + (b - a) * frac);
          }
          s_pos += step;
        }
        // Drop what has been played, keeping the frame under the position.
        const size_t used = (size_t)s_pos;
        if (used > 0)
        {
          s_src.erase(s_src.begin(), s_src.begin() + 2 * used);
          s_pos -= (double)used;
        }
      }
      sceAudioOutOutput(s_handle, out);
      if (++grains == 1)
        InfoLog("Audio: first %u frames out at %u Hz.", kGrain, kOutRate);
    }
  }
}

int SDL_InitSubSystem(uint32_t)
{
  return 0;
}

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained)
{
  if (s_open)
  {
    PS5SDLSetError("Audio device is already opened");
    return -1;
  }
  if (!desired || !desired->callback || desired->format != AUDIO_S16SYS || desired->freq <= 0 ||
      (desired->channels != 1 && desired->channels != 2 && desired->channels != 4))
  {
    PS5SDLSetError("only signed 16-bit audio with 1, 2 or 4 channels and a callback");
    return -1;
  }
  if (!s_initialized)
  {
    const int rc = sceAudioOutInit();
    InfoLog("Audio: sceAudioOutInit 0x%x.", (unsigned)rc);
    s_initialized = true;
  }
  s_handle = sceAudioOutOpen(kUserSystem, kPortMain, 0, kGrain, kOutRate, kS16Stereo);
  if (s_handle < 0)
  {
    char message[64];
    snprintf(message, sizeof(message), "sceAudioOutOpen failed (0x%x)", (unsigned)s_handle);
    PS5SDLSetError(message);
    return -1;
  }
  s_spec = *desired;
  if (obtained)
    *obtained = s_spec;
  s_src.clear();
  s_pos = 0.0;
  s_paused = true;
  s_running = true;
  s_open = true;
  s_thread = std::thread(OutputThread);
  InfoLog("Audio: sceAudioOut port %d, %u-channel %d Hz resampled to %u Hz stereo, %u frames a pull.",
    s_handle, (unsigned)s_spec.channels, s_spec.freq, kOutRate, (unsigned)s_spec.samples);
  return 0;
}

SDL_AudioDeviceID SDL_OpenAudioDevice(const char *, int, const SDL_AudioSpec *, SDL_AudioSpec *, int)
{
  PS5SDLSetError("the PS5 has only the default audio device");
  return 0;
}

void SDL_PauseAudio(int pause_on)
{
  s_paused = pause_on != 0;
}

void SDL_PauseAudioDevice(SDL_AudioDeviceID, int pause_on)
{
  SDL_PauseAudio(pause_on);
}

void SDL_LockAudio(void)
{
  s_lock.lock();
}

void SDL_UnlockAudio(void)
{
  s_lock.unlock();
}

void SDL_LockAudioDevice(SDL_AudioDeviceID)
{
  SDL_LockAudio();
}

void SDL_UnlockAudioDevice(SDL_AudioDeviceID)
{
  SDL_UnlockAudio();
}

void SDL_CloseAudio(void)
{
  if (!s_open)
    return;
  s_running = false;
  if (s_thread.joinable())
    s_thread.join();
  sceAudioOutClose(s_handle);
  s_handle = -1;
  s_open = false;
}

void SDL_CloseAudioDevice(SDL_AudioDeviceID)
{
  SDL_CloseAudio();
}
