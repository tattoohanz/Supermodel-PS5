/*
 * menu-preview.cpp
 *
 * Draws the PS5 game menu (Src/OSD/PS5/MenuLayout.cpp) on the host into a
 * PNG, with a small software rasteriser standing in for the console's Vulkan
 * backend, so the layout can be checked without a console.
 *
 *   menu-preview <title-dir> <out.png> [selected set] [width height]
 *
 * <title-dir> is a built title (ps5/dist/PPSA97374): its Assets/Fonts and
 * Assets/Menu are used, and its Config/Games.xml is not (the list is the
 * art's sets, with the names given below).
 */

#include "MenuLayout.h"
#include "Util/PNGFile.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <map>
#include <string>
#include <sys/stat.h>
#include <vector>

// The core's logging, to stderr (Util::PNG reports failures through it).
void DebugLog(const char *, ...) {}
void InfoLog(const char *, ...) {}
Result ErrorLog(const char *fmt, ...)
{
  va_list vl;
  va_start(vl, fmt);
  vfprintf(stderr, fmt, vl);
  va_end(vl);
  fputc('\n', stderr);
  return Result::FAIL;
}

struct HostTexture
{
  std::vector<uint8_t> rgba;
  int w = 0, h = 0;
  ImTextureData *data = nullptr;  // ImGui's own (the font atlas)
};

static std::vector<HostTexture> s_textures(1);  // 0 is invalid

static MenuImage Load(const std::string &path)
{
  MenuImage img;
  HostTexture t;
  uint32_t w = 0, h = 0;
  std::string error;
  struct stat st;
  if (stat(path.c_str(), &st) != 0 || !Util::PNG::LoadRGBA(path, t.rgba, w, h, &error))
    return img;
  t.w = (int)w;
  t.h = (int)h;
  s_textures.push_back(std::move(t));
  img.id = (ImTextureID)(s_textures.size() - 1);
  img.width = (float)w;
  img.height = (float)h;
  return img;
}

static void SyncImGuiTextures()
{
  for (ImTextureData *tex : ImGui::GetPlatformIO().Textures)
  {
    if (tex->Status == ImTextureStatus_WantCreate)
    {
      HostTexture t;
      t.data = tex;
      s_textures.push_back(t);
      tex->SetTexID((ImTextureID)(s_textures.size() - 1));
      tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
      tex->SetStatus(ImTextureStatus_OK);
    else if (tex->Status == ImTextureStatus_WantDestroy)
      tex->SetStatus(ImTextureStatus_Destroyed);
  }
}

static void Sample(const HostTexture &t, float u, float v, float out[4])
{
  int w = t.w, h = t.h;
  const uint8_t *pixels = t.rgba.data();
  int bpp = 4;
  if (t.data)
  {
    w = t.data->Width;
    h = t.data->Height;
    pixels = t.data->Pixels;
    bpp = t.data->BytesPerPixel;
  }
  if (!pixels || w <= 0 || h <= 0)
  {
    out[0] = out[1] = out[2] = out[3] = 1.0f;
    return;
  }
  const float x = u * w - 0.5f, y = v * h - 0.5f;
  const int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
  const float fx = x - x0, fy = y - y0;
  float acc[4] = { 0, 0, 0, 0 };
  for (int j = 0; j < 2; j++)
    for (int i = 0; i < 2; i++)
    {
      const int px = std::clamp(x0 + i, 0, w - 1), py = std::clamp(y0 + j, 0, h - 1);
      const float wgt = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
      const uint8_t *p = pixels + ((size_t)py * w + px) * bpp;
      if (bpp == 4)
        for (int k = 0; k < 4; k++)
          acc[k] += wgt * p[k] / 255.0f;
      else
      {
        acc[0] += wgt;
        acc[1] += wgt;
        acc[2] += wgt;
        acc[3] += wgt * p[0] / 255.0f;
      }
    }
  for (int k = 0; k < 4; k++)
    out[k] = acc[k];
}

static void Rasterise(ImDrawData *dd, std::vector<float> &fb, int W, int H)
{
  for (const ImDrawList *list : dd->CmdLists)
  {
    for (const ImDrawCmd &cmd : list->CmdBuffer)
    {
      if (cmd.UserCallback)
        continue;
      const int cx0 = std::max(0, (int)cmd.ClipRect.x), cy0 = std::max(0, (int)cmd.ClipRect.y);
      const int cx1 = std::min(W, (int)std::ceil(cmd.ClipRect.z)), cy1 = std::min(H, (int)std::ceil(cmd.ClipRect.w));
      const size_t id = (size_t)cmd.GetTexID();
      const HostTexture *tex = id < s_textures.size() ? &s_textures[id] : nullptr;
      for (unsigned e = 0; e < cmd.ElemCount; e += 3)
      {
        const ImDrawVert *v[3];
        for (int k = 0; k < 3; k++)
          v[k] = &list->VtxBuffer[cmd.VtxOffset + list->IdxBuffer[cmd.IdxOffset + e + k]];
        const float area = (v[1]->pos.x - v[0]->pos.x) * (v[2]->pos.y - v[0]->pos.y) - (v[2]->pos.x - v[0]->pos.x) * (v[1]->pos.y - v[0]->pos.y);
        if (std::fabs(area) < 1e-6f)
          continue;
        int bx0 = (int)std::floor(std::min({ v[0]->pos.x, v[1]->pos.x, v[2]->pos.x }));
        int by0 = (int)std::floor(std::min({ v[0]->pos.y, v[1]->pos.y, v[2]->pos.y }));
        int bx1 = (int)std::ceil(std::max({ v[0]->pos.x, v[1]->pos.x, v[2]->pos.x }));
        int by1 = (int)std::ceil(std::max({ v[0]->pos.y, v[1]->pos.y, v[2]->pos.y }));
        bx0 = std::max(bx0, cx0); by0 = std::max(by0, cy0); bx1 = std::min(bx1, cx1); by1 = std::min(by1, cy1);
        for (int y = by0; y < by1; y++)
          for (int x = bx0; x < bx1; x++)
          {
            const float px = x + 0.5f, py = y + 0.5f;
            float w0 = ((v[1]->pos.x - px) * (v[2]->pos.y - py) - (v[2]->pos.x - px) * (v[1]->pos.y - py)) / area;
            float w1 = ((v[2]->pos.x - px) * (v[0]->pos.y - py) - (v[0]->pos.x - px) * (v[2]->pos.y - py)) / area;
            float w2 = 1.0f - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0)
              continue;
            float col[4];
            for (int k = 0; k < 4; k++)
            {
              const int shift = k * 8;
              col[k] = (w0 * ((v[0]->col >> shift) & 0xFF) + w1 * ((v[1]->col >> shift) & 0xFF) + w2 * ((v[2]->col >> shift) & 0xFF)) / 255.0f;
            }
            if (tex)
            {
              float t[4];
              Sample(*tex, w0 * v[0]->uv.x + w1 * v[1]->uv.x + w2 * v[2]->uv.x, w0 * v[0]->uv.y + w1 * v[1]->uv.y + w2 * v[2]->uv.y, t);
              for (int k = 0; k < 4; k++)
                col[k] *= t[k];
            }
            float *d = &fb[((size_t)y * W + x) * 3];
            for (int k = 0; k < 3; k++)
              d[k] = d[k] * (1 - col[3]) + col[k] * col[3];
          }
      }
    }
  }
}

int main(int argc, char **argv)
{
  if (argc < 3)
  {
    fprintf(stderr, "usage: %s <title-dir> <out.png> [selected set] [width height]\n", argv[0]);
    return 2;
  }
  const std::string dir = argv[1];
  const std::string selectedSet = argc > 3 ? argv[3] : "srally2";
  const int W = argc > 5 ? atoi(argv[4]) : 1920, H = argc > 5 ? atoi(argv[5]) : 1080;

  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;
  io.DisplaySize = ImVec2((float)W, (float)H);
  io.DeltaTime = 1.0f / 60.0f;
  ImFont *bold = io.Fonts->AddFontFromFileTTF((dir + "/Assets/Fonts/Poppins-Bold.ttf").c_str(), 32.0f);
  ImFont *medium = io.Fonts->AddFontFromFileTTF((dir + "/Assets/Fonts/Poppins-Medium.ttf").c_str(), 32.0f);

  // The games: the sets the box art is there for, with Sega Rally 2's details.
  std::vector<MenuEntry> games;
  {
    DIR *d = opendir((dir + "/Assets/Menu/boxart").c_str());
    std::vector<std::string> sets;
    while (d)
    {
      dirent *e = readdir(d);
      if (!e)
        break;
      std::string name = e->d_name;
      if (name.size() > 4 && name.compare(name.size() - 4, 4, ".png") == 0)
        sets.push_back(name.substr(0, name.size() - 4));
    }
    if (d)
      closedir(d);
    std::sort(sets.begin(), sets.end());
    for (const std::string &set : sets)
    {
      MenuEntry e;
      e.set = set;
      e.title = set == "srally2" ? "Sega Rally 2 (Export)" : set;
      e.manufacturer = "Sega";
      e.year = 1998;
      e.version = "Export";
      e.hardware = "Model 3 Step 2.0";
      e.sound = "DSB2";
      e.marquee = Load(dir + "/Assets/Menu/marquee/" + set + ".png");
      games.push_back(e);
    }
  }
  int selected = 0;
  for (size_t i = 0; i < games.size(); i++)
    if (games[i].set == selectedSet)
      selected = (int)i;

  MenuFrame f;
  f.games = &games;
  f.selected = selected;
  f.scroll = (float)selected;
  f.time = 0.3;
  f.bold = bold;
  f.medium = medium;
  f.headerLogo = Load(dir + "/Assets/Menu/header/logo.png");
  for (const auto &row : std::vector<MenuSettingRow>{ { "Picture", "16:9" }, { "PowerPC", "120 MHz" },
         { "CRT filter", "CRT arcade" }, { "Anti-aliasing", "FXAA" }, { "Bezel", "On" }, { "Glow", "Off" },
         { "Music", "100%" }, { "Sound effects", "100%" } })
    f.settings.push_back(row);
  f.settingsRow = 2;
  f.settingsFocus = getenv("PREVIEW_SETTINGS") != nullptr;
  if (!games.empty())
  {
    f.boxart = Load(dir + "/Assets/Menu/boxart/" + selectedSet + ".png");
    f.backdrop = Load(dir + "/Assets/Menu/backdrop/" + selectedSet + ".png");
    f.logo = Load(dir + "/Assets/Menu/logo/" + selectedSet + ".png");
  }
  else
    f.message = "No games: copy ROM sets (.zip) to /data/supermodel/roms";

  ImDrawData *dd = nullptr;
  for (int pass = 0; pass < 2; pass++)  // the first frame bakes the glyphs
  {
    ImGui::NewFrame();
    MenuDraw(ImGui::GetBackgroundDrawList(), f, ImVec2(0, 0), H / 1080.0f);
    ImGui::Render();
    SyncImGuiTextures();
    dd = ImGui::GetDrawData();
  }
  std::vector<float> fb((size_t)W * H * 3, 0.0f);
  Rasterise(dd, fb, W, H);
  std::vector<uint8_t> rgb((size_t)W * H * 3);
  for (size_t i = 0; i < rgb.size(); i++)
    rgb[i] = (uint8_t)std::clamp((int)(fb[i] * 255.0f + 0.5f), 0, 255);
  if (Util::PNG::WriteRGB(argv[2], rgb.data(), W, H))  // true on failure
  {
    fprintf(stderr, "cannot write %s\n", argv[2]);
    return 1;
  }
  printf("%s: %dx%d, %d games, %d draw lists, %d vertices\n", argv[2], W, H, (int)games.size(), dd->CmdListsCount, dd->TotalVtxCount);
  return 0;
}
