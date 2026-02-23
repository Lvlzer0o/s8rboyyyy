# Repository Guidelines

## Project Structure & Module Organization
This repository is a native desktop skateboarding prototype in C++17. Core gameplay, rendering, input, HUD, and world logic currently live in `src/main.cpp`. Build configuration is in `CMakeLists.txt`. Runtime assets are under `assets/` (`assets/models/` for OBJ data and `assets/textures/` for textures). `build/` and `cmake-build-debug/` are generated directories and should not be treated as source. `index.html` is an earlier web prototype reference, not the primary runtime target.

## Build, Test, and Development Commands
- `cmake -S . -B build`: Configure the project with CMake.
- `cmake --build build -j4`: Compile the executable (`oneshot-skate`).
- `./build/oneshot-skate`: Run the game locally.
- `cmake --build build --clean-first`: Rebuild from a clean build tree when debugging linker or stale-artifact issues.

Use the same command sequence in PR validation so local and review workflows stay aligned.

## Coding Style & Naming Conventions
Use C++17 and keep formatting consistent with existing code:
- 2-space indentation, braces on new lines, and short focused functions.
- `PascalCase` for types (`Player`, `Obstacle`), `camelCase` for functions (`updatePlayer`), and `UPPER_SNAKE_CASE` for constants (`MAX_SPEED`).
- Prefer `constexpr` for gameplay tunables and compile-time constants.
- Keep rendering helpers and gameplay updates grouped by domain to avoid monolithic growth in `main.cpp`.

## Testing Guidelines
There is no formal test framework yet. Every change should pass:
1. Build success (`cmake --build build -j4`).
2. Manual smoke test: launch, move/turn, jump, pause/resume, restart, and verify no regressions in HUD and scoring.
3. Visual changes should be checked in motion, not just at spawn.

## Commit & Pull Request Guidelines
Follow imperative, descriptive commit messages (for example, `Add rail grind scoring` or `Refine procedural board geometry`). Keep commits scoped to one concern. PRs should include:
- What changed and why.
- How it was validated (commands + manual checks).
- Screenshot/GIF when visuals, UI, or animation are affected.

## Security & Configuration Tips
Do not commit local IDE state or generated binaries. Keep asset paths relative (for example, `assets/models/skateboard.obj`) so builds remain portable across machines.
