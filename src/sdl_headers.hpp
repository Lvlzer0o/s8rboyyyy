#pragma once

// Support both CMake config packages and system package layouts.
#if __has_include(<SDL.h>)
#include <SDL.h>
#elif __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#error "SDL headers were not found. Install SDL2 development files and ensure they are on the include path."
#endif

#if __has_include(<SDL_ttf.h>)
#include <SDL_ttf.h>
#elif __has_include(<SDL2/SDL_ttf.h>)
#include <SDL2/SDL_ttf.h>
#else
#error "SDL_ttf headers were not found. Install SDL2_ttf development files and ensure they are on the include path."
#endif
