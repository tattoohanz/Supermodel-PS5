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
 * Audio.cpp
 *
 * PS5, first step: no output yet. The sound board still runs (synchronously,
 * AsyncSoundBoardThread off - nothing here would wake the asynchronous
 * thread), its samples are counted and dropped. sceAudioOut comes next.
 */

#include "OSD/Audio.h"
#include "Supermodel.h"

#include <atomic>

static constexpr unsigned SAMPLE_RATE_M3 = 44100;
static constexpr unsigned MIN_SND_FREQ = 45;
static constexpr float MAX_SND_FREQ = 75.0f;

static double s_samplesPerFrameExact = (double)SAMPLE_RATE_M3 / 60.0;
static double s_sampleAccumulator = 0.0;
static std::atomic<unsigned long long> s_samplesDropped{ 0 };
static bool s_opened = false;

int GetSamplesThisFrame(void)
{
  s_sampleAccumulator += s_samplesPerFrameExact;
  const int samples = (int)s_sampleAccumulator;
  s_sampleAccumulator -= (double)samples;
  return samples;
}

unsigned GetMaxSamplesPerFrame(void)
{
  return SAMPLE_RATE_M3 / MIN_SND_FREQ;
}

void SetAudioCallback(AudioCallbackFPtr, void *)
{
}

void SetAudioEnabled(bool)
{
}

void SetAudioType(Game::AudioTypes)
{
}

Result OpenAudio(const Util::Config::Node &config)
{
  float hz = config["RefreshRate"].ValueAsDefault<float>(0.0f);
  if (hz <= 0.0f)
    hz = 60.0f;
  if (hz > MAX_SND_FREQ)
    hz = MAX_SND_FREQ;
  if (hz < (float)MIN_SND_FREQ)
    hz = (float)MIN_SND_FREQ;
  s_samplesPerFrameExact = (double)SAMPLE_RATE_M3 / (double)hz;
  s_sampleAccumulator = 0.0;
  s_samplesDropped = 0;
  s_opened = true;
  InfoLog("Audio: no output on the PS5 yet; %.4f samples per frame at %.4f Hz are dropped.",
    s_samplesPerFrameExact, hz);
  return Result::OKAY;
}

bool OutputAudio(unsigned numSamples, const float *, const float *, const float *, const float *, bool)
{
  s_samplesDropped += numSamples;
  return false;
}

bool IsAudioBufferFull(void)
{
  return false;
}

void CloseAudio()
{
  if (!s_opened)
    return;
  s_opened = false;
  InfoLog("Audio: %llu samples generated (dropped).", (unsigned long long)s_samplesDropped.load());
}

void GetAudioBufferStats(unsigned *underruns, unsigned *overruns)
{
  if (underruns)
    *underruns = 0;
  if (overruns)
    *overruns = 0;
}

// Generated samples so far, for the frame-rate log line.
unsigned long long PS5AudioSamplesGenerated()
{
  return s_samplesDropped.load();
}
