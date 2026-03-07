# OneShot 3D Skate (C++)

Native desktop version of the skateboarding demo using SDL2 + OpenGL.

## Requirements
- C++17 toolchain
- CMake 3.16+
- OpenGL
- SDL2
- SDL2_ttf
- OpenGL-compatible font file (DejaVu Sans or similar, for HUD text)

On Windows, install the Visual Studio Desktop C++ workload plus SDL2 and SDL2_ttf development packages, then expose them to CMake with either `CMAKE_PREFIX_PATH` or a CMake toolchain file.

On Debian/Ubuntu-like systems:

```bash
sudo apt install build-essential cmake libgl1-mesa-dev libglu1-mesa-dev libsdl2-dev libsdl2-ttf-dev
```

On Windows with a package-manager toolchain:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/path/to/your/toolchain.cmake
cmake --build build -j4
```

Or use the project helper script (recommended for cloud/dev containers):

```bash
./scripts/install_linux_deps.sh
```

## Build and Run

```bash
pwsh ./scripts/build.ps1
./build/oneshot-skate
```

To pass a custom toolchain or prefix path through the helper script:

```powershell
pwsh ./scripts/build.ps1 -CMakeArgs @(
  "-DCMAKE_TOOLCHAIN_FILE=C:/path/to/your/toolchain.cmake"
)
```

If CMake or a C++ compiler is missing, `scripts/build.ps1` now prints install guidance and exits early with a clear error.

Manual commands still work:

```bash
cmake -S . -B build
cmake --build build -j4
./build/oneshot-skate
```

Controls:
- `WASD` / Arrows: move and turn
- `Space`: Ollie
- `P`: Pause
- `Enter`: Start, unpause, or continue after game over/win
- `R`: Restart after game over or win
- `Esc`: Exit

Desktop polish features included:
- Menu scene with cinematic spinning intro camera
- Pause overlay with pulsing visual
- In-game HUD with overlay panels and state-specific menu screens
- Multi-agent orchestration panel for OBJ rendering workflow:
  - Visual understanding agent (mesh semantics)
  - Physics agent (colliders and dynamics hints)
  - Rigging agent (skeleton contract)
  - Render execution agent (runtime integration)
  - Blender bridge agent (DCC handoff)
