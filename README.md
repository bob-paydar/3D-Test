# 3D Test v6.4.4
# D3D11 Win32 Sample (Borderless Fullscreen, White FPS)
![screenshot](https://github.com/bob-paydar/3D-Test/blob/main/Screenshot.png)
**Programmer:** Bob Paydar

This is a lightweight Direct3D 11 Windows desktop app built in **Visual Studio 2022**.  
It renders a colorful, glass‑like 3D cube with glowing text on each face, inside an animated galaxy/skybox environment. The FPS is shown **inside the 3D scene** (not in the title bar). Fullscreen is handled via **borderless window** for reliability both under the debugger and when launching the EXE directly.

## Features
- **Borderless Fullscreen** toggle with the **F** key (robust outside VS); **ESC** exits fullscreen.
- **F1** displays a **Help** dialog about fullscreen controls.
- Glassy, colorful cube (about **50%** transparency) with **fresnel reflections** and **refraction**.
- **Glowing text on each cube face (procedurally generated with GDI+ → uploaded to a D3D11 texture).
- **Animated starfield** + **procedural skybox** for a complete background environment.
- **In‑scene FPS** billboard rendered as a textured quad (white text + soft glow).

## Controls
- **F**: Toggle borderless fullscreen on/off.  
- **ESC**: Exit borderless fullscreen (if active).  
- **F1**: Show help dialog for display modes.

## Build (Visual Studio 2022)
1. Create a **Windows Desktop** → **Empty Project** (x64 recommended).  
2. Add `Main.cpp` to the project (from this repo / ZIP).  
3. **C/C++ → Language**: set **`/std:c++17`** (or newer).  
4. **Linker → Input → Additional Dependencies** – add:
   ```
   d3d11.lib; dxgi.lib; d3dcompiler.lib; Gdiplus.lib; user32.lib; gdi32.lib; ole32.lib
   ```
5. Build & Run. The window title is:  
   `3D Test v6.4 — Programmer: Bob Paydar - F1: Help`

> No external assets are required. Textures (for face labels and FPS) are generated at runtime using **GDI+** and uploaded to the GPU.

## Notes on Fullscreen
- This sample deliberately uses **borderless fullscreen** (a popup window covering the monitor) instead of DXGI’s exclusive fullscreen. This avoids cases where exclusive fullscreen is blocked or immediately reverted when you run the **EXE directly** outside Visual Studio.  
- If you prefer exclusive fullscreen, it can be added back (via `IDXGISwapChain::SetFullscreenState`) but beware of driver/OS policy differences.

## Troubleshooting
- **I don’t see FPS in the scene.** v6.4.4 adds a dedicated input layout (`g_inputLayoutBill`) for the FPS quad. Make sure you’re using this version and rebuilt cleanly.
- **Keys work in VS but not in EXE.** This was typically exclusive‑fullscreen related. The current build uses borderless fullscreen so **F/ESC** are consistent both ways.
- **Analyzer hints** like “Macro can be converted to constexpr.” These are suggestions, not errors. The code uses `constexpr` counts where it matters.

## Code Structure (high‑level)
- **Main.cpp**
  - D3D device/swap chain setup (`InitD3D`, `CreateRenderTargets`, `Resize`)
  - HLSL shaders (inline strings): cube VS/PS, fullscreen‑triangle VS/PS, skybox VS/PS, billboard VS/PS
  - Text & FPS texture generation: `CreateTextSRV`, `UpdateFpsTexture`
  - Render helpers: `DrawStarsToRT`, `DrawSkyboxToRT`, `BlitSRVToBackbuffer`, `DrawBillboardFPS`
  - Main loop with per‑frame FPS update
  - Borderless fullscreen helpers: `EnterBorderless`, `ExitBorderless`, `ToggleBorderless`
  - Win32 window proc: F / ESC / F1 handling

## Changelog
- **v6.4.4**
  - Fixed: FPS billboard **not visible** on some systems by creating & binding a dedicated **billboard input layout**.
- **v6.4.3**
  - Fixed a typo (`StringAlignmentAlignmentCenter` → `StringAlignmentCenter`), removed duplicate FPS text draw, replaced `_countof(...)` with `constexpr` where useful.
- **v6.4.2**
  - Title changed to “**3D Test v6.4 — Programmer: Bob Paydar - F1: Help**”.
  - FPS color changed to **white** (white glow + core).
- **v6.4.1**
  - Restored `CreateTextSRV(...)` and resolved `ComPtr` assignment ambiguity with `Attach/Detach`.
- **v6.4**
  - Switched to **borderless fullscreen** for reliable EXE behavior, kept in‑scene FPS, glass cube, and galaxy background.

## License
Public Domain. Feel free to use and modify.

---

**Build tip:** If you see a shader compile message box, it will include the HLSL error from `D3DCompile`. Fix the line indicated and rebuild.
