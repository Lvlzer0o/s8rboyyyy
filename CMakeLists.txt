cmake_minimum_required(VERSION 3.16)
project(oneshot_skate CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL QUIET)
if (NOT OpenGL_FOUND)
  message(FATAL_ERROR
    "OpenGL development libraries were not found. "
    "On Ubuntu/Debian install: libgl1-mesa-dev libglu1-mesa-dev")
endif()

pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
if (NOT SDL2_FOUND)
  message(FATAL_ERROR
    "SDL2 development package not found. "
    "On Ubuntu/Debian install: libsdl2-dev")
endif()

pkg_check_modules(SDL2_TTF QUIET IMPORTED_TARGET SDL2_ttf)
if (NOT SDL2_TTF_FOUND)
  message(FATAL_ERROR
    "SDL2_ttf development package not found. "
    "On Ubuntu/Debian install: libsdl2-ttf-dev")
endif()

add_executable(oneshot-skate
  src/main.cpp
  src/player.cpp
  src/world.cpp
  src/tricks.cpp
  src/gameplay_state.cpp
  src/agent_orchestrator.cpp
  src/input_controller.cpp
)

target_include_directories(oneshot-skate PRIVATE
  ${OPENGL_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIRS}
  ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_libraries(oneshot-skate PRIVATE
  PkgConfig::SDL2
  PkgConfig::SDL2_TTF
)

if (OpenGL_GL_PREFERENCE STREQUAL "GLVND")
  target_link_libraries(oneshot-skate PRIVATE
    OpenGL::OpenGL
    OpenGL::GLU
  )
else()
  target_link_libraries(oneshot-skate PRIVATE
    OpenGL::GL
    OpenGL::GLU
  )
endif()
