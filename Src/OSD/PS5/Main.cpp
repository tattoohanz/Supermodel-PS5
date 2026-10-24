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
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

/*
 * Main.cpp
 *
 * The PS5 front end (title PPSA97374). A slim version of OSD/SDL/Main.cpp's
 * RunGameSession() and Supermodel(): one game, started straight from
 * /data/supermodel/roms/, on the Vulkan renderer, presented on VideoOut.
 *
 * First step: picture only. No sound output and no controls yet (the game
 * runs its attract mode).
 *
 * The game is srally2 unless /data/supermodel/autoboot.txt names another ROM
 * set (its first line: a set name, or a full path to a .zip).
 * /data/supermodel/Config/Supermodel.ini, when present, is applied over the
 * defaults, the same as on the desktop (global section, then the game's).
 */

#include "Supermodel.h"
#include "BlockFile.h"
#include "GameLoader.h"
#include "ROMSet.h"
#include "Util/Format.h"
#include "Util/NewConfig.h"
#include "Util/ConfigBuilders.h"
#include "OSD/Audio.h"
#include "OSD/FileSystemPath.h"
#include "OSD/Video.h"
#include "Inputs/Inputs.h"
#include "Inputs/InputSystem.h"
#include "Model3/Model3.h"
#include "Graphics/Vulkan3D/VulkanContext.h"
#include "Graphics/Vulkan3D/Vulkan2D.h"
#include "Graphics/Vulkan3D/Vulkan3D.h"

#include "Display.h"
#include "PS5App.h"
#include "PS5InputSystem.h"
#include "Diag.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

unsigned long long PS5AudioSamplesGenerated();

/******************************************************************************
 Diagnostics (Diag.h): the picture's stages dumped 15, 30 and 45 s in
******************************************************************************/

static const char *s_diagTag = nullptr;

const char *PS5DiagDumpTag()
{
  return s_diagTag;
}

/******************************************************************************
 Configuration
******************************************************************************/

// In the title's folder, known once PS5AppInit has run (RunSession sets it).
static std::string s_gameXMLFilePath;
static const std::string s_configFilePath = PS5_APP_DIR "/Config/Supermodel.ini";
static const char s_autobootPath[] = PS5_APP_DIR "/autoboot.txt";
static const char s_defaultGame[] = "srally2";

static Util::Config::Node s_runtime_config("Global");

// The desktop build's DefaultConfig(), taken from OSD/SDL/Main.cpp when the
// title is built (ps5/build-title.sh), so every key the core reads exists.
// Without the control preset at its end: no pads yet.
#include "DefaultConfig.inc"

static Util::Config::Node PS5DefaultConfig()
{
  Util::Config::Node config = UpstreamDefaultConfig();
  config.Set("GameXMLFile", s_gameXMLFilePath);
  config.Set<std::string>("ROMPath", PS5_APP_ROMS);
  // Nothing wakes the asynchronous sound board thread here (no audio
  // callback yet, see Audio.cpp): run the sound board with each frame.
  config.Set("AsyncSoundBoardThread", false);
  config.Set("Vulkan3DEngine", true);
  config.Set("FullScreen", true);
  config.Set("HighPriority", false);
  config.Set<std::string>("Outputs", "none");
  return config;
}

/******************************************************************************
 Log
******************************************************************************/

// Everything to stderr (the log file), with the time since start.
class CPS5Logger : public CLogger
{
public:
  explicit CPS5Logger(LogLevel level) : m_level(level) {}

  void DebugLog(const char *fmt, va_list vl) override
  {
    if (m_level <= LogLevel::Debug)
      Write("debug: ", fmt, vl);
  }

  void InfoLog(const char *fmt, va_list vl) override
  {
    if (m_level <= LogLevel::Info)
      Write("", fmt, vl);
  }

  void ErrorLog(const char *fmt, va_list vl) override
  {
    if (m_level <= LogLevel::Error)
      Write("error: ", fmt, vl);
  }

private:
  void Write(const char *prefix, const char *fmt, va_list vl)
  {
    char text[2048];
    vsnprintf(text, sizeof(text), fmt, vl);
    size_t n = strlen(text);
    while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r'))
      text[--n] = '\0';
    std::lock_guard<std::mutex> lock(m_mutex);
    fprintf(stderr, "[%9.3f] %s%s\n", PS5AppSeconds(), prefix, text);
  }

  LogLevel m_level;
  std::mutex m_mutex;
};

/******************************************************************************
 NVRAM (as OSD/SDL/Main.cpp)
******************************************************************************/

static const int NVRAM_FILE_VERSION = 0;

static void SaveNVRAM(IEmulator *Model3)
{
  CBlockFile NVRAM;
  std::string file_path = Util::Format() << FileSystemPath::GetPath(FileSystemPath::NVRAM) << Model3->GetGame().name << ".nv";
  if (Result::OKAY != NVRAM.Create(file_path, "Supermodel NVRAM State", "Supermodel Version " SUPERMODEL_VERSION))
  {
    ErrorLog("Unable to save NVRAM to '%s'.", file_path.c_str());
    return;
  }
  int32_t fileVersion = NVRAM_FILE_VERSION;
  NVRAM.Write(&fileVersion, sizeof(fileVersion));
  NVRAM.Write(Model3->GetGame().name);
  Model3->SaveNVRAM(&NVRAM);
  NVRAM.Close();
  InfoLog("Saved NVRAM to '%s'.", file_path.c_str());
}

static void LoadNVRAM(IEmulator *Model3)
{
  CBlockFile NVRAM;
  std::string file_path = Util::Format() << FileSystemPath::GetPath(FileSystemPath::NVRAM) << Model3->GetGame().name << ".nv";
  struct stat st;
  if (stat(file_path.c_str(), &st) != 0)
  {
    InfoLog("NVRAM: no '%s' yet (first run).", file_path.c_str());
    return;
  }
  if (Result::OKAY != NVRAM.Load(file_path))
  {
    ErrorLog("Unable to load NVRAM from '%s'.", file_path.c_str());
    return;
  }
  if (Result::OKAY != NVRAM.FindBlock("Supermodel NVRAM State"))
  {
    ErrorLog("'%s' does not contain NVRAM data.", file_path.c_str());
    return;
  }
  int32_t fileVersion;
  NVRAM.Read(&fileVersion, sizeof(fileVersion));
  if (fileVersion != NVRAM_FILE_VERSION)
  {
    ErrorLog("'%s' is incompatible with this version of Supermodel.", file_path.c_str());
    return;
  }
  Model3->LoadNVRAM(&NVRAM);
  NVRAM.Close();
  InfoLog("Loaded NVRAM from '%s'.", file_path.c_str());
}

/******************************************************************************
 Video callbacks (OSD/Video.h)
******************************************************************************/

static Vulkan3D::CVulkanContext *s_vkContext = nullptr;

bool BeginFrameVideo()
{
  return s_vkContext ? s_vkContext->BeginFrame() : true;
}

void EndFrameVideo()
{
  if (s_vkContext)
    s_vkContext->EndFrame();
}

/******************************************************************************
 Session
******************************************************************************/

static uint64_t NowNs()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void SleepUntilNs(uint64_t target)
{
  for (;;)
  {
    uint64_t now = NowNs();
    if (now >= target)
      return;
    uint64_t left = target - now;
    // The kernel's sleep overshoots; sleep to within 2 ms, then spin.
    if (left > 2000000ull)
      usleep((useconds_t)((left - 1500000ull) / 1000ull));
    else
      __builtin_ia32_pause();
  }
}

static std::string ReadAutoboot()
{
  FILE *f = fopen(s_autobootPath, "r");
  if (!f)
    return s_defaultGame;
  char line[512] = "";
  if (!fgets(line, sizeof(line), f))
    line[0] = '\0';
  fclose(f);
  size_t n = strlen(line);
  while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t'))
    line[--n] = '\0';
  const char *start = line;
  while (*start == ' ' || *start == '\t')
    start++;
  if (*start == '\0')
    return s_defaultGame;
  InfoLog("autoboot.txt names '%s'.", start);
  return start;
}

static bool FileExists(const std::string &path)
{
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

// The game's picture inside the display: 496x384 kept in shape and as large
// as fits, unless Stretch is on (as CalcScreenGeometry in OSD/SDL/Main.cpp).
static void CalcScreenGeometry(unsigned totalX, unsigned totalY, bool keepAspect,
  unsigned *xOffset, unsigned *yOffset, unsigned *xRes, unsigned *yRes)
{
  float w = (float)totalX, h = (float)totalY;
  if (keepAspect)
  {
    const float ratio = 496.0f / 384.0f;
    if (w / h > ratio)
      w = h * ratio;
    else
      h = w / ratio;
  }
  *xRes = (unsigned)(w + 0.5f);
  *yRes = (unsigned)(h + 0.5f);
  *xOffset = (totalX - *xRes) / 2;
  *yOffset = (totalY - *yRes) / 2;
}

static int RunSession()
{
  s_gameXMLFilePath = std::string(PS5AppTitleDir()) + "/Config/Games.xml";

  // Configuration: defaults, Supermodel.ini's global section, the game's section.
  Util::Config::Node fileConfig("Global");
  if (FileExists(s_configFilePath))
  {
    Util::Config::FromINIFile(&fileConfig, s_configFilePath);
    InfoLog("Config: read '%s'.", s_configFilePath.c_str());
  }
  else
    InfoLog("Config: no '%s'; using the defaults.", s_configFilePath.c_str());
  Util::Config::Node globalConfig("Global");
  Util::Config::MergeINISections(&globalConfig, PS5DefaultConfig(), fileConfig);

  // The game.
  std::string rom = ReadAutoboot();
  std::string romPath = rom.find('/') != std::string::npos ? rom : std::string(PS5_APP_ROMS) + "/" + rom + ".zip";
  InfoLog("Game: loading '%s' (game list '%s').", romPath.c_str(), s_gameXMLFilePath.c_str());
  if (!FileExists(romPath))
  {
    ErrorLog("Game: '%s' is not there. Copy %s.zip to %s/.", romPath.c_str(), rom.c_str(), PS5_APP_ROMS);
    PS5Notify((std::string("Supermodel: ") + romPath + " not found").c_str());
    return 1;
  }
  Game game;
  ROMSet rom_set;
  {
    GameLoader loader(s_gameXMLFilePath);
    if (loader.Load(&game, &rom_set, romPath))
    {
      ErrorLog("Game: '%s' could not be loaded (see above).", romPath.c_str());
      PS5Notify((std::string("Supermodel: can't load ") + romPath).c_str());
      return 1;
    }
  }
  if (const Util::Config::Node *section = fileConfig.TryGet(game.name))
    Util::Config::MergeINISections(&s_runtime_config, globalConfig, *section);
  else
    s_runtime_config = globalConfig;
  InfoLog("Game: %s (%s), stepping %s.", game.title.c_str(), game.name.c_str(), game.stepping.c_str());

  // The display.
  const bool vsync = s_runtime_config["VSync"].ValueAsDefault<bool>(true);
  s_vkContext = new Vulkan3D::CVulkanContext();
  if (Result::OKAY != s_vkContext->InitDevice(PS5DisplayWindow(), false, vsync, ""))
  {
    ErrorLog("Vulkan: the renderer could not start (see above).");
    PS5Notify("Supermodel: Vulkan could not start (see Supermodel.log)");
    return 1;
  }
  const VkExtent2D extent = s_vkContext->Extent();
  unsigned totalXRes = extent.width, totalYRes = extent.height;
  unsigned xOffset, yOffset, xRes, yRes;
  CalcScreenGeometry(totalXRes, totalYRes, !s_runtime_config["Stretch"].ValueAs<bool>(), &xOffset, &yOffset, &xRes, &yRes);
  InfoLog("Video: display %ux%u, game picture %ux%u at %u,%u.", totalXRes, totalYRes, xRes, yRes, xOffset, yOffset);

  // The emulator.
  CModel3 *Model3 = new CModel3(s_runtime_config);
  if (Result::OKAY != Model3->Init())
  {
    ErrorLog("Model 3: Init failed.");
    return 1;
  }
  if (Model3->LoadGame(game, rom_set) != Result::OKAY)
  {
    ErrorLog("Model 3: LoadGame failed.");
    return 1;
  }
  rom_set = ROMSet();  // the ROMs are in the emulator now
  LoadNVRAM(Model3);

  SetAudioType(game.audio);
  if (Result::OKAY != OpenAudio(s_runtime_config))
    return 1;

  // Inputs (none yet: the attract mode runs).
  std::shared_ptr<CInputSystem> inputSystem = std::make_shared<CPS5InputSystem>();
  CInputs *Inputs = new CInputs(inputSystem);
  if (!Inputs->Initialize())
  {
    ErrorLog("Inputs: Initialize failed.");
    return 1;
  }
  Inputs->LoadFromConfig(s_runtime_config);
  Inputs->ApplyGameSpecificInputRanges(game);
  Model3->AttachInputs(Inputs);

  // Renderers.
  Vulkan3D::CVulkan2D *Render2D = new Vulkan3D::CVulkan2D(s_runtime_config, *s_vkContext);
  Vulkan3D::CVulkan3D *Render3D = new Vulkan3D::CVulkan3D(s_runtime_config, game.name, *s_vkContext, *Render2D);
  const UpscaleMode upscaleMode = (UpscaleMode)s_runtime_config["UpscaleMode"].ValueAs<int>();
  if (Result::OKAY != Render2D->Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes, 0, upscaleMode))
  {
    ErrorLog("Vulkan2D: Init failed.");
    return 1;
  }
  if (Result::OKAY != Render3D->Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes, 0))
  {
    ErrorLog("Vulkan3D: Init failed.");
    return 1;
  }
  Model3->AttachRenderers(Render2D, Render3D, nullptr);

  Model3->Reset();
  InfoLog("Session: running %s.", game.title.c_str());
  {
    char toast[256];
    snprintf(toast, sizeof(toast), "Supermodel: %s (picture only for now)", game.title.c_str());
    PS5Notify(toast);
  }

  // The loop: one emulated frame per refresh period, as the desktop loop paces it.
  const bool throttle = s_runtime_config["Throttle"].ValueAs<bool>();
  float refreshHz = s_runtime_config["RefreshRate"].ValueAs<float>();
  if (refreshHz <= 0.0f)
    refreshHz = 60.0f;
  const uint64_t frameNs = (uint64_t)(1e9 / refreshHz);
  InfoLog("Session: %.3f Hz pacing%s, display %.3f Hz, vsync %s.", refreshHz, throttle ? "" : " off",
    PS5DisplayRefreshMilliHz() / 1000.0, vsync ? "on" : "off");

  uint64_t nextTime = NowNs() + frameNs;
  const uint64_t sessionStart = NowNs();
  static const char *const diagTags[] = { "15s", "30s", "45s" };
  static const unsigned diagSeconds[] = { 15, 30, 45 };
  unsigned diagNext = 0;
  uint64_t statStart = NowNs();
  uint64_t runNs = 0, runMaxNs = 0;
  unsigned frames = 0;
  unsigned long long totalFrames = 0;
  unsigned long long samplesAtStat = PS5AudioSamplesGenerated();
  for (;;)
  {
    Inputs->Poll(&game, xOffset, yOffset, xRes, yRes);

    const uint64_t t0 = NowNs();
    if (diagNext < 3 && NowNs() - sessionStart >= diagSeconds[diagNext] * 1000000000ull)
    {
      s_diagTag = diagTags[diagNext++];
      InfoLog("diag: %s: dumping the picture's stages to %s/Screenshots/diag-%s-*.png.", s_diagTag, PS5_APP_DIR, s_diagTag);
    }
    Model3->RunFrame();
    s_diagTag = nullptr;
    const uint64_t dt = NowNs() - t0;
    runNs += dt;
    if (dt > runMaxNs)
      runMaxNs = dt;

    if (throttle)
    {
      SleepUntilNs(nextTime);
      nextTime += frameNs;
      const uint64_t now = NowNs();
      if (nextTime < now)
        nextTime = now;
    }

    frames++;
    totalFrames++;
    const uint64_t now = NowNs();
    if (now - statStart >= 5000000000ull)
    {
      const double secs = (now - statStart) / 1e9;
      const unsigned long long samples = PS5AudioSamplesGenerated();
      InfoLog("ps5: %.2f FPS, RunFrame avg %.2f ms max %.2f ms, sound %.0f samples/s, frame %llu",
        frames / secs, runNs / 1e6 / frames, runMaxNs / 1e6, (samples - samplesAtStat) / secs, totalFrames);
      statStart = now;
      runNs = runMaxNs = 0;
      frames = 0;
      samplesAtStat = samples;
    }
  }

  // Not reached yet (no way to quit without controls); kept for when there is.
  Model3->PauseThreads();
  SaveNVRAM(Model3);
  CloseAudio();
  delete Render2D;
  delete Render3D;
  return 0;
}

int main(int, char **)
{
  if (!PS5AppInit())
    return 1;
  SetLogger(std::make_shared<CPS5Logger>(CLogger::LogLevel::Info));
  InfoLog("%s on the PS5 (%s).", SUPERMODEL_TITLE_FULL, PS5_APP_TITLE_ID);

  int result = 1;
  try
  {
    result = RunSession();
  }
  catch (const std::exception &e)
  {
    ErrorLog("Exception: %s", e.what());
    PS5Notify((std::string("Supermodel: ") + e.what()).c_str());
  }
  if (result != 0)
  {
    InfoLog("Stopped (see the errors above).");
    // Stay up so the log can be read over FTP and the toast is seen.
    for (;;)
      sleep(60);
  }
  return result;
}
