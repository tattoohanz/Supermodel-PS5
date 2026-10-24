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
 * Thread.cpp
 *
 * OSD/Thread.h on pthreads, with SDL2's semantics (the SDL port's
 * implementation): mutexes are recursive, semaphores count.
 */

#include "OSD/Thread.h"
#include "Supermodel.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <pthread.h>
#include <unistd.h>

extern "C" int scePthreadGetaffinity(pthread_t thread, unsigned long long *mask);

namespace
{
  struct ThreadImpl
  {
    pthread_t thread;
    ThreadStart start;
    void *param;
    int result;
    UINT32 id;
    std::string name;
  };

  struct SemaphoreImpl
  {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    UINT32 value;
  };

  char s_lastError[128] = "";

  void SetError(const char *what, int err)
  {
    snprintf(s_lastError, sizeof(s_lastError), "%s: %s (%d)", what, strerror(err), err);
  }

  void *ThreadEntry(void *arg)
  {
    ThreadImpl *impl = (ThreadImpl *)arg;
    // Which CPUs the console lets the thread run on (diagnostic).
    unsigned long long mask = 0;
    const int rc = scePthreadGetaffinity(pthread_self(), &mask);
    InfoLog("Thread: '%s' running, CPU mask 0x%llx (0x%x).", impl->name.c_str(), mask, (unsigned)rc);
    impl->result = impl->start(impl->param);
    return nullptr;
  }

  UINT32 s_nextId = 1;
}

void CThread::Sleep(UINT32 ms)
{
  usleep((useconds_t)ms * 1000);
}

UINT32 CThread::GetTicks()
{
  static struct timespec start;
  static bool started = false;
  if (!started)
  {
    clock_gettime(CLOCK_MONOTONIC, &start);
    started = true;
  }
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (UINT32)((now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000);
}

CThread *CThread::CreateThread(const std::string &name, ThreadStart start, void *startParam)
{
  ThreadImpl *impl = new ThreadImpl();
  impl->start = start;
  impl->param = startParam;
  impl->result = 0;
  impl->id = __atomic_fetch_add(&s_nextId, 1, __ATOMIC_RELAXED);
  impl->name = name;
  // No attributes: the platform layer's pthread_create wrap gives a thread
  // that asks for no stack 2 MiB in direct memory, as it does RADV's.
  int err = pthread_create(&impl->thread, nullptr, ThreadEntry, impl);
  if (err != 0)
  {
    SetError("pthread_create", err);
    delete impl;
    return nullptr;
  }
  return new CThread(name, impl);
}

CSemaphore *CThread::CreateSemaphore(UINT32 initVal)
{
  SemaphoreImpl *impl = new SemaphoreImpl();
  pthread_mutex_init(&impl->mutex, nullptr);
  pthread_cond_init(&impl->cond, nullptr);
  impl->value = initVal;
  return new CSemaphore(impl);
}

CCondVar *CThread::CreateCondVar()
{
  pthread_cond_t *impl = new pthread_cond_t;
  int err = pthread_cond_init(impl, nullptr);
  if (err != 0)
  {
    SetError("pthread_cond_init", err);
    delete impl;
    return nullptr;
  }
  return new CCondVar(impl);
}

CMutex *CThread::CreateMutex()
{
  pthread_mutex_t *impl = new pthread_mutex_t;
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  int err = pthread_mutex_init(impl, &attr);
  pthread_mutexattr_destroy(&attr);
  if (err != 0)
  {
    SetError("pthread_mutex_init", err);
    delete impl;
    return nullptr;
  }
  return new CMutex(impl);
}

const char *CThread::GetLastError()
{
  return s_lastError;
}

CThread::CThread(const std::string &name, void *impl)
  : m_name(name),
    m_impl(impl)
{
}

CThread::~CThread()
{
  if (nullptr != m_impl)
    ErrorLog("Runaway thread error. A thread was not properly halted: %s\n", GetName().c_str());
}

const std::string &CThread::GetName() const
{
  return m_name;
}

UINT32 CThread::GetId()
{
  return m_impl ? ((ThreadImpl *)m_impl)->id : 0;
}

int CThread::Wait()
{
  if (m_impl == nullptr)
    return -1;
  ThreadImpl *impl = (ThreadImpl *)m_impl;
  pthread_join(impl->thread, nullptr);
  int status = impl->result;
  delete impl;
  m_impl = nullptr;
  return status;
}

CSemaphore::CSemaphore(void *impl) : m_impl(impl)
{
}

CSemaphore::~CSemaphore()
{
  SemaphoreImpl *impl = (SemaphoreImpl *)m_impl;
  pthread_cond_destroy(&impl->cond);
  pthread_mutex_destroy(&impl->mutex);
  delete impl;
}

UINT32 CSemaphore::GetValue()
{
  SemaphoreImpl *impl = (SemaphoreImpl *)m_impl;
  pthread_mutex_lock(&impl->mutex);
  UINT32 value = impl->value;
  pthread_mutex_unlock(&impl->mutex);
  return value;
}

bool CSemaphore::Wait()
{
  SemaphoreImpl *impl = (SemaphoreImpl *)m_impl;
  if (pthread_mutex_lock(&impl->mutex) != 0)
    return false;
  while (impl->value == 0)
    pthread_cond_wait(&impl->cond, &impl->mutex);
  impl->value--;
  pthread_mutex_unlock(&impl->mutex);
  return true;
}

bool CSemaphore::Post()
{
  SemaphoreImpl *impl = (SemaphoreImpl *)m_impl;
  if (pthread_mutex_lock(&impl->mutex) != 0)
    return false;
  impl->value++;
  pthread_cond_signal(&impl->cond);
  pthread_mutex_unlock(&impl->mutex);
  return true;
}

CCondVar::CCondVar(void *impl) : m_impl(impl)
{
}

CCondVar::~CCondVar()
{
  pthread_cond_destroy((pthread_cond_t *)m_impl);
  delete (pthread_cond_t *)m_impl;
}

bool CCondVar::Wait(CMutex *mutex)
{
  return pthread_cond_wait((pthread_cond_t *)m_impl, (pthread_mutex_t *)mutex->m_impl) == 0;
}

bool CCondVar::Signal()
{
  return pthread_cond_signal((pthread_cond_t *)m_impl) == 0;
}

bool CCondVar::SignalAll()
{
  return pthread_cond_broadcast((pthread_cond_t *)m_impl) == 0;
}

CMutex::CMutex(void *impl) : m_impl(impl)
{
}

CMutex::~CMutex()
{
  pthread_mutex_destroy((pthread_mutex_t *)m_impl);
  delete (pthread_mutex_t *)m_impl;
}

bool CMutex::Lock()
{
  return pthread_mutex_lock((pthread_mutex_t *)m_impl) == 0;
}

bool CMutex::Unlock()
{
  return pthread_mutex_unlock((pthread_mutex_t *)m_impl) == 0;
}
