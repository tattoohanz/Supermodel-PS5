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
 * PS5App.cpp
 *
 * After XPSemu Chihiro's ui/xemu-os-utils-ps5.c; the HEN protocol and its
 * timing are PS5SX2's (ProsperoHenJailbreak.cpp).
 */

#include "PS5App.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
int sceKernelUsleep(unsigned int microseconds);
int sceKernelSendNotificationRequest(int device, void *request, size_t size, int blocking);
uid_t __real_geteuid(void);
gid_t __real_getegid(void);

// geteuid/getegid (linked with --wrap): the real IDs. Mesa's disk shader
// cache is off when they differ from getuid/getgid, as they may after the
// jailbreak.
uid_t __wrap_geteuid(void) { return getuid(); }
gid_t __wrap_getegid(void) { return getgid(); }
}

// Where the eboot is loaded: imports.txt's offsets count from here.
static const uint64_t EBOOT_BASE = 0x400000ULL;

static const char HEN_REQUEST[] = "/download0/etahen_jailbreak";
static const char HEN_REQUEST_STAGED[] = "/download0/etahen_jailbreak.tmp";
static const unsigned HEN_POLL_US = 16667;
static const int HEN_MAX_POLLS = 600;
static const int HEN_POST_CONSUME_POLLS = 450;

static struct timespec s_start;
static const char *s_titleDir = "/data/homebrew/" PS5_APP_TITLE_ID;

const char *PS5AppTitleDir()
{
  return s_titleDir;
}

double PS5AppSeconds()
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (double)(now.tv_sec - s_start.tv_sec) + (double)(now.tv_nsec - s_start.tv_nsec) / 1e9;
}

void PS5Notify(const char *message)
{
  static uint8_t request[0xc30];
  memset(request, 0, sizeof(request));
  snprintf((char *)request + 0x2d, 1024, "%s", message);
  sceKernelSendNotificationRequest(0, request, sizeof(request), 0);
}

static bool HenJailbreak(char *msg, size_t size)
{
  int pid = getpid();
  int uidBefore = (int)__real_geteuid();
  if (uidBefore == 0)
  {
    snprintf(msg, size, "already root (pid %d)", pid);
    return true;
  }
  char request[32];
  int len = snprintf(request, sizeof(request), "{\"PID\":%d}\n", pid);
  if ((unlink(HEN_REQUEST) != 0 && errno != ENOENT) ||
      (unlink(HEN_REQUEST_STAGED) != 0 && errno != ENOENT))
  {
    snprintf(msg, size, "stale request cleanup failed, errno %d", errno);
    return false;
  }
  int fd = open(HEN_REQUEST_STAGED, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
  if (fd < 0)
  {
    snprintf(msg, size, "request open failed, errno %d", errno);
    return false;
  }
  bool ok = fchmod(fd, 0666) == 0 && write(fd, request, len) == len && fsync(fd) == 0;
  ok = close(fd) == 0 && ok;
  if (!ok || rename(HEN_REQUEST_STAGED, HEN_REQUEST) != 0)
  {
    snprintf(msg, size, "request write failed, errno %d", errno);
    unlink(HEN_REQUEST_STAGED);
    return false;
  }
  int polls = 0;
  while (access(HEN_REQUEST, F_OK) == 0)
  {
    if (++polls >= HEN_MAX_POLLS)
    {
      unlink(HEN_REQUEST);
      snprintf(msg, size, "helper.elf didn't answer (pid %d): load helper.elf with kstuff and "
        "start Supermodel again", pid);
      return false;
    }
    sceKernelUsleep(HEN_POLL_US);
  }
  int uid = (int)__real_geteuid();
  for (int i = 0; i < HEN_POST_CONSUME_POLLS && uid != 0; i++)
  {
    sceKernelUsleep(HEN_POLL_US);
    uid = (int)__real_geteuid();
  }
  snprintf(msg, size, "request taken after %d polls, uid %d -> %d", polls, uidBefore, uid);
  return true;  // /data access is what counts, checked by the log
}

static void ReportUnresolvedImports()
{
  static const char *const paths[] = {
    "/app0/imports.txt",
    "/data/homebrew/" PS5_APP_TITLE_ID "/imports.txt",
  };
  FILE *f = nullptr;
  const char *path = nullptr;
  for (size_t i = 0; !f && i < sizeof(paths) / sizeof(paths[0]); i++)
  {
    path = paths[i];
    f = fopen(path, "r");
  }
  if (!f)
  {
    fprintf(stderr, "ps5: can't read imports.txt, errno %d\n", errno);
    return;
  }
  fprintf(stderr, "ps5: imports from %s\n", path);
  char name[128], list[400] = "";
  unsigned long offset;
  int checked = 0, missing = 0;
  while (fscanf(f, "%lx %127s", &offset, name) == 2)
  {
    checked++;
    if (*(const uint64_t *)(EBOOT_BASE + offset) == 0)
    {
      fprintf(stderr, "ps5: unresolved import: %s\n", name);
      if (missing++ < 10)
        snprintf(list + strlen(list), sizeof(list) - strlen(list), "%s%s", missing > 1 ? " " : "", name);
    }
  }
  fclose(f);
  fprintf(stderr, "ps5: %d of %d imports unresolved\n", missing, checked);
  if (missing)
  {
    char toast[512];
    snprintf(toast, sizeof(toast), "Supermodel: %d unresolved imports: %s", missing, list);
    PS5Notify(toast);
  }
}

bool PS5AppInit()
{
  clock_gettime(CLOCK_MONOTONIC, &s_start);

  char msg[192];
  bool jailbroken = HenJailbreak(msg, sizeof(msg));

  static const char *const dirs[] = {
    PS5_APP_DIR, PS5_APP_ROMS, PS5_APP_DIR "/Config", PS5_APP_DIR "/NVRAM", PS5_APP_DIR "/Saves",
    PS5_APP_DIR "/Screenshots", PS5_APP_DIR "/cache"
  };
  for (const char *dir : dirs)
    mkdir(dir, 0777);

  // RADV's disk shader cache.
  setenv("MESA_SHADER_CACHE_DIR", PS5_APP_DIR "/cache", 1);
  setenv("MESA_SHADER_CACHE_MAX_SIZE", "256M", 1);

  rename(PS5_APP_LOG, PS5_APP_LOG ".old");
  bool logging = freopen(PS5_APP_LOG, "w", stderr) != nullptr;
  if (logging)
  {
    // Line buffered: unbuffered, the console's libc writes stderr a character
    // at a time, and every write to /data is slow.
    static char buf[8192];
    setvbuf(stderr, buf, _IOLBF, sizeof(buf));
    if (fileno(stderr) != STDERR_FILENO)
      dup2(fileno(stderr), STDERR_FILENO);
    dup2(fileno(stderr), STDOUT_FILENO);
    setvbuf(stdout, nullptr, _IOLBF, 0);
  }
  fprintf(stderr, "ps5: Supermodel %s, jailbreak %s: %s\n", PS5_APP_TITLE_ID,
    jailbroken ? "ok" : "NOT CONFIRMED", msg);
  if (!logging)
  {
    char toast[256];
    snprintf(toast, sizeof(toast), "Supermodel: can't write %s (%s)", PS5_APP_LOG, msg);
    PS5Notify(toast);
    return false;
  }
  // After the jailbreak the root is the console's own, without /app0.
  static const char *const titleDirs[] = { "/app0", "/data/homebrew/" PS5_APP_TITLE_ID };
  for (const char *dir : titleDirs)
  {
    char probe[128];
    snprintf(probe, sizeof(probe), "%s/Config/Games.xml", dir);
    if (access(probe, R_OK) == 0)
    {
      s_titleDir = dir;
      break;
    }
  }
  fprintf(stderr, "ps5: title folder %s\n", s_titleDir);
  ReportUnresolvedImports();
  return true;
}
