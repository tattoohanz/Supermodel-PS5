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
 * Cost.cpp
 */

#include "Cost.h"

#include <atomic>
#include <ctime>

static std::atomic<uint64_t> s_ns[PS5_COST__COUNT];
static std::atomic<uint64_t> s_counts[PS5_COST__COUNT];

uint64_t PS5CostNow()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

uint64_t PS5CostThreadCpu()
{
  struct timespec ts;
  if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0)
    return 0;
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

void PS5CostAdd(int slot, uint64_t ns)
{
  if (slot < 0 || slot >= PS5_COST__COUNT)
    return;
  s_ns[slot].fetch_add(ns, std::memory_order_relaxed);
  s_counts[slot].fetch_add(1, std::memory_order_relaxed);
}

void PS5CostTake(uint64_t ns[PS5_COST__COUNT], uint64_t counts[PS5_COST__COUNT])
{
  for (int i = 0; i < PS5_COST__COUNT; i++)
  {
    ns[i] = s_ns[i].exchange(0, std::memory_order_relaxed);
    counts[i] = s_counts[i].exchange(0, std::memory_order_relaxed);
  }
}
