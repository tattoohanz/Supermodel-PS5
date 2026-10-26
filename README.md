# Supermodel PS5 (unofficial fork)

> **Unofficial fork** of [Igloo-Garage/Supermodel-VK](https://github.com/Igloo-Garage/Supermodel-VK) for the
> **PlayStation 5**, maintained by [tattoohanz](https://github.com/tattoohanz). Not affiliated with or
> endorsed by Igloo-Garage, the Supermodel team, Sega or Sony. Please report problems with this port
> here, not upstream.

Supermodel, the Sega Model 3 arcade emulator, as a PS5 title, **`PPSA97374`**, on Supermodel-VK's
Vulkan renderer (RADV), with its data in **`/data/supermodel/`**. The PS5 code is on the **`ps5`**
branch.

### What this port adds

- **A PS5 front end** (`Src/OSD/PS5`) in place of SDL: Vulkan on VideoOut, sound through
  sceAudioOut, the DualSense and a USB or Bluetooth mouse as input.
- **A game menu** in the style of SEGA ALL.Net P-ras MULTI, with settings per game: picture (4:3,
  16:9, stretched), resolution, PowerPC speed, CRT filter, anti-aliasing, bezel, glow, music and
  sound effect volume, force feedback. Hold L3 for 2 seconds in a game to go back to it.
- **Vulkan3D**: the 3D scene at the display's resolution or 1 to 8 times 496x384 (RenderScale),
  the stencil test for layered polygons (car shadows), translucency in New3D's two layers.
- **Force feedback** of the drive boards on the DualSense's motors.
- **Each game's own bezel**, and per-game defaults (Sega Rally 2's PowerPC at 120 MHz, for full
  speed).

### Install

1. Copy `PPSA97374/` to `/data/homebrew/` (ShadowMountPlus installs it).
2. Add the line `PPSA97374` to `/data/whitelist.txt` and load `helper.elf` with kstuff, as for XPSemu.
3. Copy **your own** Model 3 ROM sets (`.zip`, MAME's names) into `/data/supermodel/roms/`.
4. Start Supermodel and pick a game.

No ROMs, game artwork or bezels of the games are included.

### Folder layout on the PS5

| Path | What it is |
|---|---|
| `/data/supermodel/roms/` | Your ROM sets (`.zip`). |
| `/data/supermodel/Config/Supermodel.ini` | Optional: the desktop build's settings, applied over the defaults (its global section, then the game's). |
| `/data/supermodel/Config/MenuSettings.ini` | The menu's settings per game (written by the menu; they win over `Supermodel.ini`). |
| `/data/supermodel/autoboot.txt` | Optional: its first line, a set name or a full path to a `.zip`, starts that game at once. |
| `/data/supermodel/NVRAM/` | Each game's NVRAM. |
| `/data/supermodel/Supermodel.log` | The log. |

### Artwork

Your own pictures (`.png`), named after the ROM set (`srally2.png`); a clone also finds its
parent's.

| Path | What it is |
|---|---|
| `/data/supermodel/Bezels/` | A game's bezel beside the 4:3 picture (with the Bezel setting on). Without one, Supermodel's own bezel is drawn. |
| `/data/homebrew/PPSA97374/Assets/Menu/marquee/` | The cabinet marquees in the menu's list. |
| `/data/homebrew/PPSA97374/Assets/Menu/boxart/` | The box art in the menu's large frame. |
| `/data/homebrew/PPSA97374/Assets/Menu/backdrop/` | The picture behind the box art. |
| `/data/homebrew/PPSA97374/Assets/Menu/logo/` | The game's logo. |
| `/data/homebrew/PPSA97374/Assets/Menu/header/logo.png` | The menu's own logo at the top. |

`ps5/prepare-menu-art.sh` cuts your originals to the sizes the menu draws them at.

### Controls

**Menu**: Up / Down choose (L1 / R1 five at a time), Cross or Options starts the game, Triangle
opens its settings (Up / Down choose one, Left / Right change it, Circle closes them), Square shows its
controls: a pad with each of the game's controls on the DualSense button that gives it, read from
`Supermodel.ini` and the defaults, so a mapping changed there shows as it is. Press a button to see it
light up; Circle or Square goes back.

| DualSense | Driving games | Gun games (The Ocean Hunter, The Lost World, L.A. Machineguns) |
|---|---|---|
| Left stick | Steering | Aim |
| R2 / L2 | Accelerator (also Square) / brake | Trigger / second trigger (also Cross / Circle) |
| R1 / L1 | Shift up / down | |
| Cross | Neutral | |
| Circle | Handbrake (also D-pad down) | |
| Triangle | View change | |

Star Wars Trilogy: the left stick, and Cross, Circle, Square and Triangle for fire 0, event 0, fire 1
and event 1 (the names in its input test); L1 turns the stick round, for the duels. A mouse also aims the guns and Star Wars Trilogy's stick, with its buttons.

Every game: Options is Start, D-pad up Coin (Create opens the console's capture menu), D-pad left /
right Service / Test (R3 / the touchpad where the D-pad is the game's joystick), L3 held 2 s back to
the menu.

### Building

`ps5/build-title.sh` makes `ps5/dist/PPSA97374/`, linked the way
[PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) links its RADV titles (`PS5_VULKAN` names
that checkout, `~/ps5/PS5_Vulkan` by default). Ship `imports.txt` with `eboot.bin`: it belongs to
that exact build.

### Credits for this port

- **The Supermodel Team** for [Supermodel](https://github.com/trzy/Supermodel), and
  **[Igloo-Garage](https://github.com/Igloo-Garage)** for Supermodel-VK, which this port builds on
  (see [`NOTICE.md`](NOTICE.md)).
- **[mihawk-99](https://github.com/mihawk-99)** for PS5_Vulkan (RADV on the PS5) and its payload
  SDK fork, and **[ps5-payload-dev](https://github.com/ps5-payload-dev/sdk)** for the payload SDK.

### License

GPL-3.0 or later, like Supermodel.

---

## The original Supermodel-VK README

# Supermodel-VK

**A Vulkan-focused fork of [Supermodel](https://github.com/trzy/Supermodel), the open-source Sega
Model 3 arcade emulator.**

## 🚧 Sega Model 3 Arcade Emulator ## released **v0.9.2**!  
This update focuses heavily on refining the UI/launcher experience and introducing a seamless in-game overlay menu, alongside several long-awaited bug fixes.

### 🎮 UI & Launcher Enhancements
* **Integrated Box Art & Cover Display:** Game covers are finally here! The game list now displays box art, making title browsing much more visual and intuitive.
* **Smoother Input Configuration:** Resolved the micro-stutter/lag that occurred whenever navigating to the controls page. Button mapping has also been moved forward for quicker, more ergonomic access.
* **Streamlined UI Text:** Stripped away redundant and cluttered descriptions from the original interface for a cleaner, modern look.
* **Custom Resolution Fix:** Fixed a bug where custom resolution settings failed to take effect.

<div align="center">
  <img src="./Assets/UI_v0.9.2_preview.webp" width="750" alt="UI and Launcher Preview">
  <p><em>▲ New UI layout featuring integrated box art display</em></p>
</div>

### 🖥️ In-Game Experience & Graphical Fixes
* **New In-Game Quick Menu (OSD):** Bring up an in-game menu at any time during gameplay to toggle widescreen mode on the fly, cycle through CRT shaders/filters, and manage save states—all without having to exit back to check.

<div align="center">
  <img src="./Assets/OSD_v0.9.2_preview.webp" width="750" alt="In-Game OSD Menu Preview">
  <p><em>▲ Quick in-game overlay for real-time video, shader, and save state adjustments</em></p>
</div>

* **Visual & Rendering Fixes:**
  * Fixed an issue where an abnormal blue line would occasionally appear along the top edge of the screen in certain titles.
  * Resolved geometry rendering bugs (e.g., the dinosaur boss glitch in *The Lost World: Jurassic Park* where the eyes and flesh separated from the skeleton).

---
## Download and run

No installer. Grab the archive for your platform from the
[latest release](https://github.com/Igloo-Garage/Supermodel-VK/releases/latest):

| Platform | Archive | Start it with |
| --- | --- | --- |
| Windows 10/11 (64-bit) | `Supermodel-VK.v…for.Windows.zip` | double-click `supermodel.exe` |
| Linux (x86-64) | `Supermodel-VK.v…for.Linux.tar.gz` | `./supermodel` |
| macOS 11+ (universal) | `Supermodel-VK.v…for.macOS.tar.gz` | `./supermodel` |

### Windows

1. Extract the `.zip` anywhere you can write to. Keep the folder together — the executable and the
   data folders beside it are the program.
2. Run `supermodel.exe`. The settings window opens directly.
3. Use the path field at the top of the left column (or its **Browse** button) to point at the folder
   holding your ROM set `.zip` files. The archive ships an empty `ROMs/` folder you can use.
4. Double-click a game, or select it and press **Start Game**.

### Before first run

**For Ubuntu / Debian users:**

```bash
sudo apt update
sudo apt install libsdl2-2.0-0 libsdl2-net-2.0-0 libglu1-mesa libgl1 zlib1g
sudo apt install libvulkan1 mesa-vulkan-drivers   # Vulkan renderer; OpenGL works without it
sudo apt install zenity fonts-noto-cjk            # optional: Browse dialogs + Chinese UI text
```

- `zenity` is what the **Browse** buttons call. KDE users can install `kdialog` instead. Without
  either, type the path by hand.
- `fonts-noto-cjk` is only for the Chinese interface; `fonts-wqy-microhei` is a much smaller
  substitute. The English UI works without either.
- `mesa-vulkan-drivers` provides Vulkan for AMD/Intel GPUs. NVIDIA's proprietary driver ships its
  own ICD, so the package is unnecessary there (and harmless if installed).
- Then extract and run:

  ```bash
  tar -xzf Supermodel-VK.*.for.Linux.tar.gz
  cd Supermodel-VK.*
  ./supermodel
  ```

- `Permission denied` → `chmod +x supermodel`.
- `error while loading shared libraries` → `ldd ./supermodel | grep "not found"` names what is
  missing.
- No window over SSH: it needs an X11/Wayland session. If Wayland misbehaves, try
  `SDL_VIDEODRIVER=x11 ./supermodel`.
- Optional diagnostics: `sudo apt install vulkan-tools mesa-utils`, then `vulkaninfo --summary`
  and `glxinfo -B`. If OpenGL fails on a minimal install, add `libgl1-mesa-dri`.
- No sound: make sure PipeWire (`pipewire-pulse`) or PulseAudio is running.
- Linux builds are less battle-tested than Windows; please report issues with your GPU/driver and
  the terminal output. Add `-log-output=Supermodel.log` to write a log file next to the executable.

**For macOS users:**

macOS Gatekeeper fix: because the build isn't Apple-notarized, macOS will block it as an
"unidentified developer." To unblock it:

1. Open **Terminal**.
2. Type `xattr -c -r ` *(make sure there is a space after `-c -r`)*.
3. Drag the unzipped Supermodel-VK folder directly into the Terminal window (it will paste the full
   path automatically).
4. Press **Enter**. You can now launch the emulator normally.

**macOS performance:** Vulkan runs via MoltenVK (translating Vulkan to Metal). On some setups this
overhead can lower frame rates. If you encounter slowdowns, switch the renderer to **OpenGL** in the
settings window — it shares the same emulation core and visual features.

### All platforms

- No game data is included. You need your own Model 3 ROM sets, one `.zip` per game.
- The build is not code-signed. If Windows SmartScreen shows "Windows protected your PC", choose
  **More info → Run anyway**.
- Vulkan is the default renderer and needs a Vulkan 1.3 driver (1.2 with dynamic rendering and
  synchronisation2 also qualifies); otherwise the emulator falls back to OpenGL 4.1 for that
  session and says so in the settings window.
- `Esc` returns to the settings window instead of quitting; `Tab` opens the in-game quick settings.
- Settings, NVRAM, save states and logs stay next to the executable (`Data\` on Windows;
  `Config/`, `NVRAM/`, `Saves/`, `Screenshots/` in the extracted folder on Linux and macOS).
  Moving the executable on its own starts with fresh settings.

---

## What this fork changes

The emulation core is upstream's. The rendering and presentation layers are reworked:

**Vulkan 1.3 backend** — `Src/Graphics/Vulkan3D/`, alongside the original OpenGL renderers. Per-frame
vertex arenas are protected by fences, so long sessions do not leak or develop black patches.

**CRT post-processing** — scanlines, phosphor masks (aperture grille / slot / shadow) and glass glow,
all computed from an integrated beam profile rather than point-sampled. Point sampling beats into
broad light and dark bands at 4K; integrating over each pixel's footprint removes that dependency and
lets any scanline count render smoothly. There is no geometry or curvature option — this does not
attempt to simulate a spherical tube.

![Game Screenshot 1](Docs/images/02game.jpg)

![Game Screenshot 2](Docs/images/03game.jpg)

**Widescreen without stretching** — Widescreen widens the 3D field of view instead of scaling the
picture horizontally, so the extra width is real scenery and the HUD stays anchored to the screen
edges. The image below alternates between native aspect and widescreen; both are the same scene,
car and camera position.

<p align="center">
  <img src="Docs/images/compare-aspect.gif" width="900" alt="Native aspect vs widescreen">
</p>


**Standalone ImGui settings window** — the settings screen is the application shell. It stays alive
for the whole run, so `Esc` returns to it instead of quitting the program, the way PCSX2 and other
emulators behave. Settings that need a fresh session say so; the rest apply immediately.

**Three exclusive sync modes** — exclusive fullscreen at a fixed 57.5242 Hz arcade rate, exclusive
fullscreen where G-SYNC/VRR tracks the emulator rate, and borderless. The statistics panel reports
which one is actually in effect and what the panel is doing, because a mode being selected and a mode
being honoured are different things.

**Decoupled multi-threaded audio** — sample generation is fractional per frame rather than rounded,
the sound board runs on its own thread, and the output buffer adapts. A non-integer ratio between
57.5242 Hz and a 60 Hz display produces a click every few seconds otherwise.

**Integer scaling that reports itself** — the multiple is derived from the panel's real resolution
(`384 × N ≤ panel rows`), never from the resolution preset, and the settings window shows the multiple
actually in effect plus the row count the next step would need. A 2160-row 4K panel reaches 5×
(2480×1920) and no further; 6× needs 2304 rows, which no 16:9 4K panel has.

**Hot-plug gamepads** — on the `sdl` and `sdlgamepad` input systems, a pad attached or detached
mid-game is picked up without restarting. The other backends still enumerate once at start-up.

**Chinese and English interface** — with the input binding table translated in both directions, since
those strings originate in English for the console and the log.

No game data is included or distributed. You need your own ROMs.

---

## Build from source

Only needed if you want to modify the code or build for a platform without a release archive.
Release users can stop after **Download and run** above.

### Fetch the libraries first

The third-party trees are deliberately not committed — they are large and are restored on demand:

```
Libraries\Get_Libs.cmd
```

This pulls SDL2, zlib, Vulkan-Headers and glslang. On Linux or macOS use the equivalent
`git clone` commands in `Libraries/DIR.txt`, or install the distribution packages.

### Windows (Visual Studio)

Open `MSVC\Supermodel.sln` and build **Release x64**, or from a developer command prompt:

```
msbuild MSVC\Supermodel.sln /m /p:Configuration=Release /p:Platform=x64
```

`build.bat` does the same and then assembles a runnable folder under `dist\`.

The project asks for the v145 toolset and falls back to the newest installed one, so Visual Studio
2022 works as well as 2026.

### Windows (MSYS2 / GCC)

After installing MSYS2:

```
pacman -S mingw64/mingw-w64-x86_64-gcc
pacman -S mingw64/mingw-w64-x86_64-make
pacman -S mingw64/mingw-w64-x86_64-SDL2
pacman -S mingw64/mingw-w64-x86_64-SDL2_net
```

Ensure `gcc` and `mingw32-make` are on your path (`/mingw64/bin` in the MSYS2 shell, or
`C:\msys64\mingw64\bin` for a normal command prompt), then:

```
mingw32-make -f Makefiles/Makefile.Win32
```

### Linux

```
sudo apt install build-essential pkg-config libsdl2-dev libsdl2-net-dev libglu1-mesa-dev zlib1g-dev libwebp-dev
sudo apt install libvulkan1 mesa-vulkan-drivers   # runtime for the Vulkan renderer
make -f Makefiles/Makefile.UNIX
```

The binary is written to `bin/supermodel`; run it from the repository root so it picks up `Config/`
and `Assets/`. Vulkan headers are vendored under `Libraries/Vulkan-Headers`, so no Vulkan SDK is
required to build.

### macOS

```
xcode-select --install
```

Install SDL2 and SDL_net from <https://github.com/libsdl-org/SDL/releases> and
<https://github.com/libsdl-org/SDL_net/releases> into `/Library/Frameworks`, then:

```
make -f Makefiles/Makefile.OSX
```

A binary downloaded or built elsewhere needs one-time execution permission: right-click it in
Finder, choose **Open**, and confirm. See
<https://support.apple.com/guide/mac-help/open-a-mac-app-from-an-unidentified-developer-mh40616/mac>.

The macOS release is a universal binary (Apple Silicon and Intel), but it is packaged as a bare
Unix executable rather than a `.app`. Keep `supermodel` in the extracted folder next to
`Frameworks/`, open Terminal there, and run `./supermodel`. MoltenVK is bundled in `Frameworks/`,
so Vulkan 3D works without Homebrew or the Vulkan SDK; a system MoltenVK is also detected if
present. Without either, the emulator falls back to the OpenGL New 3D renderer. The bundled
MoltenVK is Apache-2.0; see `Frameworks/LICENSE-MoltenVK.txt`.

### Shaders

The Vulkan shaders are compiled to SPIR-V by `Scripts/CompileVulkanShaders.cmd` (Windows) or
`Scripts/CompileVulkanShaders.sh`. The generated `.spv.h` headers are committed, so a plain source
build does not need glslang installed; the `.spv` intermediates are not committed.

---

## Requirements

| | |
|---|---|
| OS | Windows 10/11 (64-bit), Linux, or macOS 11+ |
| Vulkan | Driver providing Vulkan 1.3, or Vulkan 1.2 with `VK_KHR_dynamic_rendering` and `VK_KHR_synchronization2` — for the Vulkan 3D renderer |
| OpenGL | Driver providing OpenGL 4.1+ — for the New 3D and Legacy 3D renderers |

Nothing else needs installing on Windows: the C++ runtime, SDL2 and SDL2_net are linked
statically, so the executable has no DLL dependencies beyond `vulkan-1.dll`, which the graphics
driver provides. The Linux build links SDL2, SDL2_net, GL and GLU dynamically, so those runtime
packages must be installed (see the `apt` line above). Vulkan is loaded at runtime and is optional:
without `libvulkan1` and a driver the emulator uses OpenGL.

---

## Version numbering

The version is written down in exactly one place, `Src/Version.h`:

```c
#define SUPERMODEL_VERSION      "0.3a-WIP"     /* upstream core; recorded in save states */
#define SUPERMODEL_FORK_VERSION "0.9.0.3a"
#define SUPERMODEL_FORK_DATE    "20261017"
```

`SUPERMODEL_TITLE_FULL` is assembled from the last two, and `build.bat` reads the same lines through
`Scripts/VersionString.ps1`. `SUPERMODEL_VERSION` is kept separate on purpose: it is stored in save
states and NVRAM, so changing it would invalidate every save a user already has.

---

## Relationship to upstream

**Taken from upstream `v0.3a-20261017-git-b05ef21`:**

- `Src/CPU/PowerPC/ppc_ops.c` — instruction-semantics fixes ported from MAME: `sraw` when
  `rA == rS`, the `subfme` carry flag, undefined behaviour in the `rlw*` rotate amounts, and
  consistent `fctiw` / `fctiwz` NaN handling across x86 and ARM.
- `Src/Graphics/New3D/R3DShaderQuads.h` and `New3D.cpp` — the QuadTriFix. A forced triangle used to
  be stored as a quad with a fabricated fourth corner, which put that corner off the real triangle
  and corrupted depth and texture coordinates across the sliver beside it. The duplicate vertex is
  now passed through and the geometry shader emits an exact triangle instead.

**Not taken:**

- **PRO-1000 machine support.** It touches `Model3`, `Real3D`, `TileGen`, `53C810`, `IEmulator` and
  `Main.cpp`, several of which this fork has modified heavily for Vulkan. It is a piece of work in
  its own right, not a file copy.
- **Upstream `Audio.cpp` / `SCSP.cpp`.** Both have diverged substantially here: this fork's versions
  carry the decoupled audio path, the adaptive output latency and the fractional sample accumulator.
  Taking upstream's would remove them.

The Vulkan renderer needs no equivalent of QuadTriFix. `CopyVertexData` splits a quad into two real
triangles (`0,1,2` and `0,2,3`) and submits them as `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST`, so there
is never a duplicate vertex to begin with. The defect belongs to New 3D's quad-shading approach,
which reconstructs Model 3's quad rasterisation rules in a geometry shader — more faithful to the
hardware, but with degenerate input to guard against.

---

## Documentation

- `Docs/README.txt` — the full upstream manual: every setting, every command-line option
- `Docs/README-VK.txt` — what this fork adds, in English
- `Docs/安装说明.txt` — setup guide in Chinese, including which input system to pick per controller

---

## Known limitations

- **Hot-plug applies to `sdl` and `sdlgamepad` only.** `xinput`, `dinput` and `rawinput` enumerate
  devices once at start-up; attaching a pad mid-game there still needs a session restart.
- **8BitDo pads have button-type triggers.** SDL maps L2/R2 as buttons and synthesises a full-scale
  axis value while held, so binding one to brake or throttle works but is on/off rather than
  analogue. That is the hardware, not a fault in the emulator. An SN30 Pro+ also reports itself as
  "Nintendo Switch Pro Controller" in Switch mode, which is why it must be recognised by that name.
- **The 4×, 6× and 8× resolution presets are unreachable on 16:9 panels.** They need 1536, 2304 and
  3072 rows respectively; 1440-row panels cap at 3× and 2160-row panels jump to 5×. They are kept
  for custom and non-16:9 panels, and the preset list says so.
- **Integer scaling and widescreen interact.** With `IntegerScale` on, the visible picture is shorter
  than the window, so the 3D layer is drawn into a sub-rectangle. The frustum widening accounts for
  that; scanlines do not, because they are counted against the whole frame. The scanline controls
  state both the pitch and the resulting count so the difference is visible rather than surprising.

---

## Community

Emulator questions belong in the [upstream Supermodel Discord](https://discord.gg/XDd7TMMUMz) —
that is where the people who reverse engineered the Model 3 hardware are.

## Licence

GNU General Public License v3 or later, as upstream. See `Docs/LICENSE.txt`.

A note on why there is only one licence file: GPL does not permit additional terms on a derivative
work, and the Vulkan renderer links against the emulation core, so the two form a single work under
that licence. `NOTICE.md` records what is original to this fork, how much of it there is, and the
licences of the third-party components.

Supermodel is the work of The Supermodel Team — Bart Trzynadlowski, Nik Henson, Ville Linde,
Stefano Teso and many contributors. This fork stands on their reverse-engineering of the Model 3
platform; none of it would exist otherwise. `NOTICE.md` has the full credits, the size of this fork's
additions, and how they were built.
