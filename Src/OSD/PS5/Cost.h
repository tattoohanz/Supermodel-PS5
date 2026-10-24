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
 * Cost.h
 *
 * PS5 diagnostics: where a frame's time goes. CModel3 (PS5 builds only)
 * and the front end add nanoseconds to these slots; Main.cpp logs them every
 * 5 s as milliseconds per emulated frame ("ps5 cost: ...").
 */

#ifndef INCLUDED_PS5_COST_H
#define INCLUDED_PS5_COST_H

#include <cstdint>

enum PS5CostSlot
{
  PS5_COST_PPC,         // RunMainBoardFrame, wall time (PowerPC thread)
  PS5_COST_PPC_CPU,     // the same, the thread's own CPU time
  PS5_COST_SYNC,        // SyncGPUs
  PS5_COST_RENDER,      // RenderFrame, all of it (main thread)
  PS5_COST_VIDEO_BEGIN, // BeginFrameVideo: frame fence wait and image acquire
  PS5_COST_VIDEO_END,   // EndFrameVideo: submit and present
  PS5_COST_WAIT,        // RunFrame waiting for the board threads after rendering
  PS5_COST_DRIVE,       // RunDriveBoardFrame
  PS5_COST_SOUND,       // RunSoundBoardFrame (asynchronous: per second, not per frame)
  PS5_COST_SOUND_CPU,   // the same, the thread's own CPU time
  PS5_COST__COUNT
};

uint64_t PS5CostNow();          // CLOCK_MONOTONIC, ns
uint64_t PS5CostThreadCpu();    // CLOCK_THREAD_CPUTIME_ID, ns (0 if the console has none)
void PS5CostAdd(int slot, uint64_t ns);

// Totals since the last call (ns), and how many times each slot was added to.
void PS5CostTake(uint64_t ns[PS5_COST__COUNT], uint64_t counts[PS5_COST__COUNT]);

#endif  // INCLUDED_PS5_COST_H
