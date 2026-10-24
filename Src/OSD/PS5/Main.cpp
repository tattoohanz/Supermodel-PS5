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
 * RunGameSession() and Supermodel(): the menu (Menu.cpp) lists the games in
 * /data/supermodel/roms/; the one chosen runs on the Vulkan renderer,
 * presented on VideoOut, until L3 is held for 2 s, and the menu is back. One
 * Vulkan context (and VideoOut) for the whole run.
 *
 * The DualSense is joystick 1 (PS5InputSystem.h); sound through sceAudioOut
 * (AudioOut.cpp under the desktop Audio.cpp).
 *
 * /data/supermodel/autoboot.txt, when there, names a game to start at once
 * (its first line: a set name, or a full path to a .zip); the menu after it.
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
#include "Menu.h"
#include "PS5App.h"
#include "PS5InputSystem.h"
#include "PS5Pad.h"
#include "Diag.h"
#include "Cost.h"

#include <cstdarg>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

extern "C" int scePthreadGetaffinity(pthread_t thread, unsigned long long *mask);

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
static const char s_lastGamePath[] = PS5_APP_DIR "/Config/menu-last.txt";
static const char s_menuSettingsPath[] = PS5_APP_DIR "/Config/MenuSettings.ini";
static CMenuSettings s_menuSettings;
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
  // sceAudioOut is stereo (AudioOut.cpp); the four Model 3 channels are
  // mixed down by Audio.cpp as on a stereo desktop.
  config.Set("NbSoundChannels", 2);
  // 100 ms of sound buffer rather than the desktop's 400: the sound board
  // runs from the audio callback, so the buffer needs no room for frame-loop
  // stalls, and 400 ms was a noticeable delay (no under-runs at 10 on the
  // console).
  config.Set("AudioLatency", 10);
  config.Set("Vulkan3DEngine", true);
  config.Set("FullScreen", true);
  config.Set("HighPriority", false);
  config.Set<std::string>("Outputs", "none");

  // The DualSense (PS5InputSystem.h has the numbering). The desktop defaults
  // already put Start on Options, steering on the left stick, shifting on R1
  // (up) and L1 (down) and neutral on Cross. On the PS5, as in XPSemu
  // Chihiro: Coin on D-pad up (Create opens the console's capture menu),
  // Service on D-pad left, Test on D-pad right (also R3 and the touchpad
  // click; the D-pad no longer steers). R2 accelerates (or Square), L2
  // brakes, Circle is the handbrake (or D-pad down), Triangle changes the
  // view. Games with an analog joystick (Star Wars Trilogy, whose input test
  // calls them fire 0, event 0, fire 1, event 1): Cross, Circle, Square,
  // Triangle, or a mouse (its movement, then left, right, middle and side
  // button; the desktop defaults already have the mouse on the stick's axes
  // and on the guns). Where the D-pad is the game's joystick
  // RunSession takes Service and Test off it (ServiceTestForGame).
  config.Set<std::string>("InputCoin1", "JOY1_POV1_UP");
  config.Set<std::string>("InputSteeringLeft", "NONE");
  config.Set<std::string>("InputSteeringRight", "NONE");
  config.Set<std::string>("InputAnalogJoyTrigger", "JOY1_BUTTON1,MOUSE_LEFT_BUTTON");
  config.Set<std::string>("InputAnalogJoyEvent", "JOY1_BUTTON2,MOUSE_RIGHT_BUTTON");
  config.Set<std::string>("InputAnalogJoyTrigger2", "JOY1_BUTTON3,MOUSE_MIDDLE_BUTTON");
  config.Set<std::string>("InputAnalogJoyEvent2", "JOY1_BUTTON4,MOUSE_BUTTON4");
  // The pedals are analog (not axis) inputs, which take only half axes: the
  // positive half of a trigger is all of it (0 released, 32767 pressed).
  config.Set<std::string>("InputAccelerator", "JOY1_RZAXIS_POS,JOY1_BUTTON3");
  config.Set<std::string>("InputBrake", "JOY1_ZAXIS_POS");
  config.Set<std::string>("InputHandBrake", "JOY1_BUTTON2,JOY1_POV1_DOWN");
  config.Set<std::string>("InputViewChange", "JOY1_BUTTON4");
  config.Set<std::string>("InputServiceA", "JOY1_POV1_LEFT,JOY1_BUTTON10");
  config.Set<std::string>("InputTestA", "JOY1_POV1_RIGHT,JOY1_BUTTON11");
  return config;
}

/*
 * Service and Test stay off the D-pad where it is the game's joystick (the
 * fighting games, Virtua Striker, Spikeout: InputJoyLeft/Right are on it),
 * or every step left or right would open the service menu: R3 and the
 * touchpad there. Not where Supermodel.ini sets them.
 */
static void ServiceTestForGame(const Game &game, const Util::Config::Node &fileConfig,
  const Util::Config::Node *gameSection)
{
  const bool dpadIsJoystick = (game.inputs & (Game::INPUT_JOYSTICK1 | Game::INPUT_JOYSTICK2)) != 0;
  if (dpadIsJoystick)
  {
    const char *const keys[][2] = { { "InputServiceA", "JOY1_BUTTON10" }, { "InputTestA", "JOY1_BUTTON11" } };
    for (const auto &key : keys)
      if (!fileConfig.TryGet(key[0]) && !(gameSection && gameSection->TryGet(key[0])))
        s_runtime_config.Set<std::string>(key[0], key[1]);
  }
  InfoLog("Input: Service on %s, Test on %s.", s_runtime_config["InputServiceA"].ValueAs<std::string>().c_str(),
    s_runtime_config["InputTestA"].ValueAs<std::string>().c_str());
}

// Per game, over the defaults and under Supermodel.ini (its global section
// and the game's own still win). By ROM set; a clone takes its parent's.
struct PS5GameDefault
{
  const char *set;
  unsigned ppcMHz;  // PowerPCFrequency; 0 = the stepping's own (66/100/166)
};

static const PS5GameDefault s_gameDefaults[] =
{
  // At the real 166 MHz the PowerPC interpreter needs 19-20 ms a frame on the
  // console (51-55 FPS); at 120 MHz 13-15 ms, 59.94 FPS, and the game showed
  // no slowdown, also at the start of a race.
  { "srally2", 120 },
};

static const PS5GameDefault *FindGameDefault(const Game &game)
{
  for (const PS5GameDefault &d : s_gameDefaults)
    if (game.name == d.set)
      return &d;
  for (const PS5GameDefault &d : s_gameDefaults)
    if (game.parent == d.set)
      return &d;
  return nullptr;
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
  const uint64_t start = PS5CostNow();
  const bool ok = s_vkContext ? s_vkContext->BeginFrame() : true;
  PS5CostAdd(PS5_COST_VIDEO_BEGIN, PS5CostNow() - start);
  return ok;
}

void EndFrameVideo()
{
  if (!s_vkContext)
    return;
  const uint64_t start = PS5CostNow();
  s_vkContext->EndFrame();
  PS5CostAdd(PS5_COST_VIDEO_END, PS5CostNow() - start);

  // The system keeps showing the title's launch picture over VideoOut until
  // the app takes it down (as XPSemu Chihiro does after its first frame).
  PS5HideSplashOnce("game frame");
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

// The first line of a small text file, trimmed; empty when there is none.
static std::string ReadLine(const char *path)
{
  FILE *f = fopen(path, "r");
  if (!f)
    return std::string();
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
  return start;
}

static bool FileExists(const std::string &path)
{
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

/*
 * The game's own bezel, when the default one (BezelMode 1, no BezelImage) is
 * set: <ROM set>.png from /data/supermodel/Bezels/ (the user's) or the
 * title's Assets/Bezels/, trying the set, then its parent, each with the
 * alias below. Otherwise the built-in Supermodel3Bezel.png stays.
 */
static void ChooseGameBezel(const Game &game)
{
  if (s_runtime_config["BezelMode"].ValueAsDefault<int>(1) != 1 ||
      !s_runtime_config["BezelImage"].ValueAsDefault<std::string>("").empty())
    return;
  // Sets drawn with another set's bezel.
  static const char *const aliases[][2] = {
    { "scudplusa", "scudplus" },  // Scud Race Plus, its parent is scud
    { "vs298", "vs2" },           // Virtua Striker 2 '98
  };
  std::vector<std::string> names;
  auto add = [&names](const std::string &name)
  {
    if (name.empty())
      return;
    names.push_back(name);
    for (const auto &alias : aliases)
      if (name == alias[0])
        names.push_back(alias[1]);
  };
  add(game.name);
  add(game.parent);
  const std::string dirs[] = { std::string(PS5_APP_DIR) + "/Bezels/", std::string(PS5AppTitleDir()) + "/Assets/Bezels/" };
  for (const std::string &name : names)
  {
    for (const std::string &dir : dirs)
    {
      const std::string path = dir + name + ".png";
      if (FileExists(path))
      {
        s_runtime_config.Set("BezelMode", 2);
        s_runtime_config.Set("BezelImage", path);
        InfoLog("Bezel: '%s' for %s.", path.c_str(), game.name.c_str());
        return;
      }
    }
  }
  InfoLog("Bezel: none for %s; the default one.", game.name.c_str());
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

/*
 * The games: the .zip files in /data/supermodel/roms/ named after a ROM set
 * in Games.xml, by title.
 */
static std::vector<MenuGame> ScanGames(const GameLoader &loader)
{
  std::vector<MenuGame> games;
  const auto &known = loader.GetGames();
  DIR *dir = opendir(PS5_APP_ROMS);
  if (!dir)
  {
    ErrorLog("Games: '%s' cannot be read.", PS5_APP_ROMS);
    return games;
  }
  while (dirent *e = readdir(dir))
  {
    std::string name = e->d_name;
    if (name.size() < 5)
      continue;
    std::string ext = name.substr(name.size() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".zip")
      continue;
    std::string set = name.substr(0, name.size() - 4);
    std::transform(set.begin(), set.end(), set.begin(), ::tolower);
    auto it = known.find(set);
    if (it == known.end())
    {
      InfoLog("Games: '%s' is not a set in Games.xml; left out.", name.c_str());
      continue;
    }
    const Game &game = it->second;
    MenuGame g;
    g.entry.set = set;
    g.entry.title = game.title;
    g.entry.manufacturer = game.manufacturer;
    g.entry.version = game.version;
    g.entry.year = game.year;
    g.entry.hardware = "Model 3 Step " + game.stepping;
    g.entry.sound = game.mpeg_board;
    g.parent = game.parent;
    g.zipPath = std::string(PS5_APP_ROMS) + "/" + name;
    MenuFindArt(g);
    games.push_back(g);
  }
  closedir(dir);
  std::sort(games.begin(), games.end(), [](const MenuGame &a, const MenuGame &b)
  {
    return strcasecmp(a.entry.title.c_str(), b.entry.title.c_str()) < 0;
  });
  InfoLog("Games: %d in %s.", (int)games.size(), PS5_APP_ROMS);
  for (const MenuGame &g : games)
    InfoLog("Games:   %-10s %s; art:%s%s%s%s", g.entry.set.c_str(), g.entry.title.c_str(),
      g.marquee.empty() ? "" : " marquee", g.boxart.empty() ? "" : " box", g.backdrop.empty() ? "" : " backdrop",
      g.logo.empty() ? "" : " logo");
  return games;
}

/*
 * For each game, what it gets for the menu settings' keys from the defaults
 * and Supermodel.ini (read again each time, as it may have been edited).
 */
static void FillMenuBase(std::vector<MenuGame> &games)
{
  Util::Config::Node fileConfig("Global");
  if (FileExists(s_configFilePath))
    Util::Config::FromINIFile(&fileConfig, s_configFilePath);
  const Util::Config::Node defaults = PS5DefaultConfig();
  for (MenuGame &g : games)
  {
    const Util::Config::Node *section = fileConfig.TryGet(g.entry.set);
    for (const MenuSettingDef &def : MenuSettingDefs())
      for (const MenuSettingChoice &choice : def.choices)
        for (const auto &kv : choice.keys)
        {
          const Util::Config::Node *n = section ? section->TryGet(kv.first) : nullptr;
          if (!n)
            n = fileConfig.TryGet(kv.first);
          const bool on = n ? n->ValueAs<bool>() : defaults[kv.first].ValueAsDefault<bool>(false);
          g.base[kv.first] = on ? "1" : "0";
        }
  }
}

static int FindGame(const std::vector<MenuGame> &games, const std::string &nameOrPath)
{
  for (size_t i = 0; i < games.size(); i++)
    if (games[i].entry.set == nameOrPath || games[i].zipPath == nameOrPath)
      return (int)i;
  return -1;
}

/*
 * One game, until L3 is held for 2 s (0), or until it fails to start (1).
 * Everything it makes is gone again when it returns, but the Vulkan context
 * and the pad's handle (PS5Pad.cpp), which are the app's.
 */
static int RunSession(const GameLoader &loader, const MenuGame &choice)
{
  CModel3 *Model3 = nullptr;
  CInputs *Inputs = nullptr;
  Vulkan3D::CVulkan2D *Render2D = nullptr;
  Vulkan3D::CVulkan3D *Render3D = nullptr;
  bool audioOpen = false;
  bool running = false;  // Reset done: the threads run and NVRAM is worth saving
  auto release = [&]()
  {
    if (Model3)
      Model3->PauseThreads();
    if (running)
      SaveNVRAM(Model3);
    if (audioOpen)
      CloseAudio();
    if (s_vkContext && s_vkContext->Device())
      vkDeviceWaitIdle(s_vkContext->Device());
    delete Render2D;
    delete Render3D;
    delete Model3;
    delete Inputs;
  };
  auto fail = [&](const char *what)
  {
    ErrorLog("Session: %s; back to the menu.", what);
    PS5Notify((std::string("Supermodel: ") + choice.entry.title + ": " + what).c_str());
    release();
    return 1;
  };

  // Configuration: defaults, Supermodel.ini's global section, the game's section.
  Util::Config::Node fileConfig("Global");
  if (FileExists(s_configFilePath))
  {
    Util::Config::FromINIFile(&fileConfig, s_configFilePath);
    InfoLog("Config: read '%s'.", s_configFilePath.c_str());
  }
  else
    InfoLog("Config: no '%s'; using the defaults.", s_configFilePath.c_str());

  // The game.
  InfoLog("Game: loading '%s'.", choice.zipPath.c_str());
  Game game;
  ROMSet rom_set;
  if (loader.Load(&game, &rom_set, choice.zipPath))
    return fail("its ROMs could not be loaded (see Supermodel.log)");
  Util::Config::Node defaults = PS5DefaultConfig();
  const PS5GameDefault *gameDefault = FindGameDefault(game);
  if (gameDefault)
    defaults.Set("PowerPCFrequency", gameDefault->ppcMHz);
  Util::Config::Node globalConfig("Global");
  Util::Config::MergeINISections(&globalConfig, defaults, fileConfig);
  const Util::Config::Node *gameSection = fileConfig.TryGet(game.name);
  if (gameSection)
    Util::Config::MergeINISections(&s_runtime_config, globalConfig, *gameSection);
  else
    s_runtime_config = globalConfig;
  InfoLog("Game: %s (%s), stepping %s.", game.title.c_str(), game.name.c_str(), game.stepping.c_str());
  // The menu's settings for the game (MenuSettings.ini), over everything.
  if (const auto *menu = s_menuSettings.Section(game.name))
    for (const auto &kv : *menu)
    {
      s_runtime_config.Set<std::string>(kv.first, kv.second);
      InfoLog("Config: %s = %s (menu settings).", kv.first.c_str(), kv.second.c_str());
    }
  InfoLog("Video: picture %s.", s_runtime_config["WideScreen"].ValueAs<bool>() ? "16:9 (WideScreen, WideBackground as set)" : "4:3");
  ServiceTestForGame(game, fileConfig, gameSection);
  {
    unsigned mhz = s_runtime_config["PowerPCFrequency"].ValueAsDefault<unsigned>(0);
    const char *from;
    std::string builtIn;
    if ((gameSection && gameSection->TryGet("PowerPCFrequency")) || fileConfig.TryGet("PowerPCFrequency"))
      from = "Supermodel.ini";
    else if (gameDefault)
      from = (builtIn = std::string("built in for ") + gameDefault->set).c_str();
    else
      from = "the default";
    if (mhz == 0)
    {
      mhz = game.stepping == "1.0" ? 66 : game.stepping == "1.5" ? 100 : 166;  // as Model3.cpp
      InfoLog("Config: PowerPC at %u MHz (stepping %s's own; %s).", mhz, game.stepping.c_str(), from);
    }
    else
      InfoLog("Config: PowerPC at %u MHz (%s).", mhz, from);
  }

  // The display (the app's).
  const VkExtent2D extent = s_vkContext->Extent();
  unsigned totalXRes = extent.width, totalYRes = extent.height;
  unsigned xOffset, yOffset, xRes, yRes;
  CalcScreenGeometry(totalXRes, totalYRes, !s_runtime_config["Stretch"].ValueAs<bool>(), &xOffset, &yOffset, &xRes, &yRes);
  InfoLog("Video: display %ux%u, game picture %ux%u at %u,%u.", totalXRes, totalYRes, xRes, yRes, xOffset, yOffset);

  // The emulator.
  Model3 = new CModel3(s_runtime_config);
  if (Result::OKAY != Model3->Init())
    return fail("the emulator could not start (Init)");
  if (Model3->LoadGame(game, rom_set) != Result::OKAY)
    return fail("the emulator could not start (LoadGame)");
  rom_set = ROMSet();  // the ROMs are in the emulator now
  LoadNVRAM(Model3);
  ChooseGameBezel(game);

  SetAudioType(game.audio);
  if (Result::OKAY != OpenAudio(s_runtime_config))
    return fail("the sound could not start");
  audioOpen = true;

  // Inputs: the DualSense.
  Inputs = new CInputs(std::make_shared<CPS5InputSystem>());
  if (!Inputs->Initialize())
    return fail("the controls could not start");
  Inputs->LoadFromConfig(s_runtime_config);
  Inputs->ApplyGameSpecificInputRanges(game);
  Model3->AttachInputs(Inputs);

  // Renderers.
  Render2D = new Vulkan3D::CVulkan2D(s_runtime_config, *s_vkContext);
  Render3D = new Vulkan3D::CVulkan3D(s_runtime_config, game.name, *s_vkContext, *Render2D);
  const UpscaleMode upscaleMode = (UpscaleMode)s_runtime_config["UpscaleMode"].ValueAs<int>();
  if (Result::OKAY != Render2D->Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes, 0, upscaleMode))
    return fail("the 2D renderer could not start");
  if (Result::OKAY != Render3D->Init(xOffset, yOffset, xRes, yRes, totalXRes, totalYRes, 0))
    return fail("the 3D renderer could not start");
  Model3->AttachRenderers(Render2D, Render3D, nullptr);

  Model3->Reset();
  running = true;
  InfoLog("Session: running %s.", game.title.c_str());
  PS5Notify((game.title + ": hold L3 for 2 s for the menu").c_str());

  // The loop: one emulated frame per refresh period, as the desktop loop paces it.
  const bool throttle = s_runtime_config["Throttle"].ValueAs<bool>();
  float refreshHz = s_runtime_config["RefreshRate"].ValueAs<float>();
  if (refreshHz <= 0.0f)
    refreshHz = 60.0f;
  const uint64_t frameNs = (uint64_t)(1e9 / refreshHz);
  InfoLog("Session: %.3f Hz pacing%s, display %.3f Hz.", refreshHz, throttle ? "" : " off",
    PS5DisplayRefreshMilliHz() / 1000.0);

  uint64_t nextTime = NowNs() + frameNs;
  const uint64_t sessionStart = NowNs();
  static const char *const diagTags[] = { "15s", "30s", "45s" };
  static const unsigned diagSeconds[] = { 15, 30, 45 };
  // Opt-in (the PNGs cost a few long frames): /data/supermodel/diag-dump.txt.
  unsigned diagNext = FileExists(PS5_APP_DIR "/diag-dump.txt") ? 0 : 3;
  uint64_t statStart = NowNs();
  uint64_t runNs = 0, runMaxNs = 0;
  unsigned frames = 0;
  unsigned long long totalFrames = 0;
  unsigned underrunsAtStat = 0, overrunsAtStat = 0;
  GetAudioBufferStats(&underrunsAtStat, &overrunsAtStat);
  uint64_t l3Since = 0;
  // Analog joystick games (Star Wars Trilogy): L1 turns the left stick round,
  // for the duels; a toast says which way it is.
  const bool stickToggle = (game.inputs & Game::INPUT_ANALOG_JOYSTICK) != 0;
  bool stickInverted = false, l1Before = false;
  CPS5InputSystem::SetLeftStickInverted(false);
  for (;;)
  {
    Inputs->Poll(&game, xOffset, yOffset, xRes, yRes);

    if (stickToggle)
    {
      const bool l1 = (PS5PadLast().buttons & PS5_PAD_L1) != 0;
      if (l1 && !l1Before)
      {
        stickInverted = !stickInverted;
        CPS5InputSystem::SetLeftStickInverted(stickInverted);
        InfoLog("Input: left stick %s (L1).", stickInverted ? "inverted" : "normal again");
        PS5Notify(stickInverted ? "Supermodel: stick inverted (L1)" : "Supermodel: stick normal (L1)");
      }
      l1Before = l1;
    }

    // L3 held for 2 s: back to the menu (Inputs->Poll has just read the pad).
    if (PS5PadLast().buttons & PS5_PAD_L3)
    {
      if (l3Since == 0)
        l3Since = NowNs();
      else if (NowNs() - l3Since >= 2000000000ull)
      {
        InfoLog("Session: L3 held for 2 s; back to the menu.");
        break;
      }
    }
    else
      l3Since = 0;

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
      unsigned underruns = 0, overruns = 0;
      GetAudioBufferStats(&underruns, &overruns);
      InfoLog("ps5: %.2f FPS, RunFrame avg %.2f ms max %.2f ms, audio under-runs %u over-runs %u, frame %llu",
        frames / secs, runNs / 1e6 / frames, runMaxNs / 1e6, underruns - underrunsAtStat, overruns - overrunsAtStat,
        totalFrames);
      // Where the frames' time went: milliseconds per emulated frame for the
      // per-frame stages, the sound board's busy milliseconds per second.
      uint64_t costNs[PS5_COST__COUNT], costCounts[PS5_COST__COUNT];
      PS5CostTake(costNs, costCounts);
      auto perFrame = [&](int slot) { return costNs[slot] / 1e6 / (frames ? frames : 1); };
      InfoLog("ps5 cost (ms/frame): PowerPC %.2f (CPU %.2f), render %.2f (fence+acquire %.2f, submit+present %.2f), "
        "wait for boards %.2f, sync %.2f, drive board %.2f; sound board %.0f ms/s (CPU %.0f), %llu runs",
        perFrame(PS5_COST_PPC), perFrame(PS5_COST_PPC_CPU), perFrame(PS5_COST_RENDER), perFrame(PS5_COST_VIDEO_BEGIN),
        perFrame(PS5_COST_VIDEO_END), perFrame(PS5_COST_WAIT), perFrame(PS5_COST_SYNC), perFrame(PS5_COST_DRIVE),
        costNs[PS5_COST_SOUND] / 1e6 / secs, costNs[PS5_COST_SOUND_CPU] / 1e6 / secs,
        (unsigned long long)costCounts[PS5_COST_SOUND]);
      statStart = now;
      runNs = runMaxNs = 0;
      frames = 0;
      underrunsAtStat = underruns;
      overrunsAtStat = overruns;
    }
  }

  // As the desktop ends a session: threads paused, NVRAM saved, sound closed,
  // then the renderers and the emulator.
  release();
  {
    uint64_t costNs[PS5_COST__COUNT], costCounts[PS5_COST__COUNT];
    PS5CostTake(costNs, costCounts);  // the next session starts from nothing
  }
  InfoLog("Session: %s stopped after %llu frames.", game.title.c_str(), totalFrames);
  return 0;
}

/*
 * The app: the display, the pad and the games once; then the menu and the
 * game chosen in it, in turn, for good.
 */
static int RunApp()
{
  s_gameXMLFilePath = std::string(PS5AppTitleDir()) + "/Config/Games.xml";
  GameLoader loader(s_gameXMLFilePath);

  // The display, for the whole run.
  s_vkContext = new Vulkan3D::CVulkanContext();
  if (Result::OKAY != s_vkContext->InitDevice(PS5DisplayWindow(), false, true, ""))
  {
    ErrorLog("Vulkan: the renderer could not start (see above).");
    PS5Notify("Supermodel: Vulkan could not start (see Supermodel.log)");
    return 1;
  }
  PS5PadRead();  // opens the pad, for the menu and every game

  std::vector<MenuGame> games = ScanGames(loader);
  s_menuSettings.Load(s_menuSettingsPath);
  CPS5Menu menu;
  const bool haveMenu = menu.Init(*s_vkContext);
  if (!haveMenu)
    ErrorLog("Menu: unavailable; the games start one after another instead (see above).");

  int selected = FindGame(games, ReadLine(s_lastGamePath));
  if (selected < 0)
    selected = std::max(0, FindGame(games, s_defaultGame));
  std::string autoboot = ReadLine(s_autobootPath);
  if (!autoboot.empty())
    InfoLog("autoboot.txt names '%s'.", autoboot.c_str());

  for (;;)
  {
    int choice = -1;
    if (!autoboot.empty())
    {
      choice = FindGame(games, autoboot);
      if (choice < 0)
      {
        ErrorLog("autoboot.txt: '%s' is not among the games; the menu instead.", autoboot.c_str());
        PS5Notify((std::string("Supermodel: ") + autoboot + " not found").c_str());
      }
      autoboot.clear();
    }
    if (choice < 0)
    {
      if (haveMenu)
      {
        FillMenuBase(games);
        choice = menu.Run(games, selected, s_menuSettings);
      }
      else
        choice = games.empty() ? -1 : selected;
      if (choice < 0)
      {
        if (!haveMenu)
        {
          ErrorLog("Games: none in %s.", PS5_APP_ROMS);
          PS5Notify("Supermodel: no games in " PS5_APP_ROMS);
          return 1;
        }
        games = ScanGames(loader);
        continue;
      }
    }
    if (haveMenu)
      menu.ReleaseTextures();
    selected = choice;
    if (FILE *f = fopen(s_lastGamePath, "w"))
    {
      fprintf(f, "%s\n", games[choice].entry.set.c_str());
      fclose(f);
    }
    RunSession(loader, games[choice]);
    if (!haveMenu)
      selected = (selected + 1) % (int)games.size();
  }
}

int main(int, char **)
{
  if (!PS5AppInit())
    return 1;
  SetLogger(std::make_shared<CPS5Logger>(CLogger::LogLevel::Info));
  InfoLog("%s on the PS5 (%s).", SUPERMODEL_TITLE_FULL, PS5_APP_TITLE_ID);
  {
    unsigned long long mask = 0;
    const int rc = scePthreadGetaffinity(pthread_self(), &mask);
    InfoLog("Thread: main thread CPU mask 0x%llx (0x%x).", mask, (unsigned)rc);
  }

  int result = 1;
  try
  {
    result = RunApp();
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
