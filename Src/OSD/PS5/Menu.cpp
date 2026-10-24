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
 * Menu.cpp
 *
 * The art is decoded on a thread of its own (Util::PNG) and copied to the
 * GPU through the context's ring buffer in the frame's command buffer, as
 * Vulkan2D uploads the bezel. The marquees stay while the menu is up; the
 * other pictures go once they have not been drawn for a few seconds. All of
 * it is freed before a game starts.
 */

#include "Menu.h"
#include "PS5App.h"
#include "PS5Pad.h"
#include "Supermodel.h"
#include "Graphics/Vulkan3D/VulkanCommon.h"
#include "Graphics/Vulkan3D/VulkanContext.h"
#include "Util/PNGFile.h"
#include "imgui/imgui_impl_vulkan.h"

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace
{
  double Now()
  {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
  }

  bool IsFile(const std::string &path)
  {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
  }

  // Sets drawn with another set's art (the art's names are its own).
  const char *const kArtAliases[][2] = {
    { "bassdx", "bass" }, { "getbass", "bass" }, { "getbassdx", "bass" }, { "getbassur", "bass" },
    { "scud", "scudp" }, { "scudau", "scudp" }, { "scuddx", "scudp" }, { "scuddxo", "scudp" },
    { "scudplus", "scudp" }, { "scudplusa", "scudp" },
    { "daytona2", "dayto2pe" },
    { "mgtrkbad", "magtruck" },
    { "vs215", "vs2" }, { "vs215o", "vs2" }, { "vs298", "vs2" }, { "vs29815", "vs2" }, { "vs2v991", "vs2" },
    { "vs299", "vs2" }, { "vs299a", "vs2" }, { "vs299j", "vs2" }, { "vs29915", "vs2" }, { "vs29915a", "vs2" },
    { "vs29915j", "vs2" },
  };

  struct Decoded
  {
    std::string path;
    std::vector<uint8_t> rgba;
    uint32_t width = 0, height = 0;
    bool ok = false;
  };

  // Decodes PNGs on its own thread, the latest requests first.
  class CLoader
  {
  public:
    CLoader() : m_thread(&CLoader::Main, this) {}

    ~CLoader()
    {
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_quit = true;
      }
      m_cv.notify_all();
      m_thread.join();
    }

    void Request(const std::string &path, bool urgent)
    {
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (urgent)
          m_requests.push_front(path);
        else
          m_requests.push_back(path);
      }
      m_cv.notify_all();
    }

    bool Take(Decoded &out)
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_done.empty())
        return false;
      out = std::move(m_done.front());
      m_done.pop_front();
      return true;
    }

    // Drops what is queued and what is done (what is being decoded still
    // arrives, and is dropped by the caller as no longer wanted).
    void Clear()
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_requests.clear();
      m_done.clear();
    }

  private:
    void Main()
    {
      for (;;)
      {
        std::string path;
        {
          std::unique_lock<std::mutex> lock(m_mutex);
          m_cv.wait(lock, [this] { return m_quit || !m_requests.empty(); });
          if (m_quit)
            return;
          path = m_requests.front();
          m_requests.pop_front();
        }
        Decoded d;
        d.path = path;
        std::string error;
        d.ok = Util::PNG::LoadRGBA(path, d.rgba, d.width, d.height, &error);
        if (!d.ok)
          ErrorLog("Menu: '%s' could not be read (%s).", path.c_str(), error.c_str());
        std::lock_guard<std::mutex> lock(m_mutex);
        m_done.push_back(std::move(d));
      }
    }

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::string> m_requests;
    std::deque<Decoded> m_done;
    bool m_quit = false;
    std::thread m_thread;
  };
}

// "1"/"true"/"yes"/"on" as "1", anything else as "0" (the settings are all
// switches so far).
static std::string Switch(const std::string &value)
{
  std::string v;
  for (char ch : value)
    v += (char)tolower((unsigned char)ch);
  return (v == "1" || v == "true" || v == "yes" || v == "on") ? "1" : "0";
}

// Which of a setting's choices the game has: by the first key of each.
static int ChoiceOf(const MenuSettingDef &def, const MenuGame &game, const CMenuSettings &settings)
{
  const char *key = def.choices[0].keys[0].first;
  std::string value;
  if (const std::string *set = settings.Get(game.entry.set, key))
    value = *set;
  else
  {
    auto it = game.base.find(key);
    if (it != game.base.end())
      value = it->second;
  }
  for (size_t i = 0; i < def.choices.size(); i++)
    if (Switch(def.choices[i].keys[0].second) == Switch(value))
      return (int)i;
  return 0;
}

void MenuFindArt(MenuGame &game)
{
  std::vector<std::string> names;
  auto add = [&names](const std::string &name)
  {
    if (name.empty() || std::find(names.begin(), names.end(), name) != names.end())
      return;
    names.push_back(name);
  };
  auto alias = [](const std::string &name) -> std::string
  {
    for (const auto &a : kArtAliases)
      if (name == a[0])
        return a[1];
    return std::string();
  };
  add(game.entry.set);
  add(game.parent);
  add(alias(game.entry.set));
  add(alias(game.parent));
  const std::string root = std::string(PS5AppTitleDir()) + "/Assets/Menu/";
  auto find = [&](const char *kind) -> std::string
  {
    for (const std::string &name : names)
    {
      const std::string path = root + kind + "/" + name + ".png";
      if (IsFile(path))
        return path;
    }
    return std::string();
  };
  game.marquee = find("marquee");
  game.boxart = find("boxart");
  game.backdrop = find("backdrop");
  game.logo = find("logo");
}

struct CPS5Menu::Impl
{
  struct Texture
  {
    Vulkan3D::AllocatedImage image;
    VkDescriptorSet set = VK_NULL_HANDLE;
    uint32_t width = 0, height = 0;
    enum State { Loading, Ready, Missing } state = Loading;
    bool keep = false;          // a marquee: kept while the menu is up
    uint64_t lastUse = 0;
    double readyAt = 0.0;
  };

  Vulkan3D::CVulkanContext *ctx = nullptr;
  ImGuiContext *imgui = nullptr;
  bool backend = false;
  VkSampler sampler = VK_NULL_HANDLE;
  ImFont *bold = nullptr, *medium = nullptr;
  CLoader loader;
  std::map<std::string, Texture> textures;
  std::deque<Decoded> pendingUploads;
  uint64_t frame = 0;
  std::string headerLogo;

  MenuImage Get(const std::string &path, bool keep, bool urgent, double now)
  {
    MenuImage img;
    if (path.empty())
      return img;
    auto it = textures.find(path);
    if (it == textures.end())
    {
      Texture t;
      t.keep = keep;
      t.lastUse = frame;
      textures.emplace(path, t);
      loader.Request(path, urgent);
      return img;
    }
    Texture &t = it->second;
    t.lastUse = frame;
    if (t.state != Texture::Ready)
      return img;
    img.id = (ImTextureID)(uintptr_t)t.set;
    img.width = (float)t.width;
    img.height = (float)t.height;
    img.alpha = (float)std::min(1.0, (now - t.readyAt) / 0.25);
    return img;
  }

  void Destroy(Texture &t)
  {
    if (t.set != VK_NULL_HANDLE)
      ImGui_ImplVulkan_RemoveTexture(t.set);
    t.set = VK_NULL_HANDLE;
    ctx->Memory().DestroyImage(t.image);
  }

  // Copies what the loader has finished into images, in this frame's commands.
  void Upload(VkCommandBuffer cmd, double now)
  {
    Decoded d;
    while (loader.Take(d))
      pendingUploads.push_back(std::move(d));
    VkDeviceSize budget = 24ull * 1024 * 1024;
    while (!pendingUploads.empty())
    {
      Decoded &p = pendingUploads.front();
      auto it = textures.find(p.path);
      if (it == textures.end() || it->second.state != Texture::Loading)
      {
        pendingUploads.pop_front();  // no longer wanted
        continue;
      }
      Texture &t = it->second;
      if (!p.ok || p.width == 0 || p.height == 0)
      {
        t.state = Texture::Missing;
        pendingUploads.pop_front();
        continue;
      }
      const VkDeviceSize bytes = (VkDeviceSize)p.width * p.height * 4;
      if (bytes > budget)
        break;  // next frame
      VkDeviceSize offset = 0;
      void *pointer = nullptr;
      if (!ctx->Ring().Alloc(bytes, 4, offset, pointer))
        break;
      if (!ctx->Memory().CreateImage(p.width, p.height, 1, VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, t.image))
      {
        ErrorLog("Menu: no memory for a %ux%u picture ('%s').", p.width, p.height, p.path.c_str());
        t.state = Texture::Missing;
        pendingUploads.pop_front();
        continue;
      }
      t.image.view = ctx->CreateView(t.image.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
      memcpy(pointer, p.rgba.data(), (size_t)bytes);
      Vulkan3D::CmdTransition(cmd, t.image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
      VkBufferImageCopy copy = {};
      copy.bufferOffset = offset;
      copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copy.imageSubresource.layerCount = 1;
      copy.imageExtent = { p.width, p.height, 1 };
      vkCmdCopyBufferToImage(cmd, ctx->Ring().Buffer(), t.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
      Vulkan3D::CmdTransition(cmd, t.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
      t.set = ImGui_ImplVulkan_AddTexture(sampler, t.image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      t.width = p.width;
      t.height = p.height;
      t.state = Texture::Ready;
      t.readyAt = now;
      budget -= bytes;
      pendingUploads.pop_front();
    }
  }

  // Pictures not drawn for 3 s (180 frames): long done on the GPU.
  void Evict()
  {
    for (auto it = textures.begin(); it != textures.end();)
    {
      Texture &t = it->second;
      if (!t.keep && t.state != Texture::Loading && frame - t.lastUse > 180)
      {
        Destroy(t);
        it = textures.erase(it);
      }
      else
        ++it;
    }
  }

  void Render(bool *presented)
  {
    VkCommandBuffer cmd = ctx->Cmd();
    Upload(cmd, Now());
    Vulkan3D::CmdTransition(cmd, ctx->SwapImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    VkRenderingAttachmentInfo color = {};
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView = ctx->SwapView();
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.clearValue.color = { { 0, 0, 0, 1 } };
    VkRenderingInfo rendering = {};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent = ctx->Extent();
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color;
    Vulkan3D::CmdBeginRendering(cmd, &rendering);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    Vulkan3D::CmdEndRendering(cmd);
    Vulkan3D::CmdTransition(cmd, ctx->SwapImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);
    ctx->MarkSwapchainReady();
    ctx->EndFrame();
    *presented = true;
  }
};

CPS5Menu::CPS5Menu() : m(new Impl())
{
}

CPS5Menu::~CPS5Menu()
{
  if (m->ctx && m->ctx->Device())
  {
    ReleaseTextures();
    if (m->backend)
      ImGui_ImplVulkan_Shutdown();
    if (m->sampler)
      vkDestroySampler(m->ctx->Device(), m->sampler, nullptr);
  }
  if (m->imgui)
    ImGui::DestroyContext(m->imgui);
  delete m;
}

static void CheckVk(VkResult result)
{
  if (result != VK_SUCCESS)
    ErrorLog("Menu: Vulkan error %d in the ImGui backend.", (int)result);
}

bool CPS5Menu::Init(Vulkan3D::CVulkanContext &context)
{
  m->ctx = &context;
  m->imgui = ImGui::CreateContext();
  ImGui::SetCurrentContext(m->imgui);
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;

  const std::string fonts = std::string(PS5AppTitleDir()) + "/Assets/Fonts/";
  if (IsFile(fonts + "Poppins-Bold.ttf") && IsFile(fonts + "Poppins-Medium.ttf"))
  {
    m->bold = io.Fonts->AddFontFromFileTTF((fonts + "Poppins-Bold.ttf").c_str(), 32.0f);
    m->medium = io.Fonts->AddFontFromFileTTF((fonts + "Poppins-Medium.ttf").c_str(), 32.0f);
  }
  if (!m->bold || !m->medium)
  {
    ErrorLog("Menu: no Poppins fonts in '%s'; ImGui's own instead.", fonts.c_str());
    m->bold = m->medium = io.Fonts->AddFontDefault();
  }

  VkSamplerCreateInfo sampler = {};
  sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler.magFilter = VK_FILTER_LINEAR;
  sampler.minFilter = VK_FILTER_LINEAR;
  sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.maxLod = 0.0f;
  if (vkCreateSampler(context.Device(), &sampler, nullptr, &m->sampler) != VK_SUCCESS)
  {
    ErrorLog("Menu: vkCreateSampler failed.");
    return false;
  }

  ImGui_ImplVulkan_InitInfo init = {};
  init.ApiVersion = VK_API_VERSION_1_3;
  init.Instance = context.Instance();
  init.PhysicalDevice = context.PhysicalDevice();
  init.Device = context.Device();
  init.QueueFamily = context.QueueFamily();
  init.Queue = context.Queue();
  init.DescriptorPoolSize = 256;  // the font atlas and the art (some 60 pictures at most)
  init.MinImageCount = 2;
  init.ImageCount = context.SwapImageCount() > 2 ? context.SwapImageCount() : 2;
  init.UseDynamicRendering = true;
  init.CheckVkResultFn = CheckVk;
  VkFormat format = context.SwapFormat();
  init.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  init.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  init.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &format;
  if (!ImGui_ImplVulkan_Init(&init))
  {
    ErrorLog("Menu: the ImGui Vulkan backend could not start.");
    return false;
  }
  m->backend = true;
  m->headerLogo = std::string(PS5AppTitleDir()) + "/Assets/Menu/header/logo.png";
  if (!IsFile(m->headerLogo))
    m->headerLogo.clear();
  InfoLog("Menu: ready (%ux%u, %u swapchain images).", context.Extent().width, context.Extent().height, init.ImageCount);
  return true;
}

void CPS5Menu::ReleaseTextures()
{
  if (!m->ctx || !m->ctx->Device())
    return;
  vkDeviceWaitIdle(m->ctx->Device());
  for (auto &kv : m->textures)
    m->Destroy(kv.second);
  m->textures.clear();
  m->pendingUploads.clear();
  m->loader.Clear();
}

int CPS5Menu::Run(const std::vector<MenuGame> &games, int selected, CMenuSettings &settings)
{
  const std::vector<MenuSettingDef> &defs = MenuSettingDefs();
  bool settingsFocus = false;
  int settingsRow = 0;
  ImGui::SetCurrentContext(m->imgui);
  ImGuiIO &io = ImGui::GetIO();
  const int n = (int)games.size();
  selected = n ? std::clamp(selected, 0, n - 1) : 0;
  std::vector<MenuEntry> entries;
  for (const MenuGame &g : games)
    entries.push_back(g.entry);
  InfoLog("Menu: shown, %d game(s).", n);

  float scroll = (float)selected;
  bool waitRelease = true;  // what is held from before (L3 out of a game) does nothing
  uint32_t prev = 0;
  double holdStart = 0.0, nextRepeat = 0.0;
  double last = Now();
  const double start = last;
  bool firstPresented = false;
  for (;;)
  {
    const double now = Now();
    const float dt = (float)std::clamp(now - last, 0.001, 0.1);
    last = now;
    m->frame++;

    // Input.
    const PS5PadState pad = PS5PadRead();
    uint32_t b = pad.valid ? pad.buttons : 0;
    if (pad.valid && pad.ly < 48)
      b |= PS5_PAD_UP;
    if (pad.valid && pad.ly > 208)
      b |= PS5_PAD_DOWN;
    if (pad.valid && pad.lx < 48)
      b |= PS5_PAD_LEFT;
    if (pad.valid && pad.lx > 208)
      b |= PS5_PAD_RIGHT;
    if (waitRelease)
    {
      if ((b & (PS5_PAD_UP | PS5_PAD_DOWN | PS5_PAD_LEFT | PS5_PAD_RIGHT | PS5_PAD_L1 | PS5_PAD_R1 | PS5_PAD_CROSS |
            PS5_PAD_OPTIONS | PS5_PAD_L3 | PS5_PAD_TRIANGLE | PS5_PAD_CIRCLE)) == 0)
        waitRelease = false;
      b = 0;
    }
    const uint32_t pressed = b & ~prev;
    prev = b;
    int move = 0;
    for (int dir = -1; dir <= 1; dir += 2)
    {
      const uint32_t bit = dir < 0 ? PS5_PAD_UP : PS5_PAD_DOWN;
      if (pressed & bit)
      {
        move = dir;
        holdStart = now;
        nextRepeat = now + 0.35;
      }
      else if ((b & bit) && now >= nextRepeat)
      {
        move = dir;
        nextRepeat = now + (now - holdStart > 1.5 ? 0.045 : 0.09);
      }
    }
    if (n > 0 && (pressed & PS5_PAD_TRIANGLE) && !defs.empty())
      settingsFocus = !settingsFocus;
    else if (settingsFocus && (pressed & PS5_PAD_CIRCLE))
      settingsFocus = false;
    if (n > 0 && settingsFocus)
    {
      const int rows = (int)defs.size();
      if (move)
        settingsRow = (settingsRow + move + rows) % rows;
      const int change = (pressed & PS5_PAD_LEFT) ? -1 : (pressed & PS5_PAD_RIGHT) ? 1 : 0;
      if (change)
      {
        const MenuSettingDef &def = defs[settingsRow];
        const MenuGame &g = games[selected];
        const int count = (int)def.choices.size();
        const int choice = (ChoiceOf(def, g, settings) + change + count) % count;
        std::string keys;
        for (const auto &kv : def.choices[choice].keys)
        {
          settings.Set(g.entry.set, kv.first, kv.second);
          keys += std::string(keys.empty() ? "" : ", ") + kv.first + "=" + kv.second;
        }
        settings.Save();
        InfoLog("Menu: %s: %s %s (%s).", g.entry.set.c_str(), def.label, def.choices[choice].label, keys.c_str());
      }
    }
    else if (n > 0)
    {
      if (move)
        selected = (selected + move + n) % n;
      if (pressed & PS5_PAD_L1)
        selected = std::max(0, selected - 5);
      if (pressed & PS5_PAD_R1)
        selected = std::min(n - 1, selected + 5);
    }
    if (pressed & (PS5_PAD_CROSS | PS5_PAD_OPTIONS))
    {
      if (n == 0)
      {
        InfoLog("Menu: no games; looking again.");
        return -1;
      }
      InfoLog("Menu: starting %s (%s).", entries[selected].set.c_str(), entries[selected].title.c_str());
      return selected;
    }
    if (std::fabs(selected - scroll) > 4.0f)
      scroll = (float)selected;
    else
      scroll += (selected - scroll) * std::min(1.0f, dt * 14.0f);

    // The pictures: the marquees, the chosen game's, its neighbours' next.
    for (int i = 0; i < n; i++)
      entries[i].marquee = m->Get(games[i].marquee, true, false, now);
    MenuFrame frame;
    frame.games = &entries;
    frame.selected = selected;
    frame.scroll = scroll;
    frame.time = now - start;
    frame.bold = m->bold;
    frame.medium = m->medium;
    frame.headerLogo = m->Get(m->headerLogo, true, true, now);
    frame.settingsFocus = settingsFocus;
    frame.settingsRow = settingsRow;
    if (n > 0)
    {
      const MenuGame &g = games[selected];
      for (const MenuSettingDef &def : defs)
        frame.settings.push_back({ def.label, def.choices[ChoiceOf(def, g, settings)].label });
      frame.boxart = m->Get(g.boxart, false, true, now);
      frame.backdrop = m->Get(g.backdrop, false, true, now);
      frame.logo = m->Get(g.logo, false, true, now);
      for (int d = -1; d <= 1; d += 2)
      {
        const MenuGame &near = games[(selected + d + n) % n];
        m->Get(near.boxart, false, false, now);
        m->Get(near.backdrop, false, false, now);
        m->Get(near.logo, false, false, now);
      }
    }
    else
      frame.message = "No games yet: copy ROM sets (.zip) to /data/supermodel/roms, then press Cross";

    // The picture.
    const VkExtent2D extent = m->ctx->Extent();
    io.DisplaySize = ImVec2((float)extent.width, (float)extent.height);
    io.DeltaTime = dt;
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    MenuDraw(ImGui::GetBackgroundDrawList(), frame, ImVec2(0, 0), extent.height / 1080.0f);
    ImGui::Render();
    if (!m->ctx->BeginFrame())
    {
      usleep(5000);
      continue;
    }
    bool presented = false;
    m->Render(&presented);
    if (presented && !firstPresented)
    {
      firstPresented = true;
      PS5HideSplashOnce("menu frame");
    }
    m->Evict();
  }
}
