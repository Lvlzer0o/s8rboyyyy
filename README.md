# OneShot 3D Skate (C++)

Native desktop version of the skateboarding demo using SDL2 + OpenGL.

## Requirements
- C++17 toolchain
- CMake 3.16+
- OpenGL
- SDL2
- SDL2_ttf
- OpenGL-compatible font file (DejaVu Sans or similar, for HUD text)

On Debian/Ubuntu-like systems:

```bash
sudo apt install build-essential cmake libsdl2-dev libsdl2-ttf-dev
```

## Build and Run

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
./oneshot-skate
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
