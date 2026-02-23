cmake_minimum_required(VERSION 3.16)
project(oneshot_skate CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)

add_executable(oneshot-skate
  src/main.cpp
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
