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
 * MenuLayout.cpp
 *
 * Coordinates are those of a 1920x1080 screen; P() scales and places them.
 * The colours were taken from a picture of P-ras MULTI's menu.
 */

#include "MenuLayout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
  // List column and right panel.
  constexpr float kListX = 390.0f;          // centre of the marquees
  constexpr float kListY = 540.0f;          // centre of the chosen one
  constexpr float kItemW = 340.0f;
  constexpr float kItemH = 134.0f;          // 2528x1000 marquees
  constexpr float kSpacing = 160.0f;
  constexpr float kListTop = 150.0f, kListBottom = 930.0f;  // between the arrows
  constexpr float kPanelX0 = 700.0f, kPanelX1 = 1852.0f, kPanelY0 = 98.0f, kPanelY1 = 992.0f;
  constexpr float kMediaX0 = 728.0f, kMediaX1 = 1824.0f, kMediaY0 = 122.0f, kMediaY1 = 690.0f;

  struct Ctx
  {
    ImDrawList *dl;
    ImVec2 origin;
    float s;
    ImVec2 P(float x, float y) const { return ImVec2(origin.x + x * s, origin.y + y * s); }
    float S(float v) const { return v * s; }
  };

  ImU32 C(int r, int g, int b, int a = 255)
  {
    return IM_COL32(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255), std::clamp(a, 0, 255));
  }

  ImU32 Fade(ImU32 col, float alpha)
  {
    const int a = (int)((col >> IM_COL32_A_SHIFT) & 0xFF);
    return (col & ~IM_COL32_A_MASK) | ((ImU32)std::clamp((int)(a * alpha + 0.5f), 0, 255) << IM_COL32_A_SHIFT);
  }

  void GlowLine(const Ctx &c, float y, float x0, float x1)
  {
    const float widths[] = { 16.0f, 9.0f, 5.0f };
    const int alphas[] = { 22, 45, 95 };
    for (int i = 0; i < 3; i++)
      c.dl->AddLine(c.P(x0, y), c.P(x1, y), C(110, 215, 255, alphas[i]), c.S(widths[i]));
    c.dl->AddLine(c.P(x0, y), c.P(x1, y), C(222, 246, 255), c.S(2.4f));
  }

  // Text with a soft dark shadow, its top left at (x, y) in screen units.
  void Text(const Ctx &c, ImFont *font, float size, float x, float y, ImU32 col, const char *text, float shadow = 2.0f)
  {
    if (!font || !text || !*text)
      return;
    if (shadow > 0.0f)
      c.dl->AddText(font, c.S(size), c.P(x + shadow, y + shadow), Fade(C(0, 0, 0, 150), ((col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f), text);
    c.dl->AddText(font, c.S(size), c.P(x, y), col, text);
  }

  // Width of text in screen units.
  float TextWidth(const Ctx &c, ImFont *font, float size, const char *text)
  {
    if (!font || !text)
      return 0.0f;
    return font->CalcTextSizeA(c.S(size), 1e9f, 0.0f, text).x / c.s;
  }

  // The largest size not above `size` at which text fits in `width`, in
  // steps of 2 units: ImGui bakes a font's glyphs once for every size used,
  // so sizes are kept to a few.
  float FitSize(const Ctx &c, ImFont *font, float size, float width, const char *text)
  {
    const float w = TextWidth(c, font, size, text);
    if (w <= width || w <= 0.0f)
      return size;
    const float fit = std::max(size * width / w, size * 0.55f);
    return std::max(2.0f, std::floor(fit * 0.5f) * 2.0f);
  }

  // An image fitted into a box, aspect kept: centred, or against the left.
  void ImageFit(const Ctx &c, const MenuImage &img, float x0, float y0, float x1, float y1, float alpha, float rounding = 0.0f,
    bool left = false)
  {
    if (!img.Valid())
      return;
    const float bw = x1 - x0, bh = y1 - y0;
    const float k = std::min(bw / img.width, bh / img.height);
    const float w = img.width * k, h = img.height * k;
    const float x = left ? x0 : x0 + (bw - w) * 0.5f, y = y0 + (bh - h) * 0.5f;
    const ImU32 tint = C(255, 255, 255, (int)(255 * alpha * img.alpha));
    if (rounding > 0.0f)
      c.dl->AddImageRounded(img.id, c.P(x, y), c.P(x + w, y + h), ImVec2(0, 0), ImVec2(1, 1), tint, c.S(rounding));
    else
      c.dl->AddImage(img.id, c.P(x, y), c.P(x + w, y + h), ImVec2(0, 0), ImVec2(1, 1), tint);
  }

  void Star(const Ctx &c, float cx, float cy, float r, ImU32 col)
  {
    ImVec2 pts[10];
    for (int i = 0; i < 10; i++)
    {
      const float a = -1.5707963f + i * 0.62831853f;
      const float rr = (i & 1) ? r * 0.45f : r;
      pts[i] = c.P(cx + std::cos(a) * rr, cy + std::sin(a) * rr);
    }
    c.dl->AddConcavePolyFilled(pts, 10, col);
  }

  // A gamepad outline, as the reference has before the game's name.
  void Gamepad(const Ctx &c, float x, float y, ImU32 col)
  {
    c.dl->AddRectFilled(c.P(x, y + 6), c.P(x + 64, y + 38), col, c.S(16));
    c.dl->AddCircleFilled(c.P(x + 12, y + 36), c.S(11), col);
    c.dl->AddCircleFilled(c.P(x + 52, y + 36), c.S(11), col);
    const ImU32 hole = C(14, 30, 48, 255);
    c.dl->AddRectFilled(c.P(x + 11, y + 19), c.P(x + 25, y + 23), hole);
    c.dl->AddRectFilled(c.P(x + 16, y + 14), c.P(x + 20, y + 28), hole);
    c.dl->AddCircleFilled(c.P(x + 45, y + 16), c.S(3), hole);
    c.dl->AddCircleFilled(c.P(x + 52, y + 22), c.S(3), hole);
    c.dl->AddCircleFilled(c.P(x + 45, y + 28), c.S(3), hole);
  }

  // A face button: a ring with its symbol.
  void Button(const Ctx &c, float cx, float cy, char symbol)
  {
    c.dl->AddCircleFilled(c.P(cx, cy), c.S(17), C(20, 20, 30, 200));
    c.dl->AddCircle(c.P(cx, cy), c.S(17), C(230, 230, 240), 0, c.S(2.2f));
    const float r = 8.0f;
    switch (symbol)
    {
    case 'x':
      c.dl->AddLine(c.P(cx - r, cy - r), c.P(cx + r, cy + r), C(130, 180, 255), c.S(3.2f));
      c.dl->AddLine(c.P(cx - r, cy + r), c.P(cx + r, cy - r), C(130, 180, 255), c.S(3.2f));
      break;
    case 'o':
      c.dl->AddCircle(c.P(cx, cy), c.S(r), C(255, 120, 140), 0, c.S(3.0f));
      break;
    case '^':
      c.dl->AddTriangle(c.P(cx, cy - r), c.P(cx + r, cy + r * 0.8f), c.P(cx - r, cy + r * 0.8f), C(80, 220, 180), c.S(3.0f));
      break;
    case 'd':  // D-pad up and down
      c.dl->AddTriangleFilled(c.P(cx, cy - 11), c.P(cx + 7, cy - 3), c.P(cx - 7, cy - 3), C(230, 230, 240));
      c.dl->AddTriangleFilled(c.P(cx, cy + 11), c.P(cx - 7, cy + 3), c.P(cx + 7, cy + 3), C(230, 230, 240));
      break;
    }
  }

  void Background(const Ctx &c)
  {
    const ImU32 tl = C(85, 5, 145), mid_t = C(215, 45, 210), tr = C(80, 136, 118);
    const ImU32 bl = C(145, 12, 150), mid_b = C(240, 140, 190), br = C(27, 49, 88);
    c.dl->AddRectFilledMultiColor(c.P(0, 0), c.P(700, 1080), tl, mid_t, mid_b, bl);
    c.dl->AddRectFilledMultiColor(c.P(700, 0), c.P(1920, 1080), mid_t, tr, br, mid_b);
    // The dotted texture over the list's side.
    const ImU32 dot = C(255, 255, 255, 24);
    for (float y = 92.0f; y < 1006.0f; y += 12.0f)
      for (float x = 12.0f; x < 690.0f; x += 12.0f)
        c.dl->AddRectFilled(c.P(x, y), c.P(x + 2.4f, y + 2.4f), dot);
    // Darker bands behind the header and the footer.
    c.dl->AddRectFilledMultiColor(c.P(0, 0), c.P(1920, 92), C(40, 0, 60, 190), C(10, 30, 40, 170), C(10, 30, 40, 0), C(40, 0, 60, 0));
    c.dl->AddRectFilledMultiColor(c.P(0, 1000), c.P(1920, 1080), C(20, 0, 40, 0), C(5, 20, 40, 0), C(5, 20, 40, 190), C(20, 0, 40, 190));
  }

  void Header(const Ctx &c, const MenuFrame &f)
  {
    if (f.headerLogo.Valid())
      ImageFit(c, f.headerLogo, 56, 8, 400, 68, 1.0f, 0.0f, true);
    else
      Text(c, f.bold, 44, 60, 12, C(40, 110, 255), "SEGA");
    const char *title = "Supermodel  \xC3\x97  PS5";  // U+00D7
    const float size = 40.0f;
    Text(c, f.medium, size, 960 - TextWidth(c, f.medium, size, title) * 0.5f, 14, C(222, 240, 255), title);
    if (f.games && !f.games->empty())
    {
      char count[32];
      snprintf(count, sizeof(count), "%d / %d", f.selected + 1, (int)f.games->size());
      const float w = TextWidth(c, f.medium, 28, count);
      c.dl->AddRectFilled(c.P(1856 - w - 26, 16), c.P(1870, 60), C(10, 20, 40, 150), c.S(10));
      c.dl->AddRect(c.P(1856 - w - 26, 16), c.P(1870, 60), C(200, 235, 255, 160), c.S(10), 0, c.S(2));
      Text(c, f.medium, 28, 1857 - w, 20, C(222, 240, 255), count, 0.0f);
    }
    GlowLine(c, 74, 50, 1870);
  }

  void Arrow(const Ctx &c, float cx, float tipY, bool up, double t)
  {
    const float bounce = (float)std::sin(t * 4.0) * 5.0f;
    const float dir = up ? 1.0f : -1.0f;
    const float tip = tipY - bounce * dir;
    const ImVec2 a = c.P(cx, tip), b = c.P(cx - 40, tip + 42 * dir), d = c.P(cx + 40, tip + 42 * dir);
    c.dl->AddTriangleFilled(c.P(cx, tip - 4 * dir), c.P(cx - 46, tip + 45 * dir), c.P(cx + 46, tip + 45 * dir), C(255, 120, 0, 160));
    c.dl->AddTriangleFilled(a, b, d, C(255, 238, 0));
  }

  void List(const Ctx &c, const MenuFrame &f)
  {
    const auto &games = *f.games;
    const int n = (int)games.size();
    c.dl->PushClipRect(c.P(0, kListTop - 8), c.P(700, kListBottom + 8), true);
    const int first = std::max(0, (int)std::floor(f.scroll) - 4);
    const int last = std::min(n - 1, (int)std::ceil(f.scroll) + 4);
    // Far ones first, so the chosen one is drawn over its neighbours.
    std::vector<int> order;
    for (int i = first; i <= last; i++)
      order.push_back(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return std::fabs(a - f.scroll) > std::fabs(b - f.scroll); });
    for (int i : order)
    {
      const float d = (float)i - f.scroll;
      const float near = std::max(0.0f, 1.0f - std::fabs(d));
      const float k = 1.0f + 0.10f * near;
      const float w = kItemW * k, h = kItemH * k;
      const float cy = kListY + d * kSpacing;
      const float x0 = kListX - w * 0.5f, y0 = cy - h * 0.5f, x1 = x0 + w, y1 = y0 + h;
      const int shade = 190 + (int)(65 * near);
      const MenuEntry &e = games[i];
      c.dl->AddRectFilled(c.P(x0 + 6, y0 + 8), c.P(x1 + 6, y1 + 8), C(0, 0, 0, 90), c.S(6));
      if (e.marquee.Valid())
        c.dl->AddImageRounded(e.marquee.id, c.P(x0, y0), c.P(x1, y1), ImVec2(0, 0), ImVec2(1, 1),
          C(shade, shade, shade, (int)(255 * e.marquee.alpha)), c.S(4));
      else
      {
        c.dl->AddRectFilledMultiColor(c.P(x0, y0), c.P(x1, y1), C(30, 30, 70), C(60, 20, 90), C(20, 10, 40), C(10, 20, 50));
        const float size = FitSize(c, f.bold, 34, kItemW - 30, e.title.c_str());
        const float tw = TextWidth(c, f.bold, size, e.title.c_str());
        Text(c, f.bold, size, kListX - tw * 0.5f, cy - size * 0.62f, C(240, 240, 255), e.title.c_str());
      }
      c.dl->AddRect(c.P(x0, y0), c.P(x1, y1), C(255, 255, 255, 70), c.S(4), 0, c.S(1.5f));
      if (i == f.selected)
      {
        const float pulse = 0.65f + 0.35f * (float)std::sin(f.time * 5.0);
        c.dl->AddRect(c.P(x0 - 7, y0 - 7), c.P(x1 + 7, y1 + 7), C(227, 7, 51, (int)(210 * pulse)), c.S(8), 0, c.S(8));
        c.dl->AddRect(c.P(x0 - 2.5f, y0 - 2.5f), c.P(x1 + 2.5f, y1 + 2.5f), C(252, 252, 4), c.S(5), 0, c.S(5.5f));
      }
    }
    c.dl->PopClipRect();
    if (n > 1)
    {
      Arrow(c, kListX, 98, true, f.time);
      Arrow(c, kListX, 982, false, f.time);
    }
  }

  void Settings(const Ctx &c, const MenuFrame &f);

  void Info(const Ctx &c, const MenuFrame &f, float x, float y, const char *label, const std::string &value)
  {
    if (value.empty())
      return;
    Star(c, x + 11, y + 17, 12, C(215, 242, 251));
    char line[256];
    snprintf(line, sizeof(line), "%s :  %s", label, value.c_str());
    Text(c, f.medium, 27, x + 32, y, C(207, 236, 244), line, 1.5f);
  }

  void Panel(const Ctx &c, const MenuFrame &f)
  {
    c.dl->AddRectFilled(c.P(kPanelX0, kPanelY0), c.P(kPanelX1, kPanelY1), C(12, 28, 46, 215), c.S(8));
    c.dl->AddRect(c.P(kPanelX0, kPanelY0), c.P(kPanelX1, kPanelY1), C(80, 124, 173), c.S(8), 0, c.S(3));
    c.dl->AddRect(c.P(kPanelX0 + 5, kPanelY0 + 5), c.P(kPanelX1 - 5, kPanelY1 - 5), C(150, 200, 240, 50), c.S(6), 0, c.S(1.5f));

    // The large frame: the box art, over a blurred copy of itself.
    c.dl->AddRectFilled(c.P(kMediaX0, kMediaY0), c.P(kMediaX1, kMediaY1), C(4, 10, 22), c.S(10));
    if (f.backdrop.Valid())
      c.dl->AddImageRounded(f.backdrop.id, c.P(kMediaX0, kMediaY0), c.P(kMediaX1, kMediaY1), ImVec2(0, 0), ImVec2(1, 1),
        C(255, 255, 255, (int)(255 * f.backdrop.alpha)), c.S(10));
    if (f.message)
    {
      const float size = FitSize(c, f.medium, 34, kMediaX1 - kMediaX0 - 80, f.message);
      const float tw = TextWidth(c, f.medium, size, f.message);
      Text(c, f.medium, size, (kMediaX0 + kMediaX1) * 0.5f - tw * 0.5f, (kMediaY0 + kMediaY1) * 0.5f - size * 0.6f,
        C(222, 240, 255), f.message);
    }
    if (f.boxart.Valid())
    {
      const float bh = 520.0f, bw = bh * f.boxart.width / f.boxart.height;
      const float bx = (kMediaX0 + kMediaX1) * 0.5f - bw * 0.5f, by = kMediaY0 + 24;
      c.dl->AddRectFilled(c.P(bx + 10, by + 14), c.P(bx + bw + 10, by + bh + 14), C(0, 0, 0, (int)(110 * f.boxart.alpha)), c.S(10));
      c.dl->AddImage(f.boxart.id, c.P(bx, by), c.P(bx + bw, by + bh), ImVec2(0, 0), ImVec2(1, 1),
        C(255, 255, 255, (int)(255 * f.boxart.alpha)));
    }
    // The clear logo beside it, on the right.
    if (f.logo.Valid() && f.boxart.Valid())
    {
      const float bw = 520.0f * f.boxart.width / f.boxart.height;
      const float lx0 = (kMediaX0 + kMediaX1) * 0.5f + bw * 0.5f + 30, lx1 = kMediaX1 - 24;
      if (lx1 - lx0 > 120)
        ImageFit(c, f.logo, lx0, kMediaY0 + 140, lx1, kMediaY1 - 140, 1.0f);
    }
    c.dl->AddRect(c.P(kMediaX0 - 3, kMediaY0 - 3), c.P(kMediaX1 + 3, kMediaY1 + 3), C(0, 204, 253, 60), c.S(12), 0, c.S(9));
    c.dl->AddRect(c.P(kMediaX0, kMediaY0), c.P(kMediaX1, kMediaY1), C(0, 204, 253), c.S(10), 0, c.S(4));

    if (!f.games || f.games->empty() || f.selected < 0 || f.selected >= (int)f.games->size())
      return;
    const MenuEntry &e = (*f.games)[f.selected];

    // The name.
    const ImU32 lime = C(209, 234, 55);
    Gamepad(c, 742, 718, lime);
    const float size = FitSize(c, f.bold, 50, kPanelX1 - 40 - 822, e.title.c_str());
    Text(c, f.bold, size, 822, 744 - size * 0.66f, lime, e.title.c_str(), 2.5f);

    // The details on the left, the settings on the right.
    char year[16] = "";
    if (e.year)
      snprintf(year, sizeof(year), "%u", e.year);
    Info(c, f, 760, 792, "Manufacturer", e.manufacturer);
    Info(c, f, 760, 836, "Released", year);
    Info(c, f, 760, 880, "Version", e.version);
    Info(c, f, 760, 924, "Hardware", e.hardware);
    Settings(c, f);
  }

  // The chosen game's settings: a box under the box art, lit while Triangle
  // has the D-pad on it.
  void Settings(const Ctx &c, const MenuFrame &f)
  {
    const float x0 = 1300, x1 = 1830, y0 = 786, y1 = 976;
    const float pulse = 0.6f + 0.4f * (float)std::sin(f.time * 5.0);
    c.dl->AddRectFilled(c.P(x0, y0), c.P(x1, y1), C(6, 16, 30, 200), c.S(8));
    if (f.settingsFocus)
      c.dl->AddRect(c.P(x0 - 3, y0 - 3), c.P(x1 + 3, y1 + 3), C(252, 252, 4, (int)(170 * pulse)), c.S(10), 0, c.S(6));
    c.dl->AddRect(c.P(x0, y0), c.P(x1, y1), f.settingsFocus ? C(252, 252, 4) : C(0, 204, 253, 170), c.S(8), 0, c.S(2.5f));
    Text(c, f.bold, 24, x0 + 20, y0 + 10, C(0, 204, 253), "GAME SETTINGS", 1.5f);
    if (!f.settingsFocus)
    {
      Button(c, x1 - 120, y0 + 26, '^');
      Text(c, f.medium, 22, x1 - 96, y0 + 12, C(200, 220, 235), "Open", 1.0f);
    }
    for (size_t i = 0; i < f.settings.size(); i++)
    {
      const float y = y0 + 52 + (float)i * 44;
      const bool active = f.settingsFocus && (int)i == f.settingsRow;
      if (active)
        c.dl->AddRectFilled(c.P(x0 + 10, y - 4), c.P(x1 - 10, y + 38), C(252, 252, 4, 40), c.S(6));
      Text(c, f.medium, 27, x0 + 24, y, C(207, 236, 244), f.settings[i].label.c_str(), 1.5f);
      const char *value = f.settings[i].value.c_str();
      const float vw = TextWidth(c, f.bold, 27, value);
      const float vx = x1 - 60 - vw;
      Text(c, f.bold, 27, vx, y, active ? C(252, 252, 4) : C(209, 234, 55), value, 1.5f);
      if (active)
      {
        const ImU32 arrow = C(252, 252, 4);
        c.dl->AddTriangleFilled(c.P(vx - 26, y + 18), c.P(vx - 14, y + 9), c.P(vx - 14, y + 27), arrow);
        c.dl->AddTriangleFilled(c.P(x1 - 34, y + 18), c.P(x1 - 46, y + 9), c.P(x1 - 46, y + 27), arrow);
      }
    }
  }

  void Footer(const Ctx &c, const MenuFrame &f)
  {
    GlowLine(c, 1016, 50, 1870);
    struct Hint { char symbol; const char *text; };
    static const Hint list[] = { { 'x', "Start" }, { 'd', "Choose" }, { '^', "Settings" }, { 0, "L1 / R1  Page" },
      { 0, "Hold L3 in a game: back to this menu" } };
    static const Hint settings[] = { { 'x', "Start" }, { 'd', "Choose" }, { 0, "Left / Right  Change" },
      { 'o', "Back" } };
    const Hint *hints = f.settingsFocus ? settings : list;
    const int count = f.settingsFocus ? 4 : 5;
    float widths[5];
    float total = 0.0f;
    for (int i = 0; i < count; i++)
    {
      widths[i] = (hints[i].symbol ? 46.0f : 0.0f) + TextWidth(c, f.medium, 27, hints[i].text);
      total += widths[i] + (i < count - 1 ? 50.0f : 0.0f);
    }
    float x = 960.0f - total * 0.5f;
    for (int i = 0; i < count; i++)
    {
      if (hints[i].symbol)
      {
        Button(c, x + 17, 1048, hints[i].symbol);
        Text(c, f.medium, 27, x + 46, 1031, C(235, 240, 255), hints[i].text);
      }
      else
        Text(c, f.medium, 27, x, 1031, C(235, 240, 255), hints[i].text);
      x += widths[i] + 50.0f;
    }
  }
}

void MenuDraw(ImDrawList *draw, const MenuFrame &frame, ImVec2 origin, float scale)
{
  Ctx c{ draw, origin, scale };
  Background(c);
  Header(c, frame);
  Panel(c, frame);
  if (frame.games && !frame.games->empty())
    List(c, frame);
  Footer(c, frame);
}
