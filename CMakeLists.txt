cmake_minimum_required(VERSION 3.16)
project(oneshot_skate CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

function(select_first_target out_var)
  foreach(candidate ${ARGN})
    if (TARGET ${candidate})
      set(${out_var} ${candidate} PARENT_SCOPE)
      return()
    endif()
  endforeach()

  string(JOIN ", " candidates ${ARGN})
  message(FATAL_ERROR "Expected one of these targets to exist, but none were found: ${candidates}")
endfunction()

if (WIN32)
  set(OpenGL_GL_PREFERENCE LEGACY)
else()
  set(OpenGL_GL_PREFERENCE GLVND)
endif()

find_package(OpenGL QUIET)
if (NOT OpenGL_FOUND)
  if (WIN32)
    message(FATAL_ERROR
      "OpenGL development libraries were not found. "
      "Install the Visual Studio Desktop development with C++ workload and the Windows SDK.")
  else()
    message(FATAL_ERROR
      "OpenGL development libraries were not found. "
      "On Ubuntu/Debian install: libgl1-mesa-dev libglu1-mesa-dev")
  endif()
endif()

if (NOT TARGET OpenGL::GLU)
  if (WIN32)
    message(FATAL_ERROR
      "GLU development libraries were not found. "
      "Install the Visual Studio Desktop development with C++ workload and the Windows SDK.")
  else()
    message(FATAL_ERROR
      "GLU development libraries were not found. "
      "On Ubuntu/Debian install: libglu1-mesa-dev")
  endif()
endif()

find_package(SDL2 QUIET CONFIG)
find_package(SDL2_ttf QUIET CONFIG)

if (NOT (TARGET SDL2::SDL2 OR TARGET SDL2::SDL2-static))
  find_package(PkgConfig QUIET)
  if (PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
  endif()
endif()

if (NOT (TARGET SDL2_ttf::SDL2_ttf OR TARGET SDL2_ttf::SDL2_ttf-static))
  if (NOT PkgConfig_FOUND)
    find_package(PkgConfig QUIET)
  endif()
  if (PkgConfig_FOUND)
    pkg_check_modules(SDL2_TTF QUIET IMPORTED_TARGET SDL2_ttf)
  endif()
endif()

if (NOT (TARGET SDL2::SDL2 OR TARGET SDL2::SDL2-static OR TARGET PkgConfig::SDL2))
  message(FATAL_ERROR
    "SDL2 development package not found. "
    "Install SDL2 and make it discoverable with CMAKE_PREFIX_PATH or a CMake toolchain file. "
    "On Ubuntu/Debian install: libsdl2-dev")
endif()

if (NOT (TARGET SDL2_ttf::SDL2_ttf OR TARGET SDL2_ttf::SDL2_ttf-static OR TARGET PkgConfig::SDL2_TTF))
  message(FATAL_ERROR
    "SDL2_ttf development package not found. "
    "Install SDL2_ttf and make it discoverable with CMAKE_PREFIX_PATH or a CMake toolchain file. "
    "On Ubuntu/Debian install: libsdl2-ttf-dev")
endif()

select_first_target(SDL2_LINK_TARGET
  SDL2::SDL2
  SDL2::SDL2-static
  PkgConfig::SDL2)
select_first_target(SDL2_TTF_LINK_TARGET
  SDL2_ttf::SDL2_ttf
  SDL2_ttf::SDL2_ttf-static
  PkgConfig::SDL2_TTF)
select_first_target(OPENGL_LINK_TARGET
  OpenGL::OpenGL
  OpenGL::GL)

add_executable(oneshot-skate
  src/main.cpp
  src/player.cpp
  src/world.cpp
  src/tricks.cpp
  src/gameplay_state.cpp
  src/agent_orchestrator.cpp
  src/input_controller.cpp
)

target_link_libraries(oneshot-skate PRIVATE
  ${SDL2_LINK_TARGET}
  ${SDL2_TTF_LINK_TARGET}
  ${OPENGL_LINK_TARGET}
  OpenGL::GLU
)
